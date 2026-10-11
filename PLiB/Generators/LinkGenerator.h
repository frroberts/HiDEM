//
// Created by Fredrik Robertsen on 01/11/2020.
//

#ifndef PHELL_LINKGENERATOR_H
#define PHELL_LINKGENERATOR_H

#include <vector>
#include <algorithm>
#include <BulkRandom.h>
#include <csignal>
#include <random>

#include "NeighbourGrid.h"
#include "ElementrayLink.h"
#include "LinkData.h"
#include "Accel/CombinedAccel.h"

#define LKeyOffset 256

// todo abstract link init better

class LinkGenerator : public ManagedParent {
    int addedLinks = 0;

    lpReal_t distScale;
    lpReal_t genDist;

    ManVec<std::pair<int, int> > linkToPairs;

public:
    ManVec<ElementaryLink> generateLinksDistance(double maxRadius, ParticleData &data) {
        //
        std::vector<Map<double> *> prob;
        prob.push_back(new MapSingle<double>(1));
        linkToPairs.push_back(std::make_pair(0, 0));

        return generateLinksBreakProbMulti(maxRadius, data, prob, 1);
    }

    lpReal_t getGenDistance() {
        return 0;
    }

    template <typename  Pol, typename Fun>
    void genNewLinks(Pol policy, ParticleData *pDataPtr, NeighAccel *accel, ManVec<ElementaryLink> &newLinks, int numNeighs, uint8_t state, Fun checkFun) {
        newLinks.resize(numNeighs); // this could be half of this + margin ?
        auto newLinksPtr = newLinks.data();

        RangeV2 r("gen new links", 3);

        int linkToPairsCount = linkToPairs.size();
        auto linkToPairsPtr = linkToPairs.data();

        pDataPtr->iterateOwnedParticles(policy, [=] DEVCALLABLE (pInd_t pInd, LinkGenerator &lGen) {
            //
            int i = -1;
            accel->iteratePossibleNeighbours([&] (pInd_t nInd) {
                i++;

                if (pInd == nInd)
                    return;

                if (pDataPtr->globalKeys[pInd] >= pDataPtr->globalKeys[nInd])
                    return;

                if (checkFun(pInd, nInd, i) != true) {
                    return;
                }

                int nType = pDataPtr->type[nInd];
                int type = pDataPtr->type[pInd];

                bool found = false;
                for (int n = 0; n < linkToPairsCount; n++) {
                    if (linkToPairsPtr[n] == std::make_pair(type, nType)) {
                        found = true;
                    }
                }
                if (!found)
                    return;

                hpReal_t xDist = pDataPtr->xCord[nInd] - pDataPtr->xCord[pInd];
                hpReal_t yDist = pDataPtr->yCord[nInd] - pDataPtr->yCord[pInd];
                hpReal_t zDist = pDataPtr->zCord[nInd] - pDataPtr->zCord[pInd];
                hpReal_t dist = std::sqrt((xDist * xDist) + (yDist * yDist) + (zDist * zDist));
                hpReal_t radiusSum = pDataPtr->radius[nInd] + pDataPtr->radius[pInd];
                if (dist > radiusSum * lGen.distScale)
                    return;

                ElementaryLink link;
                link.globalKeyParticleA = pDataPtr->globalKeys[pInd];
                link.globalKeyParticleB = pDataPtr->globalKeys[nInd];
                link.globalKey = -1;
                link.owner = pDataPtr->owners[pInd];
                link.state = state;
                link.particleLinkNum = i;

                link.initialLen = dist;

                int pos = Atomic::atomicIncVal(lGen.addedLinks); //addedPtr->fetch_add(1);

                newLinksPtr[pos] = link;

            }, pInd);
        }, *this);

        Iterator::syncAll();

        newLinks.resize(addedLinks);
    }


    void setLinkTo(ManVec<std::pair<int, int>> &linkToPairsIn) {
        for (int n = 0; n < linkToPairsIn.size(); n++) {
            this->linkToPairs.push_back(linkToPairsIn[n]);
        }
    }

    //0.5 / 2.0, pData)
    ManVec<ElementaryLink> generateLinksBreakProbMulti(double maxRadius, ParticleData &pData,
                                                       std::vector<Map<double> *> &linkGenerateProbabilities,
                                                       int seed) {

        // need the max particle radius
        std::mt19937 gen(seed); // Standard mersenne_twister_engine seeded with rd()
        std::uniform_real_distribution<> dis(0.0, 1.0);

        BulkRandom<float> randomVals;

        auto *grid = new NeighbourGrid();
        grid->generate(pData, (maxRadius + maxRadius) * distScale);
        auto *nListPtr = new NeighAccel();
        auto &nList = *(nListPtr);

        nList.generateNeighbours(*grid, pData, (maxRadius + maxRadius) * distScale, (maxRadius + maxRadius) * distScale);

        ManVec<ElementaryLink> newLinks;

        newLinks.resize(nList.list.size());

        randomVals.setSize(nList.maxNeighbours*pData.numLocal);
        randomVals.reGenerate();

        auto randomValsPtr = randomVals.values.data();
        int maxNeighs = nList.maxNeighbours;
        auto pDataPtr = &pData;

        ManVec<Map<double> *> genProbabilities;
        genProbabilities.resize(linkGenerateProbabilities.size());
        for (int i = 0; i < linkGenerateProbabilities.size(); i++) {
            genProbabilities[i] = linkGenerateProbabilities[i];
        }
        auto *genProbabilitiesPtr = genProbabilities.data();;

        auto checkFun = [=] DEVCALLABLE (pInd_t pInd, pInd_t nInd, int i) {

            int type = pDataPtr->type[pInd];

            double prob = genProbabilitiesPtr[type]->getVal(pDataPtr->xCord[pInd], pDataPtr->yCord[pInd], pDataPtr->zCord[pInd]);
            if (randomValsPtr[(pInd*maxNeighs)+i] > prob)
                return false;

            return true;
        };

        // not offloaded as maps require inheritance for now and nvidia at least cannot support it on the device
        genNewLinks(ParallelExec(), &pData, nListPtr, newLinks, nList.list.size(), 1, checkFun);

        for (auto &l: newLinks) {
            l.globalKey = l.globalKeyParticleA * LKeyOffset + l.particleLinkNum;
        }
        //stripDuplicates(links);

        // todo sort links, 2 level sort on global particle ids

        delete grid;
        delete nListPtr;

        return newLinks;
    }

    void setDistScale(lpReal_t distScale) {
        this->distScale = distScale;
    }


    ManVec<ElementaryLink> regenerateLinks(ParticleData *pDataPtr, LinkData *lDataPtr, CombinedAccel *accel) {
        ManVec<ElementaryLink> newLinks;

        int numNeighs = accel->nList.list.size();

        addedLinks = 0;

        {
            auto checkFun = [=] DEVCALLABLE (pInd_t pInd, pInd_t nInd, int i) {
                // check direction here
                int particleLinkCount = accel->lAccel.linksPerParticle[pInd];

                for (size_t j = 0; j < particleLinkCount; ++j) {
                    lInd_t lInd = accel->lAccel.getPartLinkId(j, pInd);
                    pInd_t pIndOther = accel->lAccel.otherPartLink[
                        (accel->lAccel.numPraticlesAllocated * j) + pInd];

                    if (pIndOther == nInd && lDataPtr->state[lInd] == 1) {
                        return false;
                        // break;
                    }
                }
                return true;
            };


            genNewLinks(OffloadExec(), pDataPtr, &accel->nList, newLinks, numNeighs, 0, checkFun);
        } {
            auto newLinksPtr = newLinks.data();
            RangeV2 r("sync new links", 3);

            MPI_Barrier(MPI_COMM_WORLD);

            gKey_t gKeyLocalOffset = 0;
            gKey_t gKeyGlobalMax;

            // track this better
            gKey_t gKeyLocalMax = *stx::max_element(EXECPOLICY lDataPtr->globalKeys.begin(), lDataPtr->globalKeys.end());//gKeyReducer.max(lDataPtr->globalKeys);

            gKey_t newKeyCount = newLinks.size();

            MPI_Allreduce(&gKeyLocalMax, &gKeyGlobalMax, 1, MPI_LONG_LONG_INT, MPI_MAX, MPI_COMM_WORLD);
            MPI_Exscan(&newKeyCount, &gKeyLocalOffset, 1, MPI_LONG_LONG_INT, MPI_SUM, MPI_COMM_WORLD);

            Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i) {
                newLinksPtr[i].globalKey = 1 + (gKeyGlobalMax + gKeyLocalOffset + i);
            }, newKeyCount);

            MPI_Barrier(MPI_COMM_WORLD);
        }
        return newLinks;
    }
};


#endif //PHELL_LINKGENERATOR_H
