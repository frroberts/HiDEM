//
// Created by Fredrik Robertsen on 19/06/2021.
//

#ifndef PHELL_LINKDATA_H
#define PHELL_LINKDATA_H

#include <ManVec.h>
#include <Allocator.h>
#include <ExecutionPolicy.h>
#include <Iterator.h>
#include "Logger/Logger.h"
#include "ParticleData.h"
#include "LoggerV2/RangeCollection.h"

class LinkData : public ManagedParent {
public:
    size_t numLocal = 0;

    int allocatedMaxLinks = 13; // todo: not sure this is cleaver, i should fix it better

    size_t numPraticlesAllocated;

    // indexes
    ManVec<lInd_t> ownedIndexes;
    ManVec<lInd_t> nonOwnedIndexes;

    // sized numLocal
    ManVec<lInd_t> particleLinkIds;
    ManVec<int> particleLinkNum;

    ManVec<gKey_t> globalKeyEndA;
    ManVec<gKey_t> globalKeyEndB;

    ManVec<lInd_t> sideIndEndA;
    ManVec<lInd_t> sideIndEndB;

    ManVec<gKey_t> globalKeys;

    ManVec<int> owners;
    ManVec<uint8_t> state; // 0 is inactive everything else is seen as state

    // generated
    ManVec<pInd_t> localPartIndA;
    ManVec<pInd_t> localPartIndB;

    ManVec<int> linksPerParticle;


    LinkData(const LinkData &old_obj) = delete;

    LinkData(LinkData &&) = delete;

    LinkData() = default;

    template<class Policy, typename Fun, class... Args>
    void iterateAllLinks(Policy policy, Fun fun, Args &... args) {
        Iterator::iterate(policy, [=] DEVCALLABLE(lInd_t lInd, LinkData &lData, Args &... args) {
            auto indA = lData.localPartIndA[lInd];
            auto indB = lData.localPartIndB[lInd];
            fun(lInd, indA, indB, args...);
        }, numLocal, *this, args...);
    }

    template<typename Fun, class... Args>
    void iterateOwnedLinks(OffloadExec policy, Fun fun, Args &... args) {
        //
        Iterator::iterateIndirect(policy, [=] DEVCALLABLE(lInd_t lInd, LinkData &lData, Args &... args) {
            auto indA = lData.localPartIndA[lInd];
            auto indB = lData.localPartIndB[lInd];
            fun(lInd, indA, indB, args...);
        }, ownedIndexes, *this, args...);
    }

    template<class Policy, typename Fun, class... Args>
    void iterateOwnedLinks(Policy policy, Fun fun, Args &... args) {
        //
        Iterator::iterateIndirect(policy, [=](lInd_t lInd, LinkData &lData, Args &... args) {
            auto indA = lData.localPartIndA[lInd];
            auto indB = lData.localPartIndB[lInd];
            fun(lInd, indA, indB, args...);
        }, ownedIndexes, *this, args...);
        /*
    for(auto const &lInd: ownedIndexes)
    {
        auto indA = localPartIndA[lInd];
        auto indB = localPartIndB[lInd];
        fun(lInd, indA, indB,  args...);
    }*/
    }

    template<typename Fun, class... Args>
    DEVCALLABLE
    void iterateParticleLinks(Fun fun, pInd_t pInd, Args &... args) {
        //
        for (size_t i = 0; i < linksPerParticle[pInd]; ++i) {
            lInd_t lInd = getPartLinkId(i, pInd);
            pInd_t pIndOther = localPartIndA[lInd];
            if (pIndOther == pInd)
                pIndOther = localPartIndB[lInd];
            fun(lInd, pIndOther, args...);
        }
    }


    template<typename Fun, class... Args>
    DEVCALLABLE
    void iterateParticleLinksSideInd(Fun fun, pInd_t pInd, Args &... args) {
        //
        int particleLinkCount = linksPerParticle[pInd];
        for (size_t i = 0; i < particleLinkCount; ++i) {
            lInd_t lInd = getPartLinkId(i, pInd);
            // should this not be localPartIndB, i.e the other particle?
            pInd_t pIndOther = localPartIndB[lInd];
            lInd_t sideInd;
            if (pIndOther == pInd) {
                pIndOther = localPartIndA[lInd];
                sideInd = sideIndEndB[lInd];
            } else {
                sideInd = sideIndEndA[lInd];
            }
            fun(lInd, sideInd, pIndOther, args...);
        }
    }

    void allocPartLinkIdArr(int numLinks, int numParticles) {
        //
        numPraticlesAllocated = numParticles;
        particleLinkIds.clear();
        particleLinkIds.resize(numParticles * numLinks);
    }

    DEVCALLABLE
    lInd_t getPartLinkId(int link, pInd_t pInd) {
        return particleLinkIds[pInd + (link * numPraticlesAllocated)];
        // return particleLinkIds[link +  (pInd * maxLinks)];
    }

    DEVCALLABLE
    void setPartLinkId(int link, pInd_t pInd, lInd_t val) {
        particleLinkIds[pInd + (link * numPraticlesAllocated)] = val;
        // particleLinkIds[link +  (pInd * maxLinks)] = val;
    }

    void buildLinkIterationTable(int numParticles) {
        RangeV2 r("build link iteration table", 3);

        allocPartLinkIdArr(allocatedMaxLinks, numParticles);

        /*
        for (int i = 0; i < maxLinks; ++i) {
            particleLinkIds[i].clear();
            particleLinkIds[i].resize(numParticles);
        }
         */

        linksPerParticle.clear();
        linksPerParticle.resize(numParticles, false);

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(pInd_t i, LinkData &lData) {
            lData.linksPerParticle[i] = 0;
        }, numParticles, *this);

        Iterator::syncAll();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(lInd_t i, LinkData &lData) {
            if (lData.state[i] == 0)
                return;
            pInd_t pIndA = lData.localPartIndA[i];
            pInd_t pIndB = lData.localPartIndB[i];

            // check both particles are actually found
            lInd_t lPIndA;
            lInd_t lPIndB;
            lPIndA = Iterator::atomicIncVal(lData.linksPerParticle[pIndA]);
            lPIndB = Iterator::atomicIncVal(lData.linksPerParticle[pIndB]);

            if (lPIndA >= lData.allocatedMaxLinks || lPIndB >= lData.allocatedMaxLinks) {
                printf("adding more than %d links", lData.allocatedMaxLinks);
#ifdef SET_LOCAL_ID_64
                printf(" %lli %lli ", pIndA, lPIndA);
                printf(" %lli %lli ", pIndB, lPIndB);
#else
                printf(" %i %i ", pIndA, lPIndA);
                printf(" %i %i ", pIndB, lPIndB);
#endif
                // raise(SIGABRT);
            }
            lData.setPartLinkId(lPIndA, pIndA, i);
            lData.setPartLinkId(lPIndB, pIndB, i);
        }, numLocal, *this);

        Iterator::syncAll();


        // todo should do a segmented sort of the particleLinkIds after this to get repetable results
        // except its not that easy since we are sorting from a 2d array, and we iterate over the first dimmension
        // make and ifdef to only do this if we want run to run reproducible results
        // CPU version here is still surprisingly slow
#ifdef ENABLE_RUN_REPRODUCE
        Iterator::iterate(OffloadExec(), [] DEVCALLABLE(pInd_t pInd, LinkData &lData) {
            int numP = lData.linksPerParticle[pInd];
            if (numP < 2)
                return;
            // in place bubblesort so that i can easily offload it
            for (int i = 0; i < numP; i++) {
                for (int j = i + 1; j < numP; j++) {
                    if (lData.getPartLinkId(j, pInd) < lData.getPartLinkId(i, pInd)) {
                        auto temp = lData.getPartLinkId(i, pInd);
                        lData.setPartLinkId(i, pInd, lData.getPartLinkId(j, pInd));
                        lData.setPartLinkId(j, pInd, temp);
                    }
                }
            }
        }, numParticles, *this);

        Iterator::syncAll();

#endif
    }

    void connectParticleLinks(GlobalLocalMapper<gKey_t, pInd_t> &pMapper) {
        RangeV2 r("connect particles", 3);

        makeLocalPartEnds(pMapper, false);

        buildLinkIterationTable(pMapper.numActive());
    }

    // todo make it start from a specific offset, and get rid of allow dangling 
    void makeLocalPartEnds(GlobalLocalMapper<gKey_t, pInd_t> &pMapper, bool allowDangling) {
        RangeV2 r("make local link ends", 3);

        int self;

        MPI_Comm_rank(MPI_COMM_WORLD, &self);

        localPartIndA.clear();
        localPartIndB.clear();
        localPartIndA.resize(numLocal, false);
        localPartIndB.resize(numLocal, false);


        Iterator::iterate(ParallelExec(), [=](ind_t i, LinkData &lData, GlobalLocalMapper<gKey_t, pInd_t> &pMapper) {

            pInd_t pIndA = pMapper.getLocalInd(lData.globalKeyEndA[i]);
            pInd_t pIndB = pMapper.getLocalInd(lData.globalKeyEndB[i]);

            if (lData.state[i] == 0)
                return;
            if ((pIndA == -1 || pIndB == -1) && !allowDangling) {
                std::cout << self << " missing particles from link " << pIndA << " " << pIndB << " "
                        << lData.globalKeyEndA[i] << " " << lData.globalKeyEndB[i] << std::endl;
                raise(SIGABRT);
            }
            lData.localPartIndA[i] = pIndA;
            lData.localPartIndB[i] = pIndB;
        }, numLocal, *this, pMapper);

        Iterator::syncAll();
    }

    void reorderLinkTargets(ManVec<lInd_t> &newOrder) {
        RangeV2 r("reoder link targets", 3);

        ManVec<lInd_t> reverseLookup;
        reverseLookup.resize(newOrder.size(), false);
        lInd_t *reverseLookupPtr = reverseLookup.data();
        lInd_t *newOrderPtr = newOrder.data();
        Iterator::syncAll();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(lInd_t i) {
            reverseLookupPtr[newOrderPtr[i]] = i;
        }, newOrder.size());

        Iterator::syncAll();

        MPI_Barrier(MPI_COMM_WORLD);

        pInd_t *localPartIndAPtr = localPartIndA.data();
        pInd_t *localPartIndBPtr = localPartIndB.data();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(lInd_t i) {
            localPartIndAPtr[i] = reverseLookupPtr[localPartIndAPtr[i]];
            localPartIndBPtr[i] = reverseLookupPtr[localPartIndBPtr[i]];
        }, numLocal);

        MPI_Barrier(MPI_COMM_WORLD);

        Iterator::syncAll();

        // reorderDevVec(linksPerParticle, newOrder);

        // particleLinkIds ?
        buildLinkIterationTable(newOrder.size());

        Iterator::syncAll();
    }

    void remap(GlobalLocalMapper<gKey_t, lInd_t> &mapper) {
        RangeV2 r("remap links", 3);
        mapper.setSize(globalKeys.size());
        mapper.clear();
        for (ind_t i = 0; i < globalKeys.size(); ++i) {
            mapper.map(globalKeys[i], i);
        }
        mapper.finalize();
    }

    void remapHalos(GlobalLocalMapper<gKey_t, lInd_t> &mapper, ParticleData &pData) {
        RangeV2 r("remap halo links", 3);

        mapper.setSize(globalKeys.size());
        mapper.clear();
        int localRank = getLocalRank();
        // do we really need the openmp parallelism here ?
        for (ind_t i = 0; i < globalKeys.size(); ++i) {
            if (state[i] == 0)
                continue;
            if (pData.owners[localPartIndA[i]] != localRank || pData.owners[localPartIndB[i]] != localRank) {
                mapper.map(globalKeys[i], i);
            }
        }
        mapper.finalize();
    }

    void generateOwnedLinksList() {
        RangeV2 r("generate owned links list", 3);
        ownedIndexes.clear();
        nonOwnedIndexes.clear();
        int self = getLocalRank();
        for (size_t i = 0; i < numLocal; ++i) {
            if (owners[i] == self) {
                ownedIndexes.push_back(i);
            } else {
                nonOwnedIndexes.push_back(i);
            }
        }
    }

    ManVec<lInd_t> getNewOrder() {
        RangeV2 r("get new order links", 3);

        ManVec<lInd_t> indexesIn;
        indexesIn.resize(numLocal);
        auto indexesPtr = indexesIn.ptr;
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(lInd_t i) {
            indexesPtr[i] = i;
        }, numLocal);

        ParallelSort sorter;
        // todo check that CPU and GPU gives the same ish results
#ifdef SOA_CT
    sorter.sortIndirectNLevel(indexesIn, particleLinkNum, localPartIndA);
#else
        sorter.sortIndirectNLevel(indexesIn, localPartIndA, particleLinkNum);
#endif

        return indexesIn;
    }


    void reindexSideData(ManVec<lInd_t> &newSideOrder) {
        RangeV2 r("reinidex links side date", 3);

        ManVec<lInd_t> reverseLookup(newSideOrder.size());

        // needed to get iterator to work, should figure out how to pass manVecs from the stack at some point
        lInd_t *reverseLookupPtr = reverseLookup.data();
        lInd_t *newCTOrderPtr = newSideOrder.data();
        lInd_t *sideIndEndAPtr = sideIndEndA.data();
        lInd_t *sideIndEndBPtr = sideIndEndB.data();


        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (lInd_t i) {
            reverseLookupPtr[newCTOrderPtr[i]] = i;
        }, newSideOrder.size());
        Iterator::syncAll();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (lInd_t i) {
            sideIndEndAPtr[i] = reverseLookupPtr[sideIndEndAPtr[i]];
            sideIndEndBPtr[i] = reverseLookupPtr[sideIndEndBPtr[i]];
        }, sideIndEndA.size());
    }

    virtual void dropLinks(ManVec<lInd_t> &droppedLocalIndexes) = 0;

    virtual void dropUnusedLinks(GlobalLocalMapper<gKey_t, pInd_t> &pMapper) = 0;

    virtual void dropBrokenLinks() = 0;

    virtual void dropNonOwnedLinks() = 0;

    virtual void clear() = 0;

    virtual void resize(size_t newSize) = 0;

    virtual void reorder(ManVec<lInd_t> &newOrder) = 0;

    virtual void sortSideData(ParticleData &pData) = 0;

    virtual void reorderSideData(ManVec<lInd_t> &newOrder) = 0;

    virtual void resizeSideData(size_t newSize) = 0;

    virtual void dropSideData(ManVec<lInd_t> &droppedSideInd) = 0;
};


#endif //PHELL_LINKDATA_H
