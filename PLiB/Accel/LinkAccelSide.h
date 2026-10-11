//
// Created by robertse on 26/12/22.
//

#ifndef PHELL_HIDEMLINKACCEL_H
#define PHELL_HIDEMLINKACCEL_H

#include "LinkAccel.h"
#include <Global.h>
#include <ParticleData.h>
#include <LinkData.h>

class LinkAccelSide : public ManagedParent {

public:
    // per particles what links they have
    ManVec<pInd_t> particleLinkId; // n particles * lData.allocatedMaxLinks
    // number of actual links per particle
    ManVec<int> linksPerParticle; // n particles

    // list of what the other particle of the link is
    ManVec<pInd_t> otherPartLink; // n particles * lData.allocatedMaxLinks
    // list of what side index is associated with the current link end
    ManVec<pInd_t> linkSideLind;

    bool keepInactive = false;

    int numPraticlesAllocated;
    int allocatedMaxLinks;

        DEVCALLABLE
    lInd_t getPartLinkId(int link, pInd_t pInd){
        return particleLinkId[pInd + (link * numPraticlesAllocated)];
    }

    DEVCALLABLE
    void setPartLinkId(int link, pInd_t pInd, lInd_t val){
        particleLinkId[pInd + (link * numPraticlesAllocated)] = val;
    }

    void generatePerParticleIndexes(ParticleData &pData, LinkData &lData) {
        otherPartLink.resize(pData.numLocal * allocatedMaxLinks);
        linkSideLind.resize(allocatedMaxLinks * numPraticlesAllocated);

        pData.iterateAllParticles(OffloadExec(), [=] DEVCALLABLE (pInd_t pInd, LinkData &lData, LinkAccelSide &lAccel) {
            int particleLinkCount = lAccel.linksPerParticle[pInd];
            for (size_t i = 0; i < particleLinkCount; ++i) {
                lInd_t lInd = lAccel.getPartLinkId(i, pInd);
                //if (!lAccel.keepInactive && lData.state[lInd] == 0)
                //    continue;
                pInd_t pIndOther = lData.localPartIndB[lInd];
                lInd_t sideInd;
                if (pIndOther == pInd) {
                    pIndOther = lData.localPartIndA[lInd];
                    sideInd = lData.sideIndEndB[lInd];

                } else {
                    sideInd = lData.sideIndEndA[lInd];
                }
                // todo layout here
                lAccel.otherPartLink[(lAccel.numPraticlesAllocated* i) + pInd] = pIndOther;
                lAccel.linkSideLind[(lAccel.numPraticlesAllocated* i) + pInd] = sideInd;
            }
        }, lData, *this);
        Iterator::syncAll();
    }

    int getMaxLinksPerParticle( LinkData &lData, pInd_t numParticles) {
        linksPerParticle.resize(numParticles);
        auto linksPerParticlePtr = linksPerParticle.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (pInd_t i) {
            linksPerParticlePtr[i] = 0;
        }, numParticles);
        auto *localPartIndAPtr = lData.localPartIndA.data();
        auto *localPartIndBPtr = lData.localPartIndB.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (lInd_t i) {
            pInd_t pIndA = localPartIndAPtr[i];
            pInd_t pIndB = localPartIndBPtr[i];
            Atomic::atomicIncVal(linksPerParticlePtr[pIndA]);
            Atomic::atomicIncVal(linksPerParticlePtr[pIndB]);
        },lData.numLocal);
        Iterator::syncAll();
        auto elem = stx::max_element(EXECPOLICY linksPerParticle.begin(), linksPerParticle.end() );
        return *elem;
    }


    void generateLinks(ParticleData &pData, LinkData &lData) {
        allocatedMaxLinks = getMaxLinksPerParticle(lData, pData.numLocal);
        linksPerParticle.resize(pData.numLocal);
        numPraticlesAllocated = pData.numLocal;
        particleLinkId.resize(allocatedMaxLinks * numPraticlesAllocated);

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (pInd_t i, LinkAccelSide &lAccel) {
            lAccel.linksPerParticle[i] = 0;
        }, pData.numLocal, *this);

        Iterator::syncAll();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (lInd_t i, LinkData &lData, LinkAccelSide &lAccel) {
            // causes issues on LUMI, not quite sure why ?
            // if (!lAccel.keepInactive && lData.state[i] == 0)
            //     return;

            pInd_t pIndA = lData.localPartIndA[i];
            pInd_t pIndB = lData.localPartIndB[i];

            // check both particles are actually found
            lInd_t lPIndA;
            lInd_t lPIndB;
            lPIndA = Atomic::atomicIncVal(lAccel.linksPerParticle[pIndA]);
            lPIndB = Atomic::atomicIncVal(lAccel.linksPerParticle[pIndB]);

            assert (lPIndA < lAccel.allocatedMaxLinks && lPIndB < lAccel.allocatedMaxLinks);

            lAccel.setPartLinkId(lPIndA, pIndA, i);
            lAccel.setPartLinkId(lPIndB, pIndB, i);
        }, lData.numLocal, lData, *this);

        Iterator::iterate(OffloadExec(), [] DEVCALLABLE(pInd_t pInd, LinkData &lData, LinkAccelSide &lAccel) {
            int numP = lAccel.linksPerParticle[pInd];
            if (numP < 2)
                return;
            // in place bubblesort so that i can easily offload it
            for (int i = 0; i < numP; i++) {
                for (int j = i + 1; j < numP; j++) {
                    if (lAccel.getPartLinkId(j, pInd) < lAccel.getPartLinkId(i, pInd)) {
                        auto temp = lAccel.getPartLinkId(i, pInd);
                        lAccel.setPartLinkId(i, pInd, lAccel.getPartLinkId(j, pInd));
                        lAccel.setPartLinkId(j, pInd, temp);
                    }
                }
            }
        }, numPraticlesAllocated, lData, *this);

        Iterator::syncAll();

        generatePerParticleIndexes(pData, lData);
    }

    template<typename Fun, class... Args>
    DEVCALLABLE
    void iterateParticleLinksCT(Fun fun, pInd_t pInd, Args &... args) {
        //
        int particleLinkCount = linksPerParticle[pInd];
        for (size_t i = 0; i < particleLinkCount; ++i) {
            lInd_t lInd = getPartLinkId(i, pInd);
            // should this not be localPartIndB, i.e the other particle?
            pInd_t pIndOther = otherPartLink[(numPraticlesAllocated* i) + pInd];
            lInd_t sideInd = linkSideLind[(numPraticlesAllocated* i) + pInd];
            fun(lInd, sideInd, pIndOther, args...);
        }
    }

    template<typename LData>
    void sortSideData(ParticleData &pData, LData &lData) {

        RangeV2 r5("sort link side data", 3);
        ind_t numParticles = pData.numLocal;

        ManVec<lInd_t> newOrder;
        newOrder.resize(lData.numLocal * 2);
#if defined(SOA_CT)

    #warning "compiling SOA_CT"
        ind_t orderInd = 0;

        for (int i = 0; i < allocatedMaxLinks; ++i) {
            for (int pInd = 0; pInd < numParticles; ++pInd) {
                int numLinks = linksPerParticle[pInd];
                if (i >= numLinks)
                    continue;

                lInd_t lInd = getPartLinkId(i, pInd);
                lInd_t ctInd;
                if(lData.localPartIndA[lInd] == pInd)
                    ctInd = lData.sideIndEndA[lInd];
                else {
                    ctInd = lData.sideIndEndB[lInd];
                }
                newOrder[orderInd] = ctInd;
                orderInd++;
            }
        }

#elif defined(AOSOA_CT)
#warning "compiling AOSOA_CT"

        int numInBlock = 64;
        int numBlocks = 1 + (numParticles / numInBlock);


        ManVec<lInd_t> blockSizes;
        ManVec<lInd_t> blockStarts;
        blockSizes.resize(numBlocks, false);
        blockStarts.resize(numBlocks, false);
        auto ptr = blockSizes.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i) {
            ptr[i] = 0;
        }, numBlocks);

        // get active in block
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int b, LinkAccelSide &lAccel) {
            int c = 0;
            for (int l = 0; l < lAccel.allocatedMaxLinks; l++) {
                for (size_t i = 0; i < numInBlock; i++) {
                    int blockStart = (b * numInBlock);
                    if (blockStart + i >= numParticles)
                        continue;
                    pInd_t pInd = blockStart + i;
                    if (l >= lAccel.linksPerParticle[pInd])
                        continue;
                    c++;
                }
            }
            ptr[b] = c;
        }, numBlocks, *this);

        // running sum of sizes to get block starts

        ParallelScan::exclusive(blockSizes, blockStarts);

        // for each block run all the links, one thread per block, its not the best but better than serial?
        auto newOrderPtr = newOrder.data();
        auto blockStartPtr = blockStarts.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int b, LData &lData, LinkAccelSide &lAccel) {
            int c = blockStartPtr[b]; // get block start
            for (int l = 0; l < lAccel.allocatedMaxLinks; l++) {
                for (size_t i = 0; i < numInBlock; i++) {
                    int blockStart = (b * numInBlock);
                    if (blockStart + i >= numParticles)
                        continue;

                    pInd_t pInd = blockStart + i;
                    if (l >= lAccel.linksPerParticle[pInd])
                        continue;

                    lInd_t lInd = lAccel.getPartLinkId(l, pInd);
                    lInd_t sideInd;
                    if (lData.localPartIndA[lInd] == pInd)
                        sideInd = lData.sideIndEndA[lInd];
                    else {
                        sideInd = lData.sideIndEndB[lInd];
                    }
                    newOrderPtr[c] = sideInd;
                    c++;
                }
            }
        }, numBlocks, lData, *this);
        Iterator::syncAll();
#else

    #warning "compiling AOS_CT"

    // can be done with a scan to get the individual particle start ?
    // and then the particles can be done in parallel at least ?

    ManVec<lInd_t> particleSarts;
    particleSarts.resize(numParticles + 1, false);
    particleSarts[0] = 0;

    stx::inclusive_scan(EXECPOLICY this->linksPerParticle.begin(), this->linksPerParticle.end(), particleSarts.begin() + 1);

    auto pStartPtr = particleSarts.data();
    auto newOrderPtr = newOrder.data();

    Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(ind_t pInd, LData &lData, LinkAccelSide &accel){
        //
        lInd_t orderInd = pStartPtr[pInd];
        for (int i = 0; i < accel.allocatedMaxLinks; ++i) {
            int numLinks = accel.linksPerParticle[pInd];
            if (i >= numLinks)
                continue;
            lInd_t lInd = accel.getPartLinkId(i,pInd);
            lInd_t ctInd;
            if (lData.localPartIndA[lInd] == pInd)
                ctInd = lData.sideIndEndA[lInd];
            else {
                ctInd = lData.sideIndEndB[lInd];
            }
            newOrderPtr[orderInd] = ctInd;
            orderInd++;
        }
    }, numParticles, lData, *this);
    Iterator::syncAll();


#endif
        Iterator::syncAll();

        lData.reorderSideData(newOrder);

        // this could be a reorder as well but not sure its much faster
        generateLinks(pData, lData);
    }
};


#endif //PHELL_HIDEMLINKACCEL_H
