//
// Created by Fredrik Robertsen on 10/29/23.
//

#ifndef PHELL_HIDEMLIB_H
#define PHELL_HIDEMLIB_H

#include <string>
#include <filesystem>

#include "HiDEMH5Checkpoint.h"
#include "Util/ArgParser.h"
#include "Geometry/GeometryCollection.h"
#include "../PLiB/Accel/CombinedAccel.h"
#include "OutputWriters/VtkParticles.h"
#include <OutputWriters/VtkHdfWriter.h>
#include <OutputWriters/Hdf5Writer.h>
#include <Generators/ParticleGenerator.h>
#include <ExternalForces.h>
#include "OutputWriters/CsvWriter.h"
#include <ParticleDataHandler.h>
#include <LinkDataHandler.h>

#include <Transport/LinkTransporter.h>

#include "DomainBuilder.h"
#include "HiDEMDistribution.h"
#include "HiDEMGeometry.h"
#include "LoggerV2/RangeCollection.h"
#include "Transport/ParticleTransporter.h"

#include <DynamicVariables.h>

#include "ThrustHelper.h"


// no need to include these if they are not used
#if defined(GPU) && defined(ENABLE_SMEM)
#include "../PLiB/Accel/CombinedAccelSmem.h"
#elif defined(ENABLE_SIMD)
#include "../PLiB/Accel/CombinedAccelSIMD.h"
#else
#endif


template <class PDataT, class LDataT>
class HiDEMLib {
public:
    double accumulatedDist = 0;

    std::chrono::steady_clock::time_point runStart;

    // todo fix this better
    // if building CPU and SMEM, non smem handled by default clause
#if defined(GPU) && defined(ENABLE_SMEM)
    CombinedAccelSmem *accel = new CombinedAccelSmem();
#elif defined(ENABLE_SIMD)
    CombinedAccelSIMD *accel = new CombinedAccelSIMD();
#else
    CombinedAccel *accel = new CombinedAccel();
#endif

    ParallelReduce<double> distReducer;
    HiDEMDistribution<PDataT,LDataT> *distr;

    ParallelReduce<lpReal_t> reducer;

    OffloadExec execType = OffloadExec();

    ParticleDataHandler<PDataT> *pDataPtr;
    LinkDataHandler<LDataT> *lDataPtr;

    GlobalLocalMapper<gKey_t, pInd_t> *lMapperPtr;
    GlobalLocalMapper<gKey_t, pInd_t> *pMapperPtr;

    HiDEMGeometry geometry;

    double searchDistanceScale;
    double interactDist;
    double searchDistance;

    double haloScale;
    double reBalanceTolerance;
    bool reBalance;

    int gridRegenInterval;
    int redistributionInterval;
    int firstBalanceTimestep;
    bool dynamicRedistribution;
    bool writeEndCheckpoint;
    bool timestepCheckpoint;
    int checkpointInterval;
    std::string checkpointDirectory;

    HiDEMH5Checkpoint checkpoint;

    bool fromRestart = false;

    bool linkRegenEnabled = false;

    int initBalancePoints;
    double initBalanceTolerance;

    std::optional<std::string> outLinkFile;
    std::optional<std::string> outLinkType;
    std::optional<std::string> outParticleFile;
    std::optional<std::string> outParticleType;
    std::optional<std::string> outWorkingPath;
    std::optional<std::string> outMeshFile;
    std::optional<std::string> outMeshType;

    bool HDF5Compression;
    int HDF5CompressionLevel;
    bool HDF5AsyncIO;

    std::optional<ExternalForces *> extForce;

    int currentTimestep = 0;
    int endTimestep;
    int startTimestep = 0;
    int outputInterval;
    int iterationBlockSize = 10;
    unsigned int maxMinutes;


    DynamicVariable<int> *timeStep;

    ~HiDEMLib() {

        delete accel;
        delete distr;
        //delete lDataPtr;
        //delete pDataPtr;

        /*

        delete lMapperPtr;
        delete pMapperPtr;
        */

        if (extForce) {
            delete extForce.value();
        }
    }

    // -----------------------------
    // get functions

    auto getParticlePtr() {
        return pDataPtr;
    }

    auto getLinkPtr() {
        return lDataPtr;
    }

    auto getExternalForcePtr() {
        if (extForce)
            return extForce.value();
        else {
            Logger::writeAtFirst(
                "External force either not defined in the config file, or incorrectly configured, exiting... \n");
            exit(1);
        }
    }

    InputMesh *getCollisionMeshPtr() {
        if (geometry.geomCollection.getColMeshData())
            return geometry.geomCollection.getColMeshData().value();
        else {
            Logger::writeAtFirst(
                "Collision mesh either not defined in the config file, or incorrectly configured, exiting... \n");
            exit(1);
        }
    }

    InputBed *getBedPtrs() {
        auto bed = geometry.geomCollection.getBed();
        if (bed) {
            return bed.value().get();
        } else {
            Logger::writeAtFirst("Bed either not defined in the config file, or incorrectly configured, exiting... \n");
            exit(1);
        }
    }

    std::tuple<Real3, Real3> getSimLimits() {
        return std::make_tuple(geometry.lowerLimit, geometry.upperLimit);
    }

    template<class T>
    void computeActiveLinks(ManVec<T> &out) {
        RangeV2 r("active link gen", 3);

        T *outPtr = out.data();

        pDataPtr->iterateAllParticles(OffloadExec(), [=] DEVCALLABLE(pInd_t pInd, CombinedAccel &accel, LinkData &lData) {
            int activeLinks = 0;

            accel.lAccel.iterateParticleLinksCT([&](lInd_t lInd, lInd_t sideInd, pInd_t pIndOther) {
                if (lData.state[lInd])
                {
                    activeLinks++;
                }
            }, pInd);

            outPtr[pInd] = activeLinks;
        }, *accel, *lDataPtr);
        Iterator::syncAll();
    }

    template<class T>
    void computeIteratedNeighbours(ManVec<T> &out) {
        RangeV2 r("active particle gen", 3);

        T *outPtr = out.data();

        pDataPtr->iterateAllParticles(OffloadExec(), [=] DEVCALLABLE(pInd_t pInd, CombinedAccel &accel) {
            int iteratedNeighs = 0;

            accel.nList.iteratePossibleNeighbours([&](pInd_t nInd) {
                iteratedNeighs++;
            }, pInd);
            outPtr[pInd] = iteratedNeighs;
        }, *accel);
        Iterator::syncAll();
    }

    // -----------------------------
    // timestep functions
    bool run(){
        int runMins =  std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - runStart).count();

        return currentTimestep < endTimestep && runMins < maxMinutes;
    }

    bool runOutput(){
        if(outputInterval > 0)
            return currentTimestep % outputInterval == 0;
        return false;
    }

    int getCurrentTimestep() {
        return currentTimestep;
    }

    DynamicVariable<int> &getTimeStep() {
        return *timeStep;
    }

    template<class LinkFun, class ... NeighArgs, class ... LinkSideArgs, class ... LinkArgs>
    void linkUpdate(LinkFun breakLinks,
                    std::tuple<NeighArgs...> neighArgs, std::tuple<LinkSideArgs...> linkSideArgs, std::tuple<LinkArgs...> linkArgs) {
        auto range = xvtxRangeStart("link");

        RangeV2 r("link update", 3);

        auto fullArgs = make3Pack(neighArgs, linkSideArgs, linkArgs);

        accel->iterateLinks(execType, *pDataPtr, *lDataPtr, breakLinks, fullArgs);
        //lDataPtr->iterateAllLinks(execType, breakLinks, *lDataPtr, *pDataPtr);
        Iterator::syncAll();
        xvtxRangeEnd(range);
    }

    template<class LinkFun, class... NeighArgs, class... LinkSideArgs, class... LinkArgs>
    void linkUpdateRegen(LinkFun breakLinks,
                         std::tuple<NeighArgs...> neighArgs, std::tuple<LinkSideArgs...> linkSideArgs,
                         std::tuple<LinkArgs...> linkArgs) {
        auto range = xvtxRangeStart("link");
        RangeV2 r("link update regen", 3);
        {
            RangeV2 r("link update", 3);
            auto fullArgs = make3Pack(neighArgs, linkSideArgs, linkArgs);

            accel->iterateLinksRegen(execType, *pDataPtr, *lDataPtr, breakLinks, fullArgs);
            Iterator::syncAll();
        } {
            RangeV2 r("link comm", 3);

            Iterator::syncAll();
            MPI_Barrier(MPI_COMM_WORLD);

            if (distr->linkStateComm.incommingIndexes.size() > 0) {
                distr->linkStateComm.postRecvs();
                distr->linkStateComm.packAllCommData(packVecPtrs(lDataPtr->state));
                distr->linkStateComm.postSends();
                distr->linkStateComm.syncRecvs();
                // unpack by hand here, and if the state changes from broken to intact recreate the link on the remote

                lDataPtr->updateStateInd(distr->linkStateComm.incommingIndexes, distr->linkStateComm.incommingBuffers);

                distr->linkStateComm.syncSends();
            }
        }
        xvtxRangeEnd(range);
    }


    template<class ForceFun, class ... Args>
    void applyForce(ForceFun applyForce, Args ... args) {
    auto range = xvtxRangeStart("apply force");

        RangeV2 r("particle apply force", 2);

        // iterate particles and apply force
        pDataPtr->iterateAllParticles(execType, applyForce, *pDataPtr, args...);

        Iterator::syncAll();
        xvtxRangeEnd(range);
    }

    template<class NFunc, class LFunc, class ... NeighArgs, class ... LinkArgs, class ... LinkSideArgs, class ... ForceArgs>
    void computePartForce(NFunc &neighbourForceFunc, LFunc &linkForceFunc,
                          const std::tuple<NeighArgs...> neighArgs, const std::tuple<LinkArgs...> linkArgs, std::tuple<LinkSideArgs...> linkSideArgs, std::tuple<ForceArgs...> forceArgs
    ) {
        auto range = xvtxRangeStart("force");
        auto fullArgs = make4Pack(neighArgs, linkArgs, linkSideArgs, forceArgs);

        {
            RangeV2 r("post recvs", 3);
            MPI_Barrier(MPI_COMM_WORLD);
            distr->pForceComm.postRecvs();
        }

        {
            auto range = xvtxRangeStart("pre");
            RangeV2 r("force neigh/linked pre", 3);
            accel->iterateCombinedPre(execType, *pDataPtr, *lDataPtr, neighbourForceFunc, linkForceFunc, fullArgs);
            xvtxRangeEnd(range);
        }

        {
            RangeV2 r("particle communication gather", 3);
            // distr->gatherHaloParticles(execType, *pDataPtr);
            distr->pForceComm.packAllCommData(packVecPtrs(
                    pDataPtr->xForce, pDataPtr->yForce, pDataPtr->zForce, pDataPtr->uForce, pDataPtr->vForce,pDataPtr->wForce));
            Iterator::syncAll();
        }

        {
            RangeV2 r("force neigh/linked post", 3);
            auto rangePost = xvtxRangeStart("force neigh/linked post");
            accel->iterateCombinedPost(execType, *pDataPtr, *lDataPtr, neighbourForceFunc, linkForceFunc, fullArgs);
            xvtxRangeEnd(rangePost);
        }

        {
            auto rangePost = xvtxRangeStart("post sends");

            RangeV2 r("post sends", 3);
            distr->pForceComm.postSends();
            xvtxRangeEnd(rangePost);
        }

        {
            auto rangePost = xvtxRangeStart("particle comm wait recv");

            RangeV2 r("particle comm wait recv", 3);
            distr->pForceComm.syncRecvs();
            xvtxRangeEnd(rangePost);
        }

        {
            RangeV2 r("particle communication scatter", 3);
            // distr->scatterHaloParticles(execType, *pDataPtr);
            distr->pForceComm.unpackAllCommData(packVecPtrs(
                    pDataPtr->xForce, pDataPtr->yForce, pDataPtr->zForce, pDataPtr->uForce, pDataPtr->vForce,pDataPtr->wForce));
        }
        {
            auto rangePost = xvtxRangeStart("sync");

            RangeV2 r("sync", 3);
            Iterator::syncAll();
            xvtxRangeEnd(rangePost);

        }
        xvtxRangeEnd(range);
    }

    bool redistBoundsCheck(double searchDistance, PDataT &pData) {
        RangeV2 r("redist bounds check", 3);

        // check if we are out of spec here and if we are set redistriution
        auto [xMax, yMax, zMax, xMin, yMin, zMin] = pData.getLimitsOwned();
        bool needsRedistribute;
        // then check if the maxes are over/under the limits
        double allowedDistance = ((searchDistance * haloScale) - searchDistance);
        auto selfDomain = distr->world.getOwnDomain();
        bool needsRedistLocal = false;

        if (xMax > selfDomain.upper.x + allowedDistance) {
            needsRedistLocal = true;
            // std::cout << "out xMax " << xMax << " " << selfDomain.upper.x + allowedDistance << std::endl;
        }
        if (yMax > selfDomain.upper.y + allowedDistance) {
            needsRedistLocal = true;
            // std::cout << "out yMax " << yMax << " " << selfDomain.upper.y + allowedDistance << std::endl;
        }
        if (zMax > selfDomain.upper.z + allowedDistance) {
            needsRedistLocal = true;
            // std::cout << "out zMax " << zMax << " " << selfDomain.upper.z + allowedDistance << std::endl;
        }
        if (xMin < selfDomain.lower.x - allowedDistance) {
            needsRedistLocal = true;
            // std::cout << "out xMin " << xMin << " " << selfDomain.lower.x - allowedDistance << std::endl;
        }
        if (yMin < selfDomain.lower.y - allowedDistance) {
            needsRedistLocal = true;
            // std::cout << "out yMin " << yMin << " " << selfDomain.lower.y - allowedDistance << std::endl;
        }
        if (zMin < selfDomain.lower.z - allowedDistance) {
            needsRedistLocal = true;
            // std::cout << "out zMin " << zMin << " " << selfDomain.lower.z - allowedDistance << std::endl;
        }
        MPI_Allreduce(&needsRedistLocal, &needsRedistribute, 1, MPI_C_BOOL, MPI_LOR, MPI_COMM_WORLD);

        return needsRedistribute;
    }

    std::tuple<lpReal_t, lpReal_t, lpReal_t> getMaxDistOwned() {
        RangeV2 r("get max dist owned", 3);

        lpReal_t xMax = std::numeric_limits<lpReal_t>::min();
        lpReal_t yMax = std::numeric_limits<lpReal_t>::min();
        lpReal_t zMax = std::numeric_limits<lpReal_t>::min();

        auto *xDiffPtr = pDataPtr->xDiff.data();
        auto *yDiffPtr = pDataPtr->yDiff.data();
        auto *zDiffPtr = pDataPtr->zDiff.data();

        xMax = ParStd::transform_reduce(EXECPOLICY pDataPtr->ownedIndexes.begin(), pDataPtr->ownedIndexes.end(), 0.0,
                                        []DEVCALLABLE (hpReal_t a, hpReal_t b) {
                                            if (a > b)
                                                return a;
                                            return b;
                                        }, [=] DEVCALLABLE (int index) {
                                            return xDiffPtr[index];
                                        });
        yMax = ParStd::transform_reduce(EXECPOLICY pDataPtr->ownedIndexes.begin(), pDataPtr->ownedIndexes.end(), 0.0,
                                        []DEVCALLABLE (hpReal_t a, hpReal_t b) {
                                            if (a > b)
                                                return a;
                                            return b;
                                        }, [=] DEVCALLABLE (int index) {
                                            return yDiffPtr[index];
                                        });
        zMax = ParStd::transform_reduce(EXECPOLICY pDataPtr->ownedIndexes.begin(), pDataPtr->ownedIndexes.end(), 0.0,
                                        []DEVCALLABLE (hpReal_t a, hpReal_t b) {
                                            if (a > b)
                                                return a;
                                            return b;
                                        }, [=] DEVCALLABLE (int index) {
                                            return zDiffPtr[index];
                                        });

        return std::make_tuple(xMax, yMax, zMax);
    }

    void accumulateDist(PDataT &pData, double &accumulatedDist, int stepSize) {
        RangeV2 r("accumilate dist", 3);

        auto [xDistMax, yDistMax, zDistMax] = getMaxDistOwned();

        double currDist = stepSize * sqrt((xDistMax * xDistMax) + (yDistMax * yDistMax) + (zDistMax * zDistMax));
        double currDistGlobal;

        MPI_Allreduce(&currDist, &currDistGlobal, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);

        accumulatedDist += currDistGlobal;
    }


    void finalize() {
        MPI_Barrier(MPI_COMM_WORLD);

        auto outputStart = std::chrono::steady_clock::now();
        if (writeEndCheckpoint) {
            checkpoint.writeCheckpoint(currentTimestep, distr->world.domains[distr->world.self], *pDataPtr, *lDataPtr,
                                       timestepCheckpoint, outWorkingPath.value_or(""), checkpointDirectory);
        }

        auto outputStop = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsedOutput = outputStop - outputStart;

        Logger::writeAtFirst("Output time " + Logger::getLimits(elapsedOutput.count(), "s") + " \n");

        //EventCollection::writeAccumulated();
        //EventCollection::clearHistory();

        MPI_Finalize();
    }


    void finalizeTs() {
        {
            RangeV2 r("particle comm wait send", 2);
            distr->pForceComm.syncSends();
        }

        {
            RangeV2 r("barrier", 2);
            MPI_Barrier(MPI_COMM_WORLD);
        }
        currentTimestep++;
        timeStep->increment();

        if ((currentTimestep % 5000 == 0 && currentTimestep != 0) || currentTimestep == 100 || currentTimestep == 200 || currentTimestep == 1000 || currentTimestep == 2000) {
            Logger::writeAtFirst("timestep done " + std::to_string(currentTimestep) + "\n");
            //EventCollection::writeAccumulated();
            //EventCollection::clearHistory();
            computeActiveLinks(pDataPtr->iteratedLinks);
            computeIteratedNeighbours(pDataPtr->iteratedNeighbours);
            int64_t linksIterated = Iterator::reduceSumIndirect(pDataPtr->iteratedLinks.data(),
                                                                pDataPtr->ownedIndexes.data(),
                                                                pDataPtr->ownedIndexes.size());
            int64_t neighboursIterated = Iterator::reduceSumIndirect(pDataPtr->iteratedNeighbours.data(),
                                                                     pDataPtr->ownedIndexes.data(),
                                                                     pDataPtr->ownedIndexes.size());
            Logger::writeAtFirst("iteratedLinks " + Logger::getStats(linksIterated, "links") + "\n");
            Logger::writeAtFirst("iteratedNegibours " + Logger::getStats(neighboursIterated, "neighbours") + "\n");

            RangeCollection::print(4);
            RangeCollection::zeroHistory();
        }
    }


    void gridService() {
        RangeV2 r("grid service", 2);

        if (currentTimestep % checkpointInterval == 0 && checkpointInterval != -1 && currentTimestep != 0) {
            RangeV2 r("checkpoint IO", 2);
            checkpoint.writeCheckpoint(currentTimestep, distr->world.getOwnDomain(), *pDataPtr, *lDataPtr, timestepCheckpoint,
                                       outWorkingPath.value_or(""),
                                       checkpointDirectory);
        }

        bool needsRedistribute = currentTimestep == startTimestep;
        bool needsGridRegen = currentTimestep == startTimestep;

        // if we do not do auto regen interval
        if (gridRegenInterval != -1) {
            needsGridRegen = (currentTimestep % gridRegenInterval) == 0;
        } else if (currentTimestep % iterationBlockSize == 0) {
            RangeV2 r("dist limit reduce", 2);
            // if we do auto regen interval get the accumulated distance
            accumulateDist(*pDataPtr, accumulatedDist, 10);
            Iterator::syncAll();
            // todo check this and confirm that it is sane
            if (accumulatedDist >= interactDist / 2.0) {
                needsGridRegen = true;
            }
        }

        if (currentTimestep % redistributionInterval == 0 && !dynamicRedistribution) {
            needsRedistribute = true;
        } else if (dynamicRedistribution && needsGridRegen) {
            RangeV2 r("boundCheck", 2);
            needsRedistribute |= redistBoundsCheck(searchDistance, *pDataPtr);
        }

        // run redistrubute the when needed, the first timestep and the specified one
        if (needsRedistribute || currentTimestep == firstBalanceTimestep) {
            RangeV2 r("redistribute", 2);
            Logger::writeAtFirst("redistribute ts: " + std::to_string(currentTimestep) + " \n");
            // Logger::writeAtFirst(fmt::format("redistribute ts: {} \n", ts));
            MPI_Barrier(MPI_COMM_WORLD);
            distr->syncCT(*lDataPtr, *lMapperPtr);

            computeActiveLinks(pDataPtr->iteratedLinks);
            computeIteratedNeighbours(pDataPtr->iteratedNeighbours);

            distr->reDistribute(*pDataPtr, *lDataPtr, *pMapperPtr, *lMapperPtr, searchDistance * haloScale,
                                reBalanceTolerance);
            MPI_Barrier(MPI_COMM_WORLD);
            needsGridRegen = true;
            // needed to not fail the link regen

            accel->generateLinks(*pDataPtr, *lDataPtr, searchDistance, interactDist);
            accel->lAccel.sortSideData(*pDataPtr, *lDataPtr);
        }

        if (needsGridRegen) {
            Logger::writeAtFirst(
                "grid regen ts: " + std::to_string(currentTimestep) + " distance: " + std::to_string(accumulatedDist) +
                " limit: " + std::to_string((searchDistance - interactDist) / 2.0) + " \n", true);

            RangeV2 r("grid generate", 2);

            accumulatedDist = 0;

            // regenerate neighbour list

            accel->generatePrePostCommIndexes(*pDataPtr, *lDataPtr, distr->pForceComm);
            accel->generateNeighbours(*pDataPtr, *lDataPtr, searchDistance, interactDist);

            if(linkRegenEnabled) {
                // regenerate all possible links
                regenerateAllLinks();
                Logger::writeAtFirst("links regenerated \n", true);
            }

            accel->generateLinks(*pDataPtr, *lDataPtr, searchDistance, interactDist);
            accel->lAccel.sortSideData(*pDataPtr, *lDataPtr);
            accel->generateIndexes(*pDataPtr, *lDataPtr, searchDistance, interactDist);

            Logger::writeAtFirst("Grid regen out \n", true);

        }
    }


    // -----------------------------
    // IO functions

    // void snapshotOutput(int ts);
    template<class... Names, class... Fields>
    void snapshotParticles(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields,
                           std::string prefix = "") {
        snapshotParticlesIndex(fieldNames, outFields, pDataPtr->ownedIndexes);
    }

    // void snapshotOutput(int ts);
    template<class... Names, class... Fields, class FilterFun>
    void
    snapshotParticlesFilter(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, FilterFun filter,
                            std::string prefix = "") {
        ManVec<pInd_t> indexes = filterIndexVec(pDataPtr->ownedIndexes, filter, *pDataPtr);

        snapshotParticlesIndex(fieldNames, outFields, indexes);
    }


    template<class... Names, class... Fields>
    void snapshotParticlesIndex(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields,
                                ManVec<pInd_t> &indexes, std::string prefix = "") {
        // add compile time check that both tuples are the same size ?
        RangeV2 r("write particles", 2);

        if (outParticleType && outParticleFile && !outParticleFile.value().empty()) {
            std::string outputStr = outWorkingPath.value_or(".") + "/" + prefix + outParticleFile.value() +
                                    std::to_string(currentTimestep);
            // no these cant do inheritance they rely on templates
            if (outParticleType.value() == "CSV") {
                outputStr = outWorkingPath.value_or(".") + "/" + std::to_string(getLocalRank()) + "_" +
                            prefix + outParticleFile.value() + std::to_string(currentTimestep);
                CsvWriter partIo;
                partIo.createFile(outputStr + ".csv");
                partIo.writerParticlesIndexed(pDataPtr->xCord, pDataPtr->yCord, pDataPtr->zCord, indexes, fieldNames,
                                              vecPtrToDataPtr(outFields));
                partIo.closeFile();
            } else if (outParticleType.value() == "HDF5") {
                Hdf5Writer partIo(HDF5Compression, HDF5CompressionLevel);
                partIo.createFile(outputStr + ".h5");
                partIo.writerParticlesIndexed(pDataPtr->xCord, pDataPtr->yCord, pDataPtr->zCord, indexes, fieldNames,
                                              vecPtrToDataPtr(outFields));
                partIo.closeFile();
            } else if (outParticleType.value() == "VTK") {
                VtkParticles partIo;
                partIo.createFile(outputStr + ".vtk");
                partIo.writerParticlesIndexed(pDataPtr->xCord, pDataPtr->yCord, pDataPtr->zCord, indexes, fieldNames,
                                              vecPtrToDataPtr(outFields));
                partIo.closeFile();
            } else if (outParticleType.value() == "VTKHDF") {
                VtkHdfWriter partIo(HDF5Compression, HDF5CompressionLevel);
                partIo.createFile(outputStr + ".vtkhdf");
                partIo.writerParticlesIndexed(pDataPtr->xCord, pDataPtr->yCord, pDataPtr->zCord, indexes, fieldNames,
                                              vecPtrToDataPtr(outFields));
                partIo.closeFile();
                //
            } else {
                Logger::writeAtFirst("Unsupported particle output type exiting ...");
                exit(1);
            }
        }
    }

    template<class... Names, class... Fields>
    void
    snapshotLinks(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, std::string prefix = "") {
        // add compile time check that both tuples are the same size ?
        snapshotLinksIndex(fieldNames, outFields, lDataPtr->ownedIndexes);
    }

    template<class... Names, class... Fields, class FilterFun>
    void
    snapshotLinksFilter(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, FilterFun filter,
                        std::string prefix = "") {
        // add compile time check that both tuples are the same size ?
        ManVec<pInd_t> indexes = filterIndexVec(lDataPtr->ownedIndexes, filter, *lDataPtr);

        snapshotLinksIndex(fieldNames, outFields, indexes);
    }

    template<class... Names, class... Fields>
    void snapshotLinksIndex(const std::tuple<Names...> &fieldNames, const std::tuple<Fields...> &outFields,
                            ManVec<pInd_t> &indexes, std::string prefix = "") {
        RangeV2 r("write links", 2);
        if (outLinkType && outLinkFile && !outLinkFile.value().empty()) {
            // no these cant do inheritance they rely on templates
            if (outLinkType.value() == "CSV") {
                std::string outputStr = outWorkingPath.value_or(".") + "/" + std::to_string(getLocalRank()) + "_" +
                                        prefix + outLinkFile.value() + std::to_string(currentTimestep);
                CsvWriter linkIo;
                linkIo.createFile(outputStr + ".csv");
                linkIo.writeIndexed(indexes, fieldNames, vecPtrToDataPtr(outFields));
                linkIo.closeFile();
            } else if (outLinkType.value() == "HDF5") {
                std::string outputStr =
                        outWorkingPath.value_or(".") + "/" + prefix + outLinkFile.value() + std::to_string(currentTimestep);
                Hdf5Writer linkIo(HDF5Compression, HDF5CompressionLevel);
                linkIo.createFile(outputStr + ".h5");
                linkIo.writeIndexed(indexes, lDataPtr->numLocal, fieldNames, vecPtrToDataPtr(outFields));
                linkIo.closeFile();
            } else {
                Logger::writeAtFirst("Unsupported link output type exiting ...");
                exit(1);
            }
        }
    }

    template<class... Names, class... Fields>
    void snapshotMesh(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, std::string prefix = "") {
        getCollisionMeshPtr()->reduceForces();
        // add compile time check that both tuples are the same size ?
        if (outMeshType && outMeshFile && !outMeshFile.value().empty()) {
            // no these cant do inheritance they rely on templates
            if (outMeshType.value() == "VTKHDF") {
                std::string outputStr = outWorkingPath.value_or(".") + "/" + prefix + outMeshFile.value() + std::to_string(currentTimestep);
                
                VtkHdfWriter meshIo;
                meshIo.createFile(outputStr + ".vtkhdf");
                meshIo.writeMeshFullSingle(
                    geometry.geomCollection.collisionMeshData->getXVtxPtr(0),
                    geometry.geomCollection.collisionMeshData->getYVtxPtr(0),
                    geometry.geomCollection.collisionMeshData->getZVtxPtr(0),
                    geometry.geomCollection.collisionMeshData->getMeshSize(0),
                    fieldNames, outFields);
                meshIo.closeFile();
            } else {
                Logger::writeAtFirst("Unsupported link output type exiting ...");
                exit(1);
            }
        }
    }


    void setup(ArgParser &parser, double scale) {

        runStart = std::chrono::steady_clock::now();

        timeStep = new DynamicVariable<int>();
        timeStep->set(0);
        //
        endTimestep = parser.getArg<int>("iteration.timeSteps").value_or(10000);
        outputInterval = parser.getArg<int>("iteration.outputInterval").value_or(0);
        maxMinutes = parser.getArg<unsigned int>("iteration.maxMinutes").value_or(-1);

        initBalancePoints = parser.getArg<int>("distribution.initBalancePoints").value_or(10000);
        initBalanceTolerance = parser.getArg<double>("distribution.initBalanceTolerance").value_or(0.001);

        auto tomlTable = parser.table;

        geometry.setup(parser, scale);

        extForce = ExternalForceBuilder::build(parser.table);

        haloScale = parser.getArg<double>("distribution.haloScale").value_or(100.0);
        reBalanceTolerance = parser.getArg<double>("distribution.reBalanceTolerance").value_or(0.00001);
        reBalance = parser.getArg<bool>("distribution.reBalance").value_or(true);

        redistributionInterval = parser.getArg<int>("distribution.redistributionInterval").value_or(100);
        firstBalanceTimestep = parser.getArg<int>("distribution.firstBalanceTimestep").value_or(2000);
        dynamicRedistribution = parser.getArg<bool>("distribution.dynamicRedistribution").value_or(true);
        writeEndCheckpoint = parser.getArg<bool>("restart.writeEndCheckpoint").value_or(false);
        timestepCheckpoint = parser.getArg<bool>("restart.timestepCheckpoint").value_or(false);
        checkpointInterval = parser.getArg<int>("restart.checkpointInterval").value_or(10000);
        checkpointDirectory = parser.getArg<std::string>("restart.checkpointDirectory").value_or("checkpoints");

        HDF5Compression = parser.getArg<bool>("HDF5.compression").value_or(false);
        HDF5CompressionLevel = parser.getArg<int>("HDF5.compressionLevel").value_or(4);
        HDF5AsyncIO = parser.getArg<bool>("HDF5.async").value_or(true);

        searchDistanceScale = parser.getArg<double>("grid.searchDistanceScale").value_or(1.4);
        gridRegenInterval = parser.getArg<int>("grid.gridRegenInterval").value_or(-1);

        linkRegenEnabled = parser.getArg<bool>("linkRegen.enabled").value_or(false);

        lpReal_t maxParticleDist = geometry.maxRadius * 2.0;
        searchDistance = searchDistanceScale * maxParticleDist;
        interactDist = searchDistance - maxParticleDist;

        if (getLocalRank() == 0) {
            std::cout << "searchDistance: " << searchDistance << " set from searchDistanceScale * particleDiameter "
                    << std::endl;
            std::cout << "interactDist: " << interactDist << std::endl;
        }

        std::optional<std::string> fromRestart = parser.getArg<std::string>("restart.fromRestart");

        if (fromRestart) {
            Logger::writeAtFirst("Starting from restart: " + fromRestart.value() + "\n");
            if (!std::filesystem::exists(fromRestart.value())) {
                Logger::writeAtFirst("Restart file does not exist: " + fromRestart.value() + "\n");
                exit(1);
            }
            setFromRestart(fromRestart.value());
        }


        distr = new HiDEMDistribution<PDataT,LDataT>(reBalance, searchDistance);
        distr->lTrans->keepInactive = linkRegenEnabled;

        generateDomains(searchDistance * haloScale, *distr);

        buildData(*distr, searchDistance * haloScale, reBalanceTolerance);

        lMapperPtr = new GlobalLocalMapper<gKey_t, pInd_t>();
        auto &lMapper = *lMapperPtr;

        pMapperPtr = new GlobalLocalMapper<gKey_t, pInd_t>();
        auto &pMapper = *pMapperPtr;

        pDataPtr->remap(pMapper);
        lDataPtr->remap(lMapper);


        outWorkingPath = parser.getArg<std::string>("output.outWorkingPath");

        outLinkFile = parser.getArg<std::string>("output.outLinkFile");
        outLinkType = parser.getArg<std::string>("output.outLinkType");

        outParticleFile = parser.getArg<std::string>("output.outParticleFile");
        outParticleType = parser.getArg<std::string>("output.outParticleType");

        outMeshFile = parser.getArg<std::string>("output.outMeshFile");
        outMeshType = parser.getArg<std::string>("output.outMeshType");

        // set up the accel
        accel->lAccel.keepInactive = linkRegenEnabled;
        accel->generatePrePostCommIndexes(*pDataPtr, *lDataPtr, distr->pForceComm);
        accel->generateNeighbours(*pDataPtr, *lDataPtr, searchDistance, interactDist);
        accel->generateLinks(*pDataPtr, *lDataPtr, searchDistance, interactDist);
        accel->generateIndexes(*pDataPtr, *lDataPtr, searchDistance, interactDist);


        Iterator::syncAll();
    }


    void generateDomains(double haloSize, HiDEMDistribution<PDataT,LDataT> &distr) {
        if (fromRestart) {
            // set up distr from file
            distr.world.domains = checkpoint.readDomains();
        } else {
            Domain bounding;
            bounding.lower = geometry.lowerLimit;
            bounding.upper = geometry.upperLimit;
            DomainBuilder::generateDomainsSampled(geometry.fullGeometry, haloSize, initBalancePoints,
                                                  initBalanceTolerance, bounding,
                                                  distr.world);
        }
    }


    void setFromRestart(std::string path) {
        checkpoint.openCheckpooint(path);
        fromRestart = true;
        startTimestep = checkpoint.readTimestep();
        currentTimestep = checkpoint.readTimestep();
    }

    std::tuple<PDataT *, LDataT *>
    buildData(HiDEMDistribution<PDataT,LDataT> &distr, double haloSize, double reBalanceTolerance) {

        pDataPtr = new ParticleDataHandler<PDataT>();
        lDataPtr = new LinkDataHandler<LDataT>();
        // todo fix betteer at some point
        lDataPtr->keepInactive = linkRegenEnabled;
        GlobalLocalMapper<gKey_t, pInd_t> *pMapperPtr = new GlobalLocalMapper<gKey_t, pInd_t>();
        auto &pMapper = *pMapperPtr;
        auto *pTrans = new ParticleTrasporterV2<PDataT>();

        if (fromRestart) {
            checkpoint.readParticleDataTup(*pDataPtr);
            pDataPtr->remap(pMapper);
            Logger::writeAtFirst("Particles loaded: " + Logger::getStats(pDataPtr->numLocal, "particles") + "\n");

            pTrans->getOutgoingHalo(distr.world, *pDataPtr);
            pTrans->getIncomming(*pDataPtr);
            pDataPtr->remap(pMapper);

            checkpoint.readLinkData(*lDataPtr);
            Logger::writeAtFirst("Links loaded: " + Logger::getStats(lDataPtr->numLocal, "links") + "\n");

        } else {
            // find your actual domain
            Domain ownDomain = distr.world.getOwnDomain();
            ManVec<ElementaryParticle> particles = geometry.generateParticles(ownDomain);
            Logger::writeAtFirst("Particles generated: " + Logger::getStats(particles.size(), "particles") + "\n");

#ifndef SET_LOCAL_ID_64
            if (particles.size() > 2000000000) {
                Logger::writeAtFirst("Generated over or close to int32max particles\n");
                Logger::writeAtFirst("Rebuild the code with -DSET_LOCAL_ID_64=1\n");
                exit(1);
            }
#endif
            pDataPtr->appendInitParticles(particles);


            pTrans->getOutgoingHalo(distr.world, *pDataPtr);
            pTrans->getIncomming(*pDataPtr);

            pDataPtr->remap(pMapper);

            ManVec<ElementaryLink> links;
            links = geometry.generateLinks(pDataPtr);
            Logger::writeAtFirst("Links generated: " + Logger::getStats(links.size(), "links") + "\n");

#ifndef SET_LOCAL_ID_64
            if (links.size() > 2000000000) {
                Logger::writeAtFirst("Generated over or close to int32max links\n");
                Logger::writeAtFirst("Rebuild the code with -DSET_LOCAL_ID_64=1\n");
                exit(1);
            }
#endif

            lDataPtr->appendInitLinks(links);
        }
        lDataPtr->makeLocalPartEnds(pMapper);

        auto *lMapperPtr = new GlobalLocalMapper<gKey_t, pInd_t>();
        auto &lMapper = *lMapperPtr;

        lDataPtr->remap(lMapper);

        // dont sync CT as the neighbours at this stage dont have any
        // distr.reDistribute(*pDataPtr, *lDataPtr, pMapper, lMapper, haloSize, reBalanceTolerance);

        delete pMapperPtr;
        delete lMapperPtr;

        delete pTrans;

        return std::tuple<PDataT *, LDataT *>(pDataPtr, lDataPtr);
    }

    void regenerateAllLinks(){
        RangeV2 r("refreeze", 3);
        {
            distr->syncCT(*lDataPtr, *lMapperPtr);

            RangeV2 r("drop broken non owned links", 3);
            lDataPtr->dropBrokenNonOwnedLinks();
        } {
            RangeV2 r("append links", 3);
            accel->generateLinks(*pDataPtr, *lDataPtr, searchDistance, interactDist);

            auto newLinks = geometry.lGen->regenerateLinks(pDataPtr, lDataPtr, accel);

            // add new links
            int oldCount = lDataPtr->numLocal;
            lDataPtr->appendInitLinks(newLinks);
            // todo fix the partial local end making
            lDataPtr->makeLocalPartEnds(*pMapperPtr, 0);

            generateOwnedNonOwned(lDataPtr->owners, lDataPtr->ownedIndexes, lDataPtr->nonOwnedIndexes, lDataPtr->numLocal);
        }
        {
            RangeV2 r("Make communicator", 3);

            distr->lTrans->getOutgoingHalo(distr->world, *pDataPtr, *lDataPtr);
            distr->lTrans->getIncomming(*lDataPtr);

            lDataPtr->remap(*lMapperPtr);
            // todo make an option to connect a subset only
            lDataPtr->connectParticleLinks(*pMapperPtr);
            distr->lCTComm.initFromTransporter(*distr->lTrans);

            distr->linkStateComm.initFromTransporter(*distr->lTrans);
            distr->linkStateComm.tagStart = 128;
            distr->linkStateComm.setupBuffers();
            distr->linkStateComm.makeLocalIndexes(*lMapperPtr);
        }
    }
};


#endif //PHELL_HIDEMLIB_H
