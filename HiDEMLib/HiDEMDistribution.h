//
// Created by Fredrik Robertsen on 24/01/2021.
//

#ifndef PHELL_DISTRIBUTION_H
#define PHELL_DISTRIBUTION_H

/*
 *    //
// Created by Fredrik Robertsen on 24/01/2021.
//

#include <random>

#include "HiDEMDistribution.h"
#include "../PLiB/DomainBuilder.h"

#include <../PLiB/Logger/Logger.h>

#include <Transport/LinkTransporter.h>
#include <Transport/ParticleTransporter.h>

#include "LinkDataHandler.h"

 */
#include <memory>
#include <World.h>
#include "HiDEMParticleData.h"
#include "HiDEMLinkData.h"
#include <Transport/Transporter.h>
#include <Geometry/Map2d.h>
#include <NeighbourGrid.h>
#include <Transport/Communicator.h>
#include <Transport/CommunicatorPacked.h>
#include <Transport/LinkTransporter.h>

#include "LinkDataHandler.h"
#include "Transport/ParticleTransporter.h"

#define maxBB 1000

template<typename PDataT, typename LDataT>
class HiDEMDistribution : public ManagedParent {
    //
public:
    CommunicatorPacked<ctReal_t, 6> lCTComm;
    CommunicatorPacked<lpReal_t, 6> pForceComm;
    CommunicatorPacked<uint8_t, 1> linkStateComm;

    ParticleTrasporterV2<PDataT> *pTrans;
    LinkTransporterV2<PDataT, LDataT> *lTrans;

    bool reBalanceActive;

    ManVec<double> xCord;
    ManVec<double> yCord;
    ManVec<double> zCord;
    ManVec<double> weights;

    NeighbourGrid *grids[4];
    bool allocated = false;

public:
    World world;

    HiDEMDistribution(const HiDEMDistribution &old_obj) = delete;

    HiDEMDistribution(HiDEMDistribution &&) = delete;

    HiDEMDistribution() = default;

    ~HiDEMDistribution() {
        if (allocated) {
            for (int i = 0; i < 4; ++i) {
                delete grids[i];
            }
        }
    };

    void syncCT(LDataT &lData, GlobalLocalMapper<gKey_t, lInd_t> &lMapper) {
        //nvtxRangePush("CT sync links");
        // syncAll up CT from all halos

        lCTComm.initFromTransporter(*lTrans);

        lCTComm.invert();
        lCTComm.tagStart = 64 + 12 + 12;
        lCTComm.setupBuffers();

        lCTComm.makeLocalIndexes(lMapper);
        lCTComm.indirectIndexes(lData.sideIndEndB, lData.sideIndEndB);

        lCTComm.packAllCommData(packVecPtrs(lData.CT0_flat, lData.CT1_flat, lData.CT2_flat, lData.CT3_flat, lData.CT4_flat, lData.CT5_flat));

        Iterator::syncAll();

        lCTComm.postRecvs();
        lCTComm.postSends();
        lCTComm.syncRecvs();
        lCTComm.syncSends();

        lCTComm.unpackAllCommData(packVecPtrs(lData.CT0_flat, lData.CT1_flat, lData.CT2_flat, lData.CT3_flat, lData.CT4_flat, lData.CT5_flat));
        Iterator::syncAll();
        //nvtxRangePop();
    }

    HiDEMDistribution(bool reBalance, double searchDistance) : reBalanceActive(reBalance) {
        for (int i = 0; i < 4; ++i) {
            grids[i] = new NeighbourGrid();
        }
        grids[0]->gridSpacing = searchDistance;
        grids[1]->gridSpacing = searchDistance * 2;
        grids[2]->gridSpacing = searchDistance * 4;
        grids[3]->gridSpacing = searchDistance * 8;
        allocated = true;

        pTrans = new ParticleTrasporterV2<PDataT>();
        lTrans = new LinkTransporterV2<PDataT, LDataT>();
    }

    //
    void distribute(PDataT &pData, LDataT &lData, GlobalLocalMapper<gKey_t, pInd_t> &pMapper, GlobalLocalMapper<gKey_t, lInd_t> &lMapper) {
        RangeV2 r("distribution step", 3);

        pTrans->clear();

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
        ManVec<pInd_t> reOrder;
        reOrder.resize(pData.numLocal);

        auto reOrderPtr = reOrder.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (pInd_t i) {
            reOrderPtr[i] = i;
        }, pData.numLocal);
        Iterator::syncAll();

        ParallelSort::sortIndirectNLevel(reOrder, packVecPtrs(grids[3]->colours, grids[2]->colours, grids[1]->colours, grids[0]->colours));
        r5.end();
        RangeV2 r6("link sort", 3);

        pData.reorder(reOrder);
        pData.remap(pMapper);

        auto newLOrder = lData.getNewOrder();
        lData.reorder(newLOrder);

        lData.reorderLinkTargets(reOrder);

        // should not need this ? i could just reindex from the reorder ?
        lData.remapHalos(lMapper, pData);


        r6.end();

        RangeV2 r7("make communicator", 3);

        pForceComm.initFromTransporter(*pTrans);
        pForceComm.tagStart = 64;
        pForceComm.setupBuffers();
        pForceComm.makeLocalIndexes(pMapper);


        linkStateComm.initFromTransporter(*lTrans);
        linkStateComm.tagStart = 128;
        linkStateComm.setupBuffers();
        linkStateComm.makeLocalIndexes(lMapper);

        // pForceComm.makePreCommIndexes(pData, pMapper);
        r7.end();
    }

    void reDistribute(PDataT &pData, LDataT &lData, GlobalLocalMapper<gKey_t, pInd_t> &pMapper, GlobalLocalMapper<gKey_t, lInd_t> &lMapper,
                      double haloSize, double redistTolerance) {
        if (reBalanceActive) {
            int numOwned = pData.ownedIndexes.size();

            xCord.resize(numOwned);
            yCord.resize(numOwned);
            zCord.resize(numOwned);
            weights.resize(numOwned);

            double *xCordPtr = xCord.data();
            double *yCordPtr = yCord.data();
            double *zCordPtr = zCord.data();
            double *weightsPtr = weights.data();

            Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (ind_t i, HiDEMParticleData &pData, LDataT &lData, HiDEMDistribution &dist) {
                pInd_t pInd = pData.ownedIndexes[i];

                //double weight = 1 + pData.iteratedNeighbours[pInd];
                //weight += 1 + pData.iteratedLinks[pInd];
                double weight = 1;

                xCordPtr[i] = pData.xCord[pInd];
                yCordPtr[i] = pData.yCord[pInd];
                zCordPtr[i] = pData.zCord[pInd];
                weightsPtr[i] = weight;
            }, numOwned, pData, lData, *this);

            Iterator::syncAll();

            DomainBuilder::generateBalancedDomains(xCord, yCord, zCord, weights, haloSize, redistTolerance, world.getOwnDomain(), world);
        }

        distribute(pData, lData, pMapper, lMapper);
    }
};


#endif //PHELL_DISTRIBUTION_H
