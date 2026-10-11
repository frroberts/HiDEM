//
// Created by Fredrik Robertsen on 01/11/2020.
//

#ifndef PHELL_LINKGENERATOR_H
#define PHELL_LINKGENERATOR_H

#include <vector>
#include <algorithm>
#include <csignal>
#include <random>

#include "NeighbourGrid.h"
#include "ElementrayLink.h"

#define LKeyOffset 256

// todo abstract link init better

class LinkGenerator {


public:


    ManVec<ElementaryLink>generateLinksDistance(double distScale, double maxRadius, ParticleData &data){
        //
        std::vector<Map<double>*> prob;
        prob.push_back(new MapSingle<double>(1));
        std::vector<std::vector<int>> linkTypes;
        linkTypes.push_back(std::vector<int>());
        linkTypes[0].push_back(0);

        return generateLinksBreakProbMulti(distScale, maxRadius, data, prob, 1, linkTypes);
    }
    //0.5 / 2.0, pData)
    ManVec<ElementaryLink> generateLinksBreakProbMulti(double distScale, double maxRadius, ParticleData &pData,
                                                       std::vector<Map<double>*> &linkGenerateProbabilities,
                                                       int seed, std::vector<std::vector<int>> linkToTypes) {

        // need the max particle radius
        std::mt19937 gen(seed); // Standard mersenne_twister_engine seeded with rd()
        std::uniform_real_distribution<> dis(0.0, 1.0);

        auto *grid = new NeighbourGrid();
        grid->generate(pData, (maxRadius + maxRadius) * distScale);
        ManVec<ElementaryLink> links;

        // todo should parallelize this
        pData.iterateOwnedParticles(SerialExec(), [&](size_t pInd, ParticleData &pData) {

            int colour = grid->getPraticleColor(pInd);
            size_t gKey = pData.globalKeys[pInd] * LKeyOffset;
            std::vector<size_t> toBeLinked;

            int type = pData.type[pInd];

            int numLinked = 0;

            grid->iteratePossibleNeighbours([&](size_t nInd) {

                int nType = pData.type[nInd];

                if (pInd == nInd)
                    return;

                hpReal_t xDist = pData.xCord[nInd] - pData.xCord[pInd];
                hpReal_t yDist = pData.yCord[nInd] - pData.yCord[pInd];
                hpReal_t zDist = pData.zCord[nInd] - pData.zCord[pInd];
                hpReal_t dist = std::sqrt((xDist*xDist)+(yDist*yDist)+(zDist*zDist));
                hpReal_t radiusSum = pData.radius[nInd] + pData.radius[pInd];
                if (dist > radiusSum * distScale)
                    return;

                if (pData.globalKeys[pInd] >= pData.globalKeys[nInd])
                    return;

                auto pos = std::find(linkToTypes[type].begin(), linkToTypes[type].end(), nType);
                if (pos == linkToTypes[type].end())
                    return;

                if (type > linkGenerateProbabilities.size()) {
                    std::cout << "particle type with no break probability found" << std::endl;
                    raise(SIGABRT);
                }
                double prob = linkGenerateProbabilities[type]->getVal(pData.xCord[pInd], pData.yCord[pInd], pData.zCord[pInd]);

                if (dis(gen) > prob)
                    return;


                toBeLinked.push_back(nInd);

                numLinked++;
            }, pInd, colour, true);


            std::sort(toBeLinked.begin(), toBeLinked.end());

            int i = 0;
            for (auto nInd: toBeLinked) {
                // only link to higher global ID particles
                if (pData.globalKeys[pInd] < pData.globalKeys[nInd]) {

                    lpReal_t len;

                    double xCord1 = pData.xCord[pInd];
                    double yCord1 = pData.yCord[pInd];
                    double zCord1 = pData.zCord[pInd];

                    double xCord2 = pData.xCord[nInd];
                    double yCord2 = pData.yCord[nInd];
                    double zCord2 = pData.zCord[nInd];

                    double xDist = xCord1 - xCord2;
                    double xDist2 = xDist * xDist;

                    double yDist = yCord1 - yCord2;
                    double yDist2 = yDist * yDist;

                    double zDist = zCord1 - zCord2;
                    double zDist2 = zDist * zDist;

                    double particleDistance2 = xDist2 + yDist2 + zDist2;

                    len = std::sqrt(particleDistance2);


                    ElementaryLink link;
                    link.globalKeyParticleA = pData.globalKeys[pInd];
                    link.globalKeyParticleB = pData.globalKeys[nInd];
                    link.globalKey = gKey;
                    link.owner = pData.owners[pInd];;
                    link.state = 1;
                    link.particleLinkNum = i;

                    link.initialLen = len;
                    gKey++;
                    links.push_back(link);
                    i++;
                }
            }

        }, pData);

        //stripDuplicates(links);

        delete grid;

        return links;
    }

};


#endif //PHELL_LINKGENERATOR_H
