//
// Created by Fredrik Robertsen on 08/10/2020.
//

#ifndef PHELL_HIDEMPARTICLEDATA_H
#define PHELL_HIDEMPARTICLEDATA_H

#include <math.h>
#include <Global.h>

#include "TupleHelpers.h"
#include <GlobalLocalMapper.h>
#include <ManVec.h>
#include <ParticleData.h>

#include "Util/OwnedListGen.h"

#include "Generators/ElementaryParticle.h"


class HiDEMParticleData : public ParticleData {
public:

    ManVec<lpReal_t> xTheta;
    ManVec<lpReal_t> yTheta;
    ManVec<lpReal_t> zTheta;

    ManVec<lpReal_t> xForce;
    ManVec<lpReal_t> yForce;
    ManVec<lpReal_t> zForce;
    ManVec<lpReal_t> uForce;
    ManVec<lpReal_t> vForce;
    ManVec<lpReal_t> wForce;

    ManVec<int> iteratedNeighbours;
    ManVec<int> iteratedLinks;

    ManVec<lpReal_t> xDiff;
    ManVec<lpReal_t> yDiff;
    ManVec<lpReal_t> zDiff;
    ManVec<lpReal_t> uDiff;
    ManVec<lpReal_t> vDiff;
    ManVec<lpReal_t> wDiff;



    // stuff used for input
    DEVCALLABLE
    auto getFieldTupleInput() {
        return packVecPtrs(
                xCord, yCord, zCord,
                xTheta, yTheta, zTheta,
                xDiff, yDiff, zDiff, uDiff, vDiff, wDiff,
                radius, owners, type, globalKeys);
    }

    // stuff used for output, can include garbage data
    DEVCALLABLE
    auto getFieldTupleOutput() {
        return packVecPtrs(
                xForce,yForce,zForce,uForce,vForce,wForce,
                iteratedNeighbours, iteratedLinks);
    }

    // stuff used for forces
    DEVCALLABLE
    auto getFieldTupleForce() {
        return packVecPtrs(
                xForce,yForce,zForce,uForce,vForce,wForce);
    }

    HiDEMParticleData() = default;
    // delete move and copy constructors? this is not getting cloned ...
    HiDEMParticleData (const HiDEMParticleData &old_obj) = delete;
    HiDEMParticleData ( HiDEMParticleData && ) = delete;

    DEVCALLABLE
    void processInitParticle(ElementaryParticle &p, HiDEMParticleData &pData, pInd_t outInd) {
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

        pData.xDiff[outInd] = 0; // option for initial velocity ?
        pData.yDiff[outInd] = 0; // option for initial velocity ?
        pData.zDiff[outInd] = 0; // option for initial velocity ?
        pData.uDiff[outInd] = 0; // option for initial velocity ?
        pData.vDiff[outInd] = 0; // option for initial velocity ?
        pData.wDiff[outInd] = 0; // option for initial velocity ?

        pData.xForce[outInd] = 0;
        pData.yForce[outInd] = 0;
        pData.zForce[outInd] = 0;
        pData.uForce[outInd] = 0;
        pData.vForce[outInd] = 0;
        pData.wForce[outInd] = 0;

        pData.iteratedNeighbours[outInd] = 0;
        pData.iteratedLinks[outInd] = 0;
    }

};




#endif //PHELL_HIDEMPARTICLEDATA_H
