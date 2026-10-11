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

int main(int argc, char *argv[]) {

    ArgParser arguments(argc, argv);

    MPI_Init(&argc, &argv);

    lpReal_t breakTime = arguments.getArg<lpReal_t >("general.breakTime").value_or(10.0);
    lpReal_t dt = arguments.getArg<lpReal_t >("general.dt").value_or(0.0001);

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

    auto neighbourForceFunc =
            [=] DEVCALLABLE REALT_TEMPL (
                    lpReal_t mask,
                    const lpReal_t &xDist, const lpReal_t &yDist, const lpReal_t &zDist,

                    const lpReal_t &xDiff, const lpReal_t &yDiff, const lpReal_t &zDiff,
                    const lpReal_t &uDiff, const lpReal_t &vDiff, const lpReal_t &wDiff,
                    const lpReal_t &radius,

                    const lpReal_t &xDiffOther, const lpReal_t &yDiffOther, const lpReal_t &zDiffOther,
                    const lpReal_t &uDiffOther, const lpReal_t &vDiffOther, const lpReal_t &wDiffOther,
                    const lpReal_t &radiusOther,

                    lpReal_t &xForce, lpReal_t &yForce, lpReal_t &zForce,
                    lpReal_t &uForce, lpReal_t &vForce, lpReal_t &wForce
            ) {

                lpReal_t xDist2 = xDist * xDist;
                lpReal_t yDist2 = yDist * yDist;
                lpReal_t zDist2 = zDist * zDist;

                lpReal_t particleDistance2 = xDist2 + yDist2 + zDist2;

                lpReal_t radSum = radius + radiusOther;
                lpReal_t activeDist2 = (radSum + attractDistance) * (radSum + attractDistance);

                mask = select(particleDistance2 >= activeDist2, lpReal_t(0), mask);

                lpReal_t particleDistance = sqrt(particleDistance2);

                lpReal_t invDist = lpReal_t(1.0) / particleDistance;
                lpReal_t xComp = (xDist) * invDist;
                lpReal_t yComp = (yDist) * invDist;
                lpReal_t zComp = (zDist) * invDist;

                lpReal_t Cnst = 0;
                lpReal_t dimDist = (radSum - particleDistance);
                Cnst = select(particleDistance < radSum, sqrt(dimDist * dimDist * dimDist), Cnst);
                Cnst = select(particleDistance >= radSum && particleDistance < radSum + attractDistance,
                              attractScaleForce * dimDist, Cnst);
                Cnst *= scaleE;

                xForce += mask * ((-damping) * (((xDiff - xDiffOther) * invDt)) + (Cnst * xComp));
                yForce += mask * ((-damping) * (((yDiff - yDiffOther) * invDt)) + (Cnst * yComp));
                zForce += mask * ((-damping) * (((zDiff - zDiffOther) * invDt)) + (Cnst * zComp));
                uForce += mask * ((-dampingRotation) * (((uDiff - uDiffOther) * invDt)));
                vForce += mask * ((-dampingRotation) * (((vDiff - vDiffOther) * invDt)));
                wForce += mask * ((-dampingRotation) * (((wDiff - wDiffOther) * invDt)));
            };

    auto linkForceFunc =
            [=] DEVCALLABLE REALT_TEMPL (
                    lpReal_t &mask,
                    const lpReal_t &xDist, const lpReal_t &yDist, const lpReal_t &zDist,

                    const lpReal_t &xDiff, const lpReal_t &yDiff, const lpReal_t &zDiff,
                    const lpReal_t &uDiff, const lpReal_t &vDiff, const lpReal_t &wDiff,
                    const lpReal_t &radius,

                    const lpReal_t &xDiffOther, const lpReal_t &yDiffOther, const lpReal_t &zDiffOther,
                    const lpReal_t &uDiffOther, const lpReal_t &vDiffOther, const lpReal_t &wDiffOther,
                    const lpReal_t &radiusOther,

                    lpReal_t &CT0, lpReal_t &CT1, lpReal_t &CT2,
                    lpReal_t &CT3, lpReal_t &CT4, lpReal_t &CT5,

                    lpReal_t &xForce, lpReal_t &yForce, lpReal_t &zForce,
                    lpReal_t &uForce, lpReal_t &vForce, lpReal_t &wForce
            ) {

                xForce += (-damping * ((xDiff - xDiffOther) * invDt)) * mask;
                yForce += (-damping * ((yDiff - yDiffOther) * invDt)) * mask;
                zForce += (-damping * ((zDiff - zDiffOther) * invDt)) * mask;
                uForce += (-dampingRotation * ((uDiff - uDiffOther) * invDt)) * mask;
                vForce += (-dampingRotation * ((vDiff - vDiffOther) * invDt)) * mask;
                wForce += (-dampingRotation * ((wDiff - wDiffOther) * invDt)) * mask;

                lpReal_t xDist2 = xDist * xDist;
                lpReal_t yDist2 = yDist * yDist;
                lpReal_t zDist2 = zDist * zDist;
                lpReal_t LI = sqrt(xDist2 + yDist2 + zDist2);
                lpReal_t LP = sqrt(xDist2 + yDist2);
                lpReal_t invLI = lpReal_t(1.0) / LI;
                lpReal_t invLP = lpReal_t(1.0) / LP;
                lpReal_t invLILP = lpReal_t(1.0) / (LI * LP); // invLI * invLP;//

                lpReal_t T00 = xDist * invLI;
                lpReal_t T01 = yDist * invLI;
                lpReal_t T02 = zDist * invLI;

                lpReal_t T10 = -yDist * invLP;
                lpReal_t T11 = xDist * invLP;

                lpReal_t T20 = -(zDist * xDist) * invLILP;
                lpReal_t T21 = -(zDist * yDist) * invLILP;
                lpReal_t T22 = (xDist2 + yDist2) * invLILP;

                lpReal_t B[12];

                B[0] = T00 * (xDiff) + T01 * (yDiff) + T02 * (zDiff);
                B[1] = T10 * (xDiff) + T11 * (yDiff);
                B[2] = T20 * (xDiff) + T21 * (yDiff) + T22 * (zDiff);
                B[4] = T10 * (uDiff) + T11 * (vDiff);
                B[5] = T20 * (uDiff) + T21 * (vDiff) + T22 * (wDiff);

                B[6] = T00 * (xDiffOther) + T01 * (yDiffOther) + T02 * (zDiffOther);
                B[7] = T10 * (xDiffOther) + T11 * (yDiffOther);
                B[8] = T20 * (xDiffOther) + T21 * (yDiffOther) + T22 * (zDiffOther);
                B[10] = T10 * (uDiffOther) + T11 * (vDiffOther);
                B[11] = T20 * (uDiffOther) + T21 * (vDiffOther) + T22 * (wDiffOther);

                lpReal_t L2 = LI * LI;
                lpReal_t L3 = LI * LI * LI;

                lpReal_t K[6];

                lpReal_t LIDiv6 = 1.f / (lpReal_t(6.0) * LI);

                K[0] = E * W2 * invLI;
                K[1] = E * W4 / (L3);
                K[5] = E * W4 / (lpReal_t(2.0) * L2);

                CT0 += K[0] * B[0] + (-K[0]) * B[6];
                CT1 += K[1] * B[1] + K[5] * B[5] + (-K[1]) * B[7] + K[5] * B[11];
                CT2 += K[1] * B[2] + (-K[5]) * B[4] + (-K[1]) * B[8] + (-K[5]) * B[10];

                xForce += (-(T00 * CT0 + T10 * CT1 + T20 * CT2)) * mask;
                yForce += (-(T01 * CT0 + T11 * CT1 + T21 * CT2)) * mask;
                zForce += (-(T02 * CT0 + T22 * CT2)) * mask;

                K[2] = G * W4 * LIDiv6;
                K[3] = E * W4 * LIDiv6 * 2.f;
                K[4] = E * W4 * LIDiv6;

                B[3] = T00 * (uDiff) + T01 * (vDiff) + T02 * (wDiff);
                B[9] = T00 * (uDiffOther) + T01 * (vDiffOther) + T02 * (wDiffOther);

                CT3 += K[2] * B[3] + (-K[2]) * B[9];
                CT4 += (-K[5]) * B[2] + K[3] * B[4] + K[5] * B[8] + K[4] * B[10];
                CT5 += K[5] * B[1] + K[3] * B[5] + (-K[5]) * B[7] + K[4] * B[11];

                uForce += (-(T00 * CT3 + T10 * CT4 + T20 * CT5)) * mask;
                vForce += (-(T01 * CT3 + T11 * CT4 + T21 * CT5)) * mask;
                wForce += (-(T02 * CT3 + T22 * CT5)) * mask;
            };

    auto breakLinks = [=]
            DEVCALLABLE REALT_TEMPL (
        const hpReal_t &xCord1, const hpReal_t &yCord1, const hpReal_t &zCord1,
        const lpReal_t &xTheta1, const lpReal_t &yTheta1, const lpReal_t &zTheta1,
        const hpReal_t &xCord2, const hpReal_t &yCord2, const hpReal_t &zCord2,
        const lpReal_t &xTheta2, const lpReal_t &yTheta2, const lpReal_t &zTheta2,
        uint8_t &state, lpReal_t &initialLen
    ) {
        // check if the link is not broken, and if it is just return to not do anyting
        if (state == 0)
            return;

        lpReal_t xDist = xCord1 - xCord2;
        lpReal_t yDist = yCord1 - yCord2;
        lpReal_t zDist = zCord1 - zCord2;

        //#compute load to be compared to maxLoad
        lpReal_t stretchLength = sqrt((xDist * xDist) + (yDist * yDist) + (zDist * zDist));
        lpReal_t maxBend = std::max(abs(xTheta1 - xTheta2), std::max(abs(yTheta1 - yTheta2), abs(zTheta1 - zTheta2)));
        lpReal_t load = (stretchLength - initialLen) / initialLen + bendBreak * maxBend;

        if (load >= maxLoad) {
            state = 0;
        }
    };

    auto applyForce = [=] DEVCALLABLE(
            pInd_t pInd, HiDEMParticleData &pData) {
        // iterates all particles to move next and prev
        // Compute next positions
        hpReal_t xCord = pData.xCord[pInd];
        hpReal_t yCord = pData.yCord[pInd];
        hpReal_t zCord = pData.zCord[pInd];

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

        // Set dragcoeffcients
        lpReal_t dragCoeff;
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

        if (zCord > waterLevel) {
            zForce += -g * particleMass;
        }
        if (zCord <= waterLevel) {
            zForce += lift * particleMass;
        }

        lpReal_t dragInvDt2 = dragCoeff * invDt2;

        lpReal_t massDtSqr = particleMass * invDtSqr;

        lpReal_t pMassDtSqr = particleMass / dtSqr;
        lpReal_t pMemDtSqr = (particleMomentInertia / dtSqr);

        lpReal_t invC1 = lpReal_t(1.0) / (pMassDtSqr + dragCoeff * invDt2);
        lpReal_t invC2 = lpReal_t(1.0) / (pMemDtSqr + dragCoeff * invDt2);

        lpReal_t interDtSqr = particleMomentInertia * invDtSqr;

        lpReal_t xDiffNext = (xForce + massDtSqr * xDiff + dragInvDt2 * -xDiff) * invC1;
        lpReal_t yDiffNext = (yForce + massDtSqr * yDiff + dragInvDt2 * -yDiff) * invC1;
        lpReal_t zDiffNext = (zForce + massDtSqr * zDiff + dragInvDt2 * -zDiff) * invC1;

        lpReal_t uDiffNext = (uForce + interDtSqr * uDiff + dragInvDt2 * -uDiff) * invC2;
        lpReal_t vDiffNext = (vForce + interDtSqr * vDiff + dragInvDt2 * -vDiff) * invC2;
        lpReal_t wDiffNext = (wForce + interDtSqr * wDiff + dragInvDt2 * -wDiff) * invC2;

        xCordPrev = xCord;
        yCordPrev = yCord;
        zCordPrev = zCord;
        xThetaPrev = xTheta;
        yThetaPrev = yTheta;
        zThetaPrev = zTheta;

        xCord = xCord + xDiffNext;
        yCord = yCord + yDiffNext;
        zCord = zCord + zDiffNext;
        xTheta = xTheta + uDiffNext;
        yTheta = yTheta + vDiffNext;
        zTheta = zTheta + wDiffNext;

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

    while (hiDem.run()) {

        // ------------------------------------------------
        //           Simulation IO
        // ------------------------------------------------
        if (hiDem.runOutput()) {
            RangeV2 r("IO", 1);
            hiDem.computeActiveLinks(pData.iteratedLinks);
            hiDem.computeIteratedNeighbours(pData.iteratedNeighbours);

            hiDem.snapshotParticles(std::make_tuple(
                                            "iteratedNeighbours", "iteratedLinks"),
                                    packVecPtrs(pData.iteratedNeighbours, pData.iteratedLinks));
        }
        hiDem.gridService();

        RangeV2 r("timestep", 1);

        hiDem.computePartForce(neighbourForceFunc, linkForceFunc,
                               packVecPtrs(pData.xDiff, pData.yDiff, pData.zDiff,
                                           pData.uDiff, pData.vDiff, pData.wDiff, pData.radius),
                               packVecPtrs(),
                               packVecPtrs(lData.CT0_flat, lData.CT1_flat, lData.CT2_flat,
                                           lData.CT3_flat, lData.CT4_flat, lData.CT5_flat),
                               packVecPtrs(pData.xForce, pData.yForce, pData.zForce,
                                           pData.uForce, pData.vForce, pData.wForce));

        hiDem.applyForce(applyForce);

        if (hiDem.getCurrentTimestep() * dt > breakTime) {
            hiDem.linkUpdate(breakLinks,
                             packVecPtrs(pData.xCord, pData.yCord, pData.zCord,
                                         pData.xTheta, pData.yTheta, pData.zTheta),
                             packVecPtrs(),
                             packVecPtrs(lData.state, lData.initialLen));
        }

        hiDem.finalizeTs();
    }

    auto iterEnd = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsedIter = iterEnd - iterStart;
    Logger::writeAtFirst("Runtime " + Logger::getLimits(elapsedIter.count(), "s") + " \n");

    hiDem.finalize();

    return 0;
}

