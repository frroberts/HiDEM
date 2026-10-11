//
// Created by Fredrik Robertsen on 10/10/2020.
//

#ifndef PHELL_HIDEMLINKDATA_H
#define PHELL_HIDEMLINKDATA_H


#include <Global.h>
#include <GlobalLocalMapper.h>
#include <ManVec.h>
#include <LinkData.h>
#include <TupleHelpers.h>
#include <Util/OwnedListGen.h>

#include "HiDEMGlobal.h"
#include "Generators/ElementrayLink.h"


class HiDEMLinkData: public LinkData {
public:
    //



    ManVec<ctReal_t> CT0_flat;
    ManVec<ctReal_t> CT1_flat;
    ManVec<ctReal_t> CT2_flat;
    ManVec<ctReal_t> CT3_flat;
    ManVec<ctReal_t> CT4_flat;
    ManVec<ctReal_t> CT5_flat;

    ManVec<lpReal_t> initialLen;

    HiDEMLinkData()
    {
        //
    }

    // sized num links
    auto getFieldTupleInput() {
        return packVecPtrs(owners, state, globalKeys, globalKeyEndA, globalKeyEndB, initialLen, particleLinkNum);
    }

    auto getFieldTupleGenerated() {
        return packVecPtrs(localPartIndA, localPartIndB, sideIndEndA, sideIndEndB);
    }

    // sized num links * 2
    auto getFieldTupleSideInput(){
        return packVecPtrs(CT0_flat, CT1_flat, CT2_flat, CT3_flat, CT4_flat, CT5_flat);
    }

    // delete move and copy constructors? this is not getting cloned ...
    HiDEMLinkData (const HiDEMLinkData &old_obj) = delete;
    HiDEMLinkData ( HiDEMLinkData && ) = delete;


    DEVCALLABLE
    void processInitLink(ElementaryLink &l, HiDEMLinkData &lData, lInd_t outInd, pInd_t sideIndEndA, pInd_t sideIndEndB) {
        lData.globalKeyEndA[outInd] = (l.globalKeyParticleA);
        lData.globalKeyEndB[outInd] = (l.globalKeyParticleB);
        lData.owners[outInd] = (l.owner);
        lData.state[outInd] = (l.state);
        lData.globalKeys[outInd] = (l.globalKey);
        lData.particleLinkNum[outInd] = (l.particleLinkNum);
        lData.initialLen[outInd] = l.initialLen;

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
    }
};


#endif //PHELL_HIDEMLINKDATA_H
