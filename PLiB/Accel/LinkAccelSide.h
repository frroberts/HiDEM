//
// Created by robertse on 26/12/22.
//

#ifndef PHELL_HIDEMLINKACCEL_H
#define PHELL_HIDEMLINKACCEL_H

#include "LinkAccel.h"
#include <Global.h>
#include <ParticleData.h>
#include <LinkData.h>

class LinkAccelSide : public LinkAccel {

public:
    ManVec<pInd_t> linkSideLind;

    void generateLinks(ParticleData &pData, LinkData &lData){
            //linksPerParticle.clear();
        linksPerParticle.resize(pData.numLocal);
        numPraticlesAllocated = pData.numLocal;
        particleLinkId.resize(lData.allocatedMaxLinks*numPraticlesAllocated);
        linkSideLind.resize(lData.allocatedMaxLinks*numPraticlesAllocated);

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (pInd_t i, LinkAccel &lAccel) {
            lAccel.linksPerParticle[i] = 0;
        }, pData.numLocal, *this);

        Iterator::syncAll();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (lInd_t i, LinkData &lData, LinkAccel &lAccel) {
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

        pData.iterateAllParticles(OffloadExec(), [=] DEVCALLABLE (pInd_t pInd, LinkData &lData, LinkAccelSide &accel) {
            int particleLinkCount = accel.linksPerParticle[pInd];
            for (size_t i = 0; i < particleLinkCount; ++i) {
                lInd_t lInd = accel.getPartLinkId(i, pInd);
                // should this not be localPartIndB, i.e the other particle?
                pInd_t pIndOther = lData.localPartIndB[lInd];
                lInd_t sideInd;
                if (pIndOther == pInd) {
                    pIndOther = lData.localPartIndA[lInd];
                    sideInd = lData.sideIndEndB[lInd];

                } else {
                    sideInd = lData.sideIndEndA[lInd];
                }
                // todo layout here
                accel.otherPartLink[(accel.numPraticlesAllocated* i) + pInd] = pIndOther;
                accel.linkSideLind[(accel.numPraticlesAllocated* i) + pInd] = sideInd;
            }
        }, lData, *this);
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


};


#endif //PHELL_HIDEMLINKACCEL_H
