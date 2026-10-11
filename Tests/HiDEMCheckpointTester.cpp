//
// Created by Fredrik Robertsen on 3/21/25.
//

#include <catch2/catch.hpp>

#include "LinkDataHandler.h"
#include "ParticleDataHandler.h"
#include "../HiDEMLib/HiDEMH5Checkpoint.h"
#include "../HiDEMLib/HiDEMLinkData.h"
#include "../HiDEMLib/HiDEMParticleData.h"


TEST_CASE("testing reading/writing particles", "[HiDEMCheckpoint]") {
    //


    HiDEMH5Checkpoint checkpoint;

    checkpoint.newCheckpooint("tmp.h5");

    ParticleDataHandler<HiDEMParticleData> pData;

    pData.resize(1);

    // set owner to 0 to get wned/non owned list generaton to generate the correct indexes
    pData.owners[0] = 0;
    generateOwnedNonOwned(pData.owners, pData.ownedIndexes, pData.nonOwnedIndexes, pData.numLocal);

    int tag = 0;
    callForTupleElem(pData.getFieldTupleInput(), [&] <typename T> (ManVec<T> *ptr) {
        (*ptr)[0] = tag;
        tag++;
    });

    checkpoint.writeParticleDataTup(pData);
    checkpoint.closeFile();

    HiDEMH5Checkpoint checkpointIn;
    checkpointIn.openCheckpooint("tmp.h5");
    ParticleDataHandler<HiDEMParticleData> pDataIn;
    checkpointIn.readParticleDataTup(pDataIn);

    REQUIRE(pData.numLocal == pDataIn.numLocal);

    tag = 0;
    callForTupleElem(pDataIn.getFieldTupleInput(), [&] <typename T> (ManVec<T> *ptr) {
        REQUIRE((*ptr)[0] == tag);
        tag++;
    });

}

TEST_CASE("testing reading/writing links", "[HiDEMCheckpoint]") {
    //


    HiDEMH5Checkpoint checkpoint;

    checkpoint.newCheckpooint("tmp.h5");

    LinkDataHandler<HiDEMLinkData> lData;

    lData.resize(1);
    lData.resizeSideData(2);

    // set owner to 0 to get wned/non owned list generaton to generate the correct indexes
    lData.owners[0] = 0;
    generateOwnedNonOwned(lData.owners, lData.ownedIndexes, lData.nonOwnedIndexes, lData.numLocal);

    int tag = 0;
    callForTupleElem(lData.getFieldTupleInput(), [&] <typename T> (ManVec<T> *ptr) {
        (*ptr)[0] = tag;
        ++tag;
    });
    callForTupleElem(lData.getFieldTupleSideInput(), [&] <typename T> (ManVec<T> *ptr) {
        (*ptr)[0] = tag;
        ++tag;
        (*ptr)[1] = tag;
        ++tag;
    });
    lData.sideIndEndA[0] = 0;
    lData.sideIndEndB[0] = 1;

    checkpoint.writeLinkData(lData);
    checkpoint.closeFile();

    HiDEMH5Checkpoint checkpointIn;
    checkpointIn.openCheckpooint("tmp.h5");
    LinkDataHandler<HiDEMLinkData> lDataIn;
    checkpointIn.readLinkData(lDataIn);

    REQUIRE(lData.numLocal == lDataIn.numLocal);

    tag = 0;
    callForTupleElem(lDataIn.getFieldTupleInput(), [&] <typename T> (ManVec<T> *ptr) {
        REQUIRE((*ptr)[0] == tag);
        ++tag;
    });

    callForTupleElem(lDataIn.getFieldTupleSideInput(), [&] <typename T> (ManVec<T> *ptr) {
        REQUIRE((*ptr)[0] == tag);
        ++tag;
        REQUIRE((*ptr)[1] == tag);
        ++tag;
    });
}