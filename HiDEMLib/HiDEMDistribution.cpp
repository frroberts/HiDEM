//
// Created by Fredrik Robertsen on 24/01/2021.
//

#include <random>

#include "HiDEMDistribution.h"
#include "../PLiB/DomainBuilder.h"

#include <../PLiB/Logger/Logger.h>

#include <Transport/LinkTransporter.h>
#include <Transport/ParticleTransporter.h>

void HiDEMDistribution::syncCT(HiDEMLinkData &lData, GlobalLocalMapper<gKey_t, lInd_t> &lMapper) {

    //nvtxRangePush("CT sync links");
    // syncAll up CT from all halos

    lCTComm.invert();
    lCTComm.tagStart = 64 + 12 + 12;
    lCTComm.setNumBuffers(6);

    lCTComm.makeLocalIndexes(lMapper);
    lCTComm.indirectIndexes(lData.sideIndEndB, lData.sideIndEndB);

    lCTComm.packCommData(lData.CT0_flat, 0);
    lCTComm.packCommData(lData.CT1_flat, 1);
    lCTComm.packCommData(lData.CT2_flat, 2);
    lCTComm.packCommData(lData.CT3_flat, 3);
    lCTComm.packCommData(lData.CT4_flat, 4);
    lCTComm.packCommData(lData.CT5_flat, 5);
    Iterator::syncAll();

    lCTComm.postRecvs();
    lCTComm.postSends();
    lCTComm.syncRecvs();
    lCTComm.syncSends();

    lCTComm.unpackCommData(lData.CT0_flat, 0);
    lCTComm.unpackCommData(lData.CT1_flat, 1);
    lCTComm.unpackCommData(lData.CT2_flat, 2);
    lCTComm.unpackCommData(lData.CT3_flat, 3);
    lCTComm.unpackCommData(lData.CT4_flat, 4);
    lCTComm.unpackCommData(lData.CT5_flat, 5);
    Iterator::syncAll();
    //nvtxRangePop();

}

HiDEMDistribution::HiDEMDistribution(bool reBalance, double searchDistance) : reBalanceActive(reBalance) {
    for (int i = 0; i < 4; ++i) {
        grids[i] = new NeighbourGrid();
    }
    grids[0]->gridSpacing = searchDistance;
    grids[1]->gridSpacing = searchDistance * 2;
    grids[2]->gridSpacing = searchDistance * 4;
    grids[3]->gridSpacing = searchDistance * 8;
}

//
void HiDEMDistribution::distribute(HiDEMParticleData &pData, HiDEMLinkData &lData, GlobalLocalMapper<gKey_t, pInd_t> &pMapper, GlobalLocalMapper<gKey_t, lInd_t> &lMapper) {
        int self = getLocalRank();
        RangeV2 r("distribution step", 3);


    auto pTrans = new ParticleTrasporterV2<HiDEMParticleData>();
    auto lTrans = new LinkTransporterV2<HiDEMParticleData, HiDEMLinkData>();

    RangeV2 r1("Reassign and send", 3);

    pTrans->reassignAndGetOutgoing(world, pData);
    MPI_Barrier(MPI_COMM_WORLD);
    lTrans->reassignAndGetOutgoing(world, pData, lData);

    r1.end();

    RangeV2 r2("Dropping", 3);

    pTrans->getIncomming(pData);

    pData.dropNonOwned();

    r2.end();

    RangeV2 r3("append and halo", 3);

    pTrans->getOutgoingHalo(world, pData);
    pTrans->getIncomming(pData);

    // possibly reorder particle stuff here ?
    lTrans->getIncomming(lData);
    lData.dropNonOwnedLinks();
    lData.dropBrokenLinks();

    // possibly reorder link stuff here ?
    r3.end();

    RangeV2 r4("append and halo links", 3);

    pData.remap(pMapper);

    // need to reconnect them here to get which ones are the halos to where
    MPI_Barrier(MPI_COMM_WORLD);

    // todo could make do without this one, maybe ? can i just look it up for all links instead when getting outgoing links
    lData.connectParticleLinks(pMapper);

    lTrans->getOutgoingHalo(world, pData, lData);
    lTrans->getIncomming(lData);
    // todo make an option to connect a subset only
    lData.connectParticleLinks(pMapper);
    r4.end();

    RangeV2 r5("sort order", 3);

    for (int i = 0; i < 4; i++) {
        grids[i]->generateNeighbourGrid(pData);
        grids[i]->colourParticles(pData);
    }
    ManVec<pInd_t> reOrder(pData.numLocal);

    auto reOrderPtr = reOrder.ptr;
    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (pInd_t i) {
        reOrderPtr[i] = i;
    }, pData.numLocal);
    Iterator::syncAll();

    ParallelSort sorter;
    sorter.sortIndirectNLevel(reOrder, grids[3]->colours, grids[2]->colours, grids[1]->colours, grids[0]->colours);
    r5.end();
    RangeV2 r6("link sort", 3);

    pData.reorder(reOrder);
    pData.remap(pMapper);

    auto newLOrder = lData.getNewOrder();
    lData.reorder(newLOrder);

    lData.reorderLinkTargets(reOrder);

    // should not need this ? i could just reindex from the reorder ?
    lData.remapHalos(lMapper, pData);

    lData.sortSideData(pData);
    r6.end();

    RangeV2 r7("make communicator", 3);

    pForceComm.initFromTransporter(*pTrans);
    pForceComm.tagStart = 64;
    pForceComm.setNumBuffers(6);
    pForceComm.makeLocalIndexes(pMapper);

    lCTComm.initFromTransporter(*lTrans);

    // pForceComm.makePreCommIndexes(pData, pMapper);
    r7.end();

    delete lTrans;
    delete pTrans;

}

void HiDEMDistribution::reDistribute(HiDEMParticleData &pData, HiDEMLinkData &lData,
                                GlobalLocalMapper<gKey_t, pInd_t> &pMapper, GlobalLocalMapper<gKey_t, lInd_t> &lMapper,
                                double haloSize, double redistTolerance, bool CTSync = true) {
    // rebalance here ?
    if(CTSync) {
        syncCT(lData, lMapper);
    }

    if (reBalanceActive) {
        int numOwned = pData.ownedIndexes.size();

        xCord.resize(numOwned);
        yCord.resize(numOwned);
        zCord.resize(numOwned);
        weights.resize(numOwned);

        double *xCordPtr = xCord.ptr;
        double *yCordPtr = yCord.ptr;
        double *zCordPtr = zCord.ptr;
        double *weightsPtr = weights.ptr;

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (ind_t i, HiDEMParticleData &pData, HiDEMLinkData& lData, HiDEMDistribution &dist){
            pInd_t pInd = pData.ownedIndexes[i];

            double weight = 1 + pData.iteratedNeighbours[pInd];
            weight += 1 + pData.iteratedLinks[pInd];
            /*
            if(CTSync) {
                lData.iterateParticleLinks([&](lInd_t lInd, pInd_t pIndOther, HiDEMLinkData &lData) {
                    weight += 1;
                }, pInd, lData);
            }
             */
            xCordPtr[i] = pData.xCord[pInd];
            yCordPtr[i] = pData.yCord[pInd];
            zCordPtr[i] = pData.zCord[pInd];
            weightsPtr[i] = weight;
        }, numOwned, pData, lData, *this);

        DomainBuilder::generateBalancedDomains(xCord, yCord, zCord, weights, haloSize, redistTolerance, bounding, world);
    }

    distribute(pData, lData, pMapper, lMapper);
}
