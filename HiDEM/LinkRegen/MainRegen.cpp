//
// Created by Fredrik Robertsen on 11/30/24.
//

#include <string>
#include <chrono>
#include <iostream>
#include <math.h>
#include <mpi.h>

#include "ExecutionPolicy.h"
#include "SIMDHelper.h"

#include "HiDEMLib.h"

#include "Util/Util.h"

#include <filesystem>

#include "Util/GPUSelect.h"

#include <BulkRandom.h>

#include "../Common/HiDEMDefaultFunctions.h"

class HiDEMParticleDataExt : public HiDEMParticleData {
public:
    ManVec<int> flag;

    // stuff used for output, can include garbage data
    DEVCALLABLE
    auto getFieldTupleOutput() {
        return packVecPtrs(
                xForce,yForce,zForce,uForce,vForce,wForce,
                iteratedNeighbours, iteratedLinks, flag);
    }

    DEVCALLABLE
    void processInitParticle(ElementaryParticle &p, HiDEMParticleDataExt &pData, pInd_t outInd) {
        pData.xCord[outInd] = p.cord.x;
        pData.yCord[outInd] = p.cord.y;
        pData.zCord[outInd] = p.cord.z;

        pData.type[outInd] = p.type;
        pData.radius[outInd] = p.radius;

        pData.owners[outInd] = p.owner;
        pData.globalKeys[outInd] = p.globalKey;

        pData.xTheta[outInd] = 0;
        pData.yTheta[outInd] = 0;
        pData.zTheta[outInd] = 0;

        pData.xDiff[outInd] = 0;
        pData.yDiff[outInd] = 0;
        pData.zDiff[outInd] = 0;
        pData.uDiff[outInd] = 0;
        pData.vDiff[outInd] = 0;
        pData.wDiff[outInd] = 0;

        pData.xForce[outInd] = 0;
        pData.yForce[outInd] = 0;
        pData.zForce[outInd] = 0;
        pData.uForce[outInd] = 0;
        pData.vForce[outInd] = 0;
        pData.wForce[outInd] = 0;

        pData.iteratedNeighbours[outInd] = 0;
        pData.iteratedLinks[outInd] = 0;

        pData.flag[outInd] = 0;
    }
};

class HiDEMLinkDataExt : public HiDEMLinkData {
public:
    ManVec<lpReal_t > maxLength;
    ManVec<uint8_t > sideFlag;

    // sized num links
    auto getFieldTupleInput() {
        return packVecPtrs(owners, state, globalKeys, globalKeyEndA, globalKeyEndB, initialLen, particleLinkNum, maxLength);
    }

    // sized num links * 2
    auto getFieldTupleSideInput(){
        return packVecPtrs(CT0_flat, CT1_flat, CT2_flat, CT3_flat, CT4_flat, CT5_flat, sideFlag);
    }

    DEVCALLABLE
    void processInitLink(ElementaryLink &l, HiDEMLinkDataExt &lData, lInd_t outInd, pInd_t sideIndEndA, pInd_t sideIndEndB) {
        lData.globalKeyEndA[outInd] = (l.globalKeyParticleA);
        lData.globalKeyEndB[outInd] = (l.globalKeyParticleB);
        lData.owners[outInd] = (l.owner);
        lData.state[outInd] = (l.state);
        lData.globalKeys[outInd] = (l.globalKey);
        lData.particleLinkNum[outInd] = (l.particleLinkNum);
        lData.initialLen[outInd] = l.initialLen;

        lData.maxLength[outInd] = std::numeric_limits<lpReal_t>::min();

        lData.sideIndEndA[outInd] = sideIndEndA;
        lData.sideIndEndB[outInd] = sideIndEndB;

        lData.CT0_flat[sideIndEndA] = 0;
        lData.CT1_flat[sideIndEndA] = 0;
        lData.CT2_flat[sideIndEndA] = 0;
        lData.CT3_flat[sideIndEndA] = 0;
        lData.CT4_flat[sideIndEndA] = 0;
        lData.CT5_flat[sideIndEndA] = 0;

        lData.CT0_flat[sideIndEndB] = 0;
        lData.CT1_flat[sideIndEndB] = 0;
        lData.CT2_flat[sideIndEndB] = 0;
        lData.CT3_flat[sideIndEndB] = 0;
        lData.CT4_flat[sideIndEndB] = 0;
        lData.CT5_flat[sideIndEndB] = 0;

        lData.sideFlag[sideIndEndA] = 0;
        lData.sideFlag[sideIndEndB] = 0;
    }
};


int main(int argc, char *argv[]) {
    ArgParser arguments(argc, argv);

    MPI_Init(&argc, &argv);
    selectDevice();


    lpReal_t breakTime = arguments.getArg<lpReal_t>("general.breakTime").value_or(10.0);
    lpReal_t dt = arguments.getArg<lpReal_t>("general.dt").value_or(0.0001);

    bool xBoundaryMin = arguments.getArg<bool>("general.xBoundaryMin").value_or(false);
    bool xBoundaryMax = arguments.getArg<bool>("general.xBoundaryMax").value_or(false);
    bool yBoundaryMin = arguments.getArg<bool>("general.yBoundaryMin").value_or(false);
    bool yBoundaryMax = arguments.getArg<bool>("general.yBoundaryMax").value_or(false);
    bool zBoundaryMin = arguments.getArg<bool>("general.zBoundaryMin").value_or(false);
    bool zBoundaryMax = arguments.getArg<bool>("general.zBoundaryMax").value_or(false);

    lpReal_t scale = arguments.getArg<lpReal_t>("general.scale").value_or(1.0);
    lpReal_t beamWidth = arguments.getArg<lpReal_t>("general.beamWidth").value_or(0.6);
    lpReal_t E = arguments.getArg<lpReal_t>("general.E").value_or(1e+09);
    lpReal_t waterLevel = arguments.getArg<lpReal_t>("general.waterLevel").value_or(0.0);
    lpReal_t attractDistance = arguments.getArg<lpReal_t>("general.attractDistance").value_or(0.0);
    lpReal_t attractScaleForce = arguments.getArg<lpReal_t>("general.attractScaleForce").value_or(0.0);

    lpReal_t maxVel = arguments.getArg<lpReal_t>("general.maxVel").value_or(20);
    lpReal_t maxRot = arguments.getArg<lpReal_t>("general.maxRot").value_or(5);
    lpReal_t rhoWater = arguments.getArg<lpReal_t>("general.rhoWater").value_or(1030.0);
    lpReal_t rhoIce = arguments.getArg<lpReal_t>("general.rhoIce").value_or(910.0);

    lpReal_t bedStiffness = arguments.getArg<lpReal_t>("general.bedStiffness").value_or(2e+08);
    lpReal_t bedInteract = arguments.getArg<lpReal_t>("general.bedInteract").value_or(2.0);
    lpReal_t waterDrag = arguments.getArg<lpReal_t>("general.waterDrag").value_or(10);
    lpReal_t airDrag = arguments.getArg<lpReal_t>("general.airDrag").value_or(1);
    lpReal_t bedDrag = arguments.getArg<lpReal_t>("general.bedDrag").value_or(1000.0);

    lpReal_t g = arguments.getArg<lpReal_t>("general.g").value_or(9.81);
    lpReal_t damping = arguments.getArg<lpReal_t>("general.damping").value_or(1000.0);
    lpReal_t dampingRotation = arguments.getArg<lpReal_t>("general.dampingRotation").value_or(1000.0);

    lpReal_t maxLoad = arguments.getArg<lpReal_t>("general.maxLoad").value_or(0.35);
    lpReal_t bendBreak = arguments.getArg<lpReal_t>("general.bendBreak").value_or(0.35);


    // ------------------------------------------------
    //           Set up anything related to HiDEMLib
    // ------------------------------------------------

    HiDEMLib<HiDEMParticleData, HiDEMLinkData> hiDem;
    hiDem.setup(arguments, scale);

    // missing output arguments
    arguments.printPossibleCLIArgs();

    // print the config used
    Logger::writeAtFirst("---------------------------------------\n");
    Logger::writeAtFirst("Using config \n\n");
    Logger::writeAtFirst(toml::format(arguments.table));
    Logger::writeAtFirst("---------------------------------------\n");
    Logger::writeAtFirst("Variable sizes\n");
    Logger::writeAtFirst("sizeof lpReal_t " + std::to_string(sizeof(lpReal_t)) + "\n");
    Logger::writeAtFirst("sizeof hpReal_t " + std::to_string(sizeof(hpReal_t)) + "\n");
    Logger::writeAtFirst("sizeof ctReal_t " + std::to_string(sizeof(ctReal_t)) + "\n");
    Logger::writeAtFirst("sizeof ind_t " + std::to_string(sizeof(ind_t)) + "\n");
    Logger::writeAtFirst("sizeof gKey_t " + std::to_string(sizeof(gKey_t)) + "\n");
    Logger::writeAtFirst("sizeof pInd_t " + std::to_string(sizeof(pInd_t)) + "\n");
    Logger::writeAtFirst("sizeof lInd_t " + std::to_string(sizeof(lInd_t)) + "\n");
    Logger::writeAtFirst("---------------------------------------\n");

    // ------------------------------------------------
    //           Get pointers etc.
    // ------------------------------------------------

    // because OMP ?
    auto limits = hiDem.getSimLimits();
    Real3 lowerLimit = std::get<0>(limits);
    Real3 upperLimit = std::get<1>(limits);

    Logger::writeAtFirst("--------------------------------------- \n");
    Logger::writeAtFirst(fmt::format("Simulation domain lower limit: {} , {}  {} \n", lowerLimit.x, lowerLimit.y,
                                     lowerLimit.z));
    Logger::writeAtFirst(fmt::format("Simulation domain upper limit: {} , {}  {} \n", upperLimit.x, upperLimit.y,
                                     upperLimit.z));
    Logger::writeAtFirst("--------------------------------------- \n");

    auto &pData = *hiDem.getParticlePtr();
    auto &lData = *hiDem.getLinkPtr();

    ExternalForces *extForcePtr = hiDem.getExternalForcePtr();

    auto bed = hiDem.getBedPtrs();
    bed->toVTKHDF("bed.vtkhdf");

    // ------------------------------------------------
    //           Simulation variables
    // ------------------------------------------------

    beamWidth *= scale;
    bedInteract *= scale;
    bedStiffness *= scale * scale;
    waterDrag *= scale * scale;
    airDrag *= scale * scale;
    bedDrag *= scale * scale;
    damping *= scale * scale;
    dampingRotation *= scale * scale;

    lpReal_t lift;
    attractDistance *= scale;
    lift = (rhoWater - rhoIce) / rhoIce * g;

    lpReal_t invDt = 1.0 / dt;

    lpReal_t invDt2 = 1.0 / (2.0 * dt);
    lpReal_t dtSqr = pow(dt, 2.0);
    lpReal_t invDtSqr = 1.0 / pow(dt, 2.0);

    lpReal_t G = E / 2.0;
    lpReal_t W2 = beamWidth * beamWidth;
    lpReal_t W4 = beamWidth * beamWidth * beamWidth * beamWidth;
    lpReal_t scaleE = E * scale;

    lpReal_t radius = pData.radius[0];
    // -------------------------------------------------
    //           Functions
    // -------------------------------------------------
    // Look at the IceBlock sample for how to easily
    // define your own force functions
    // ------------------------------------------------

    lpReal_t linkLenLimit = (radius+radius)*hiDem.geometry.linkGenScale;
    // prob is a random number "uniformly distributed on the interval [a,b)" with a = 0 b = 1 (1 is excluded)
    auto breakLinks = [=]
    DEVCALLABLE REALT_TEMPL (
        const hpReal_t &xCord1, const hpReal_t &yCord1, const hpReal_t &zCord1,
        const lpReal_t &xTheta1, const lpReal_t &yTheta1, const lpReal_t &zTheta1,
        const hpReal_t &xCord2, const hpReal_t &yCord2, const hpReal_t &zCord2,
        const lpReal_t &xTheta2, const lpReal_t &yTheta2, const lpReal_t &zTheta2,
        uint8_t &state, lpReal_t &linkLen, lpReal_t &prob
    ) {
        // only write to lData.XXX[lInd]
        lpReal_t xDist = xCord1 - xCord2;
        lpReal_t yDist = yCord1 - yCord2;
        lpReal_t zDist = zCord1 - zCord2;
        lpReal_t currentLength = sqrtf((xDist * xDist) + (yDist * yDist) + (zDist * zDist));

        if (state == 0) {
            if ((currentLength < linkLenLimit) && (prob > 0.995f)) {
                state = 1;
            }
        } else {
            lpReal_t maxBend = std::max(abs(xTheta1 - xTheta2),
                                        std::max(abs(yTheta1 - yTheta2), abs(zTheta1 - zTheta2)));
            lpReal_t load = (currentLength - linkLen) / linkLen + bendBreak * maxBend;

            if (load >= maxLoad) {
                state = 0;
            }

            if (prob < 1.0f - exp(-50.0f * load)) {
                state = 0;
            }
        }

    };

    auto applyForce = [=] DEVCALLABLE(
        pInd_t pInd, HiDEMParticleData &pData) {
        // iterates all particles to move next and prev
        // Compute next positions

        hpReal_t xCord = pData.xCord[pInd];
        hpReal_t yCord = pData.yCord[pInd];
        hpReal_t zCord = pData.zCord[pInd];

        // we can technically get rid of prev, it can be recomputed from diff
        hpReal_t xCordPrev;
        hpReal_t yCordPrev;
        hpReal_t zCordPrev;

        lpReal_t xDiff = pData.xDiff[pInd];;
        lpReal_t yDiff = pData.yDiff[pInd];;
        lpReal_t zDiff = pData.zDiff[pInd];;

        lpReal_t xTheta = pData.xTheta[pInd];
        lpReal_t yTheta = pData.yTheta[pInd];
        lpReal_t zTheta = pData.zTheta[pInd];
        lpReal_t xThetaPrev;
        lpReal_t yThetaPrev;
        lpReal_t zThetaPrev;

        lpReal_t uDiff = pData.uDiff[pInd];;
        lpReal_t vDiff = pData.vDiff[pInd];;
        lpReal_t wDiff = pData.wDiff[pInd];;

        lpReal_t radius = pData.radius[pInd];
        lpReal_t diameter = radius * lpReal_t(2.0);

        lpReal_t bedzCord = bed->getHeight(xCord, yCord);

        // Set dragcoeffcients
        lpReal_t dragCoeff;

        if ((zCord - radius) - bedzCord < bedInteract) {
            dragCoeff = std::min(bedDrag * bed->getFric(xCord, yCord), lpReal_t(1e+8) * scale * scale);
        } else
            if (zCord > waterLevel) {
            dragCoeff = airDrag;
        } else {
            dragCoeff = waterDrag;
        }


        lpReal_t xForce = pData.xForce[pInd];
        lpReal_t yForce = pData.yForce[pInd];
        lpReal_t zForce = pData.zForce[pInd];
        lpReal_t uForce = pData.uForce[pInd];
        lpReal_t vForce = pData.vForce[pInd];
        lpReal_t wForce = pData.wForce[pInd];

        lpReal_t particleMass = rhoIce * diameter * diameter * diameter;
        lpReal_t particleMomentInertia = particleMass * diameter * diameter / lpReal_t(6.0);

        // Compute gravity/buoyancy force
        if (zCord > waterLevel) {
            zForce += -g * particleMass;
        }
        if (zCord <= waterLevel) {
            zForce += lift * particleMass;
        }

        //Bed interaction force
        if (zCord - radius < bedzCord) {
            zForce += bedStiffness * (bedzCord - (zCord - radius));
        }

        // Additional force components
        xForce += extForcePtr->xForce->interp(xCord, yCord);
        yForce += extForcePtr->yForce->interp(xCord, yCord);
        zForce += extForcePtr->zForce->interp(xCord, yCord);
        uForce += extForcePtr->uForce->interp(xCord, yCord);
        vForce += extForcePtr->vForce->interp(xCord, yCord);
        wForce += extForcePtr->wForce->interp(xCord, yCord);

        auto [xVel,yVel,zVel,xThetaVel,yThetaVel,zThetaVel] =
                HiDEMDefTimeIntegration(dragCoeff, invDt2, particleMass, invDtSqr,
                                        dtSqr, particleMomentInertia, invDt,
                                        xForce, xDiff, xCord, xCordPrev,
                                        yForce, yDiff, yCord, yCordPrev,
                                        zForce, zDiff, zCord, zCordPrev,
                                        uForce, uDiff, xTheta, xThetaPrev,
                                        vForce, vDiff, yTheta, yThetaPrev,
                                        wForce, wDiff, zTheta, zThetaPrev);

        // apply default velocity limits
        HiDEMDefVelocityLimit(maxVel, maxRot, dt,
                              xVel, xCord, xCordPrev,
                              yVel, yCord, yCordPrev,
                              zVel, zCord, zCordPrev,
                              xThetaVel, xTheta, xThetaPrev,
                              yThetaVel, yTheta, yThetaPrev,
                              zThetaVel, zTheta, zThetaPrev);

        // apply default domain boundary limits
        HiDEMDefBoundaryCondition(radius, lowerLimit, upperLimit,
                                  xBoundaryMin, xBoundaryMax,
                                  yBoundaryMin, yBoundaryMax,
                                  zBoundaryMin, zBoundaryMax,
                                  xCord, xCordPrev,
                                  yCord, yCordPrev,
                                  zCord, zCordPrev);

        pData.xCord[pInd] = xCord;
        pData.yCord[pInd] = yCord;
        pData.zCord[pInd] = zCord;
        pData.xTheta[pInd] = xTheta;
        pData.yTheta[pInd] = yTheta;
        pData.zTheta[pInd] = zTheta;

        pData.xDiff[pInd] = xCord - xCordPrev;
        pData.yDiff[pInd] = yCord - yCordPrev;
        pData.zDiff[pInd] = zCord - zCordPrev;
        pData.uDiff[pInd] = xTheta - xThetaPrev;
        pData.vDiff[pInd] = yTheta - yThetaPrev;
        pData.wDiff[pInd] = zTheta - zThetaPrev;
    };


    // ------------------------------------------------
    //           Iteration loop
    // ------------------------------------------------


    MPI_Barrier(MPI_COMM_WORLD);

    auto iterStart = std::chrono::steady_clock::now();
    auto range = xvtxRangeStart("loop");

    BulkRandom<lpReal_t> breakProbs;

    while (hiDem.run()) {

        // ------------------------------------------------
        //           Simulation IO
        // ------------------------------------------------
        if (hiDem.runOutput()) {
            RangeV2 r("IO", 1);

            hiDem.computeActiveLinks(pData.iteratedLinks);
            hiDem.computeIteratedNeighbours(pData.iteratedNeighbours);

            hiDem.snapshotParticles(std::make_tuple(
                                         "iteratedNeighbours", "iteratedLinks", "owner"),
                                    packVecPtrs(pData.iteratedNeighbours, pData.iteratedLinks, pData.owners));
            hiDem.snapshotLinks(std::make_tuple(
                                    "state", "keyEndA", "keyEndB", "globalKey"),
                                packVecPtrs(lData.state, lData.globalKeyEndA, lData.globalKeyEndB,
                                            lData.globalKeys));
        }

        hiDem.gridService();
        auto forceLoop = xvtxRangeStart("forceLoop");

        // we need to regenerate these every iteration
        breakProbs.setSize(lData.numLocal);
        breakProbs.reGenerate();

        RangeV2 r("timestep", 1);

        hiDem.computePartForce(HiDEMDefNeighForceFunc(invDt, attractDistance, attractScaleForce, damping, dampingRotation, scaleE),
            HiDEMDefLinkForceFunc(invDt, damping, dampingRotation, E, W2, W4, G),
                               packVecPtrs(pData.xDiff, pData.yDiff, pData.zDiff,
                                           pData.uDiff, pData.vDiff, pData.wDiff, pData.radius),
                               packVecPtrs(),
                               packVecPtrs(lData.CT0_flat, lData.CT1_flat, lData.CT2_flat,
                                           lData.CT3_flat, lData.CT4_flat, lData.CT5_flat),
                               packVecPtrs(pData.xForce, pData.yForce, pData.zForce,
                                           pData.uForce, pData.vForce, pData.wForce));

        hiDem.applyForce(applyForce);

        if (hiDem.getCurrentTimestep() * dt > breakTime) {
            hiDem.linkUpdateRegen(breakLinks,
                                  packVecPtrs(pData.xCord, pData.yCord, pData.zCord,
                                              pData.xTheta, pData.yTheta, pData.zTheta),
                                  packVecPtrs(),
                                  packVecPtrs(lData.state, lData.initialLen, breakProbs.values));
        }

        hiDem.finalizeTs();
        xvtxRangeEnd(forceLoop);

    }
    xvtxRangeEnd(range);



    auto iterEnd = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsedIter = iterEnd - iterStart;
    Logger::writeAtFirst("Runtime " + Logger::getLimits(elapsedIter.count(), "s") + " \n");

    hiDem.finalize();
    // todo write final output

    delete &pData;
    delete &lData;

    return 0;
}
