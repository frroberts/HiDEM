//
// Created by robertse on 25/12/22.
//

#ifndef PHELL_LINKACCEL_H
#define PHELL_LINKACCEL_H

#include <ManVec.h>

class LinkAccel : public ManagedParent {
    //
public:
    ManVec<pInd_t> particleLinkId; // n particles * lData.allocatedMaxLinks
    ManVec<pInd_t> otherPartLink; // n particles * lData.allocatedMaxLinks
    ManVec<int> linksPerParticle; // n particles

    int numPraticlesAllocated;
        DEVCALLABLE
    lInd_t getPartLinkId(int link, pInd_t pInd){
        return particleLinkId[pInd + (link * numPraticlesAllocated)];
        // return particleLinkId[link +  (pInd * maxLinks)];
    }

    DEVCALLABLE
    void setPartLinkId(int link, pInd_t pInd, lInd_t val){
        particleLinkId[pInd + (link * numPraticlesAllocated)] = val;
        // particleLinkId[link +  (pInd * maxLinks)] = val;
    }

public:
    // todo untested
    void generateLinks(ParticleData &pData, LinkData &lData) {
        //linksPerParticle.clear();
        linksPerParticle.resize(pData.numLocal);
        numPraticlesAllocated = pData.numLocal;
        particleLinkId.resize(lData.allocatedMaxLinks*numPraticlesAllocated);

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(pInd_t i, LinkAccel &lAccel) {
            lAccel.linksPerParticle[i] = 0;
        }, pData.numLocal, *this);

        Iterator::syncAll();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(lInd_t i, LinkData &lData, LinkAccel &lAccel) {
            if (lData.state[i] == 0)
                return;
            pInd_t pIndA = lData.localPartIndA[i];
            pInd_t pIndB = lData.localPartIndB[i];

            // check both particles are actually found
            lInd_t lPIndA;
            lInd_t lPIndB;
            lPIndA = Iterator::atomicIncVal(lAccel.linksPerParticle[pIndA]);
            lPIndB = Iterator::atomicIncVal(lAccel.linksPerParticle[pIndB]);

            if (lPIndA > lData.allocatedMaxLinks || lPIndB > lData.allocatedMaxLinks) {
                printf("adding more than %d links", lData.allocatedMaxLinks);
#ifdef SET_LOCAL_ID_64
                printf(" %lli %lli ", pIndA, lPIndA);
                printf(" %lli %lli ", pIndB, lPIndB);
#else
                printf(" %i %i ", pIndA, lPIndA);
                printf(" %i %i ", pIndB, lPIndB);
#endif
                //raise(SIGABRT);
            }
            lAccel.setPartLinkId(lPIndA, pIndA, i); // should be in accel
            lAccel.setPartLinkId(lPIndB, pIndB, i); // should be in accel
            // lData.particleLinkIds[lPIndA][pIndA] = i;
            // lData.particleLinkIds[lPIndB][pIndB] = i;
        }, lData.numLocal, lData, *this);

        otherPartLink.resize(pData.numLocal * lData.allocatedMaxLinks);

        pData.iterateAllParticles(ParallelExec(), [&](pInd_t pInd) {
            int particleLinkCount = linksPerParticle[pInd];
            for (size_t i = 0; i < particleLinkCount; ++i) {
                lInd_t lInd = getPartLinkId(i, pInd);
                // should this not be localPartIndB, i.e the other particle?
                pInd_t pIndOther = lData.localPartIndB[lInd];
                if (pIndOther == pInd) {
                    pIndOther = lData.localPartIndA[lInd];
                }
                // todo layout here
                otherPartLink[(numPraticlesAllocated* i) + pInd] = pIndOther;
            }
        });
    }
};

#endif //PHELL_LINKACCEL_H
