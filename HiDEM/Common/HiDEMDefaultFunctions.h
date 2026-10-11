//
// Created by Fredrik Robertsen on 5/11/26.
//

#ifndef PHELL_HIDEMDEFAULTFORCEFUN_H
#define PHELL_HIDEMDEFAULTFORCEFUN_H

#include <cmath>

#include <Util/Real3.h>

#include "SIMDHelper.h"
#include "DevCall.h"
#include "HiDEMGlobal.h"

// ------------------------------------------------------------------------------------------
// NOTE: This file is used to not have to replicate the same code in every sample
// Look at the IceBlock sample for how to easily define your own force functions
// ------------------------------------------------------------------------------------------

struct HiDEMDefNeighForceFunc {
    lpReal_t invDt;
    lpReal_t attractDistance;
    lpReal_t attractScaleForce;
    lpReal_t damping;

    lpReal_t dampingRotation;
    lpReal_t scaleE;

    DEVCALLABLE
    HiDEMDefNeighForceFunc(lpReal_t invDt, lpReal_t attractDistance, lpReal_t attractScaleForce, lpReal_t damping,
                           lpReal_t dampingRotation, lpReal_t scaleE)
        : invDt(invDt), attractDistance(attractDistance), attractScaleForce(attractScaleForce), damping(damping), dampingRotation(dampingRotation),
          scaleE(scaleE)
    {}


    REALT_TEMPL_LONG
    DEVCALLABLE
    void operator()(
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

        mask = select(particleDistance2 >= activeDist2, static_cast<lpReal_t>(0.f), mask);

        lpReal_t particleDistance = std::sqrt(particleDistance2);

        lpReal_t invDist = static_cast<lpReal_t>(1.0f) / particleDistance;
        lpReal_t xComp = (xDist) * invDist;
        lpReal_t yComp = (yDist) * invDist;
        lpReal_t zComp = (zDist) * invDist;

        lpReal_t Cnst = 0;
        lpReal_t dimDist = (radSum - particleDistance);
        Cnst = select(particleDistance < radSum, std::sqrt(dimDist * dimDist * dimDist), Cnst);
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
};


struct HiDEMDefLinkForceFunc {
    lpReal_t invDt;
    lpReal_t damping;
    lpReal_t dampingRotation;
    lpReal_t E;
    lpReal_t W2;
    lpReal_t W4;
    lpReal_t G;

    DEVCALLABLE

    HiDEMDefLinkForceFunc(lpReal_t invDt, lpReal_t damping, lpReal_t dampingRotation, lpReal_t E, lpReal_t W2, lpReal_t W4, lpReal_t G)
        : invDt(invDt), damping(damping), dampingRotation(dampingRotation), E(E), W2(W2), W4(W4), G(G) {
    }

    REALT_TEMPL_LONG
    DEVCALLABLE
    void operator()(
        lpReal_t &mask,
        const lpReal_t &xDist, const lpReal_t &yDist, const lpReal_t &zDist,

        const lpReal_t &xDiff, const lpReal_t &yDiff, const lpReal_t &zDiff,
        const lpReal_t &uDiff, const lpReal_t &vDiff, const lpReal_t &wDiff,
        const lpReal_t &radius,

        const lpReal_t &xDiffOther, const lpReal_t &yDiffOther, const lpReal_t &zDiffOther,
        const lpReal_t &uDiffOther, const lpReal_t &vDiffOther, const lpReal_t &wDiffOther,
        const lpReal_t &radiusOther,

        ctReal_t &CT0, ctReal_t &CT1, ctReal_t &CT2,
        ctReal_t &CT3, ctReal_t &CT4, ctReal_t &CT5,

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
        if (xDist == 0 && yDist == 0) {
            xDist2 = 1e-10;
            yDist2 = 1e-10;
        }
        lpReal_t zDist2 = zDist * zDist;
        lpReal_t distSum = xDist2 + yDist2 + zDist2;
        lpReal_t invLI = 1.f / sqrt(distSum);
        lpReal_t invLP = 1.f / sqrt(xDist2 + yDist2);
        lpReal_t invLILP = invLI * invLP;
        lpReal_t LI = invLI * (distSum);

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

        lpReal_t LIDiv6 = (1.f / lpReal_t(6.0f)) * invLI;

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
};


struct HiDEMDefLinkBreakFunc {
    lpReal_t bendBreak;
    lpReal_t maxLoad;

    DEVCALLABLE
    HiDEMDefLinkBreakFunc(lpReal_t bendBreak, lpReal_t maxLoad)
        : bendBreak(bendBreak), maxLoad(maxLoad) {
    }

    DEVCALLABLE
    void operator()(
        const hpReal_t &xCord1, const hpReal_t &yCord1, const hpReal_t &zCord1,
        const lpReal_t &xTheta1, const lpReal_t &yTheta1, const lpReal_t &zTheta1,
        const hpReal_t &xCord2, const hpReal_t &yCord2, const hpReal_t &zCord2,
        const lpReal_t &xTheta2, const lpReal_t &yTheta2, const lpReal_t &zTheta2,
        uint8_t &state, lpReal_t &initialLen
    ) {
        // only write to lData.XXX[lInd]
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
};

DEVCALLABLE
inline void HiDEMDefVelocityLimit(const lpReal_t maxVel, const lpReal_t maxRot, const lpReal_t dt,
                           lpReal_t xVel, hpReal_t &xCord, hpReal_t xCordPrev,
                           lpReal_t yVel, hpReal_t &yCord, hpReal_t yCordPrev,
                           lpReal_t zVel, hpReal_t &zCord, hpReal_t zCordPrev,
                           lpReal_t xThetaVel, lpReal_t &xTheta, lpReal_t xThetaPrev,
                           lpReal_t yThetaVel, lpReal_t &yTheta, lpReal_t yThetaPrev,
                           lpReal_t zThetaVel, lpReal_t &zTheta, lpReal_t zThetaPrev) {
    // Restrict maximum particle velocity to below maxVel and maxRot
    if (xVel > maxVel) {
        xCord = dt * maxVel + xCordPrev;
    }
    if (yVel > maxVel) {
        yCord = dt * maxVel + yCordPrev;
    }
    if (zVel > maxVel) {
        zCord = dt * maxVel + zCordPrev;
    }
    if (xThetaVel > maxRot) {
        xTheta = dt * maxRot + xThetaPrev;
    }
    if (yThetaVel > maxRot) {
        yTheta = dt * maxRot + yThetaPrev;
    }
    if (zThetaVel > maxRot) {
        zTheta = dt * maxRot + zThetaPrev;
    }
    // Restrict maximum particle velocity to above -maxVel and -maxRot
    if (xVel < -maxVel) {
        xCord = dt * -maxVel + xCordPrev;
    }
    if (yVel < -maxVel) {
        yCord = dt * -maxVel + yCordPrev;
    }
    if (zVel < -maxVel) {
        zCord = dt * -maxVel + zCordPrev;
    }
    if (xThetaVel < -maxRot) {
        xTheta = dt * -maxRot + xThetaPrev;
    }
    if (yThetaVel < -maxRot) {
        yTheta = dt * -maxRot + yThetaPrev;
    }
    if (zThetaVel < -maxRot) {
        zTheta = dt * -maxRot + zThetaPrev;
    }
}

DEVCALLABLE
inline void HiDEMDefBoundaryCondition(const lpReal_t radius, const Real3 lowerLimit, const Real3 upperLimit,
                                      bool xBoundaryMin, bool xBoundaryMax, bool yBoundaryMin, bool yBoundaryMax,bool zBoundaryMin, bool zBoundaryMax,
                                      hpReal_t &xCord, hpReal_t xCordPrev, hpReal_t &yCord, hpReal_t yCordPrev,hpReal_t &zCord, hpReal_t zCordPrev) {
    // Boundary conditions
    if (xCord + radius < lowerLimit.x && xBoundaryMin) {
        xCord = xCordPrev;
    }
    if (xCord - radius > upperLimit.x && xBoundaryMax) {
        xCord = xCordPrev;
    }
    if (yCord + radius < lowerLimit.y && yBoundaryMin) {
        yCord = yCordPrev;
    }
    if (yCord - radius > upperLimit.y && yBoundaryMax) {
        yCord = yCordPrev;
    }
    if (zCord + radius < lowerLimit.z && zBoundaryMin) {
        zCord = zCordPrev;
    }
    if (zCord - radius > upperLimit.z && zBoundaryMax) {
        zCord = zCordPrev;
    }
}

DEVCALLABLE
inline auto HiDEMDefTimeIntegration(const lpReal_t dragCoeff, const lpReal_t invDt2, const lpReal_t particleMass, const lpReal_t invDtSqr,
                                    const lpReal_t dtSqr, const lpReal_t particleMomentInertia, const lpReal_t invDt,
                                    lpReal_t xForce, lpReal_t xDiff, hpReal_t &xCord, hpReal_t &xCordPrev,
                                    lpReal_t yForce, lpReal_t yDiff, hpReal_t &yCord, hpReal_t &yCordPrev,
                                    lpReal_t zForce, lpReal_t zDiff, hpReal_t &zCord, hpReal_t &zCordPrev,
                                    lpReal_t uForce, lpReal_t uDiff, lpReal_t &xTheta, lpReal_t &xThetaPrev,
                                    lpReal_t vForce, lpReal_t vDiff, lpReal_t &yTheta, lpReal_t &yThetaPrev,
                                    lpReal_t wForce, lpReal_t wDiff, lpReal_t &zTheta, lpReal_t &zThetaPrev) {
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

    lpReal_t xVel = (xDiffNext) * invDt;
    lpReal_t yVel = (yDiffNext) * invDt;
    lpReal_t zVel = (zDiffNext) * invDt;
    lpReal_t xThetaVel = (xTheta - xThetaPrev) * invDt;
    lpReal_t yThetaVel = (yTheta - yThetaPrev) * invDt;
    lpReal_t zThetaVel = (zTheta - zThetaPrev) * invDt;

    return std::make_tuple(xVel, yVel, zVel, xThetaVel, yThetaVel, zThetaVel);

}


#endif //PHELL_HIDEMDEFAULTFORCEFUN_H
