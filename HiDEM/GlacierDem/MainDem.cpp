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

#include "../Common/HiDEMDefaultFunctions.h"
#include "Util/GPUSelect.h"

int main(int argc, char *argv[]) {

    ArgParser arguments(argc, argv);

    MPI_Init(&argc, &argv);
    selectDevice();

    lpReal_t breakTime = arguments.getArg<lpReal_t >("general.breakTime").value_or(10.0);
    lpReal_t dt = arguments.getArg<lpReal_t >("general.dt").value_or(0.0001);

    bool xBoundaryMin = arguments.getArg<bool>("general.xBoundaryMin").value_or(false);
    bool xBoundaryMax = arguments.getArg<bool>("general.xBoundaryMax").value_or(false);
    bool yBoundaryMin = arguments.getArg<bool>("general.yBoundaryMin").value_or(false);
    bool yBoundaryMax = arguments.getArg<bool>("general.yBoundaryMax").value_or(false);
    bool zBoundaryMin = arguments.getArg<bool>("general.zBoundaryMin").value_or(false);
    bool zBoundaryMax = arguments.getArg<bool>("general.zBoundaryMax").value_or(false);

    lpReal_t scale = arguments.getArg<lpReal_t>("general.scale").value_or(1.0);
    lpReal_t beamWidth = arguments.getArg<lpReal_t >("general.beamWidth").value_or(0.6);
    lpReal_t E = arguments.getArg<lpReal_t>("general.E").value_or(1e+09);
    lpReal_t waterLevel = arguments.getArg<lpReal_t >("general.waterLevel").value_or(0.0);
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
    Logger::writeAtFirst(fmt::format("Simulation domain lower limit: {} , {}  {} \n", lowerLimit.x, lowerLimit.y, lowerLimit.z));
    Logger::writeAtFirst(fmt::format("Simulation domain upper limit: {} , {}  {} \n", upperLimit.x, upperLimit.y, upperLimit.z));
    Logger::writeAtFirst("--------------------------------------- \n");

    auto &pData = *hiDem.getParticlePtr();
    auto &lData = *hiDem.getLinkPtr();

    // optional things can be used or not

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

    // ------------------------------------------------
    //           Functions
    // ------------------------------------------------


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
        if ((zCord + radius) - bedzCord < bedInteract) { // should be - radius ?
            dragCoeff = std::min(bedDrag * bed->getFric(xCord, yCord), lpReal_t(1e+5) * scale * scale);
        } else if (zCord > waterLevel) {
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
        if (zCord + radius < bedzCord) {
            zForce += bedStiffness * (bedzCord - (zCord + radius));
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

    devProfilerStart();

    MPI_Barrier(MPI_COMM_WORLD);

    auto iterStart = std::chrono::steady_clock::now();

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

        RangeV2 r("timestep", 1);
        auto forceLoop = xvtxRangeStart("forceLoop");

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
            hiDem.linkUpdate(HiDEMDefLinkBreakFunc(bendBreak, maxLoad),
                             packVecPtrs(pData.xCord, pData.yCord, pData.zCord,
                                         pData.xTheta, pData.yTheta, pData.zTheta),
                             packVecPtrs(),
                             packVecPtrs(lData.state, lData.initialLen));
        }

        hiDem.finalizeTs();
        xvtxRangeEnd(forceLoop);

    }

    devProfilerStop();

    auto iterEnd = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsedIter = iterEnd - iterStart;
    Logger::writeAtFirst("Runtime " + Logger::getLimits(elapsedIter.count(), "s") + " \n");

    hiDem.finalize();
    // todo write final output

    return 0;
}

