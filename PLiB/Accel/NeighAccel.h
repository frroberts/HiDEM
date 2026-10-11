//
// Created by Fredrik Robertsen on 22/11/22.
//

#ifndef PHELL_NEIGHACCEL_H
#define PHELL_NEIGHACCEL_H


#include "../Global.h"
#include "../ParticleData.h"
#include "../NeighbourGrid.h"
#include "../Util/Layout.h"

class NeighAccel : public ManagedParent {

public:

    size_t maxNeighbours = 0;
    ManVec<pInd_t> list; // nParticles*maxNeighbours
    ManVec<int> numNeighs; // needed ?
    ManVec<size_t> listStarts; // needed ?

    size_t numPraticlesAllocated;

    void generateNeighbours(NeighbourGrid &grid, ParticleData &pData, double searchDistance, double interactDist) {

        double searchDistance2 = searchDistance * searchDistance;

        Iterator::syncAll();

        generateNumNeigh(searchDistance2, grid, pData, interactDist);

        Iterator::syncAll();
        int max = 0;
        int min = 0;
        ParallelReduce<int>::minMax(min, max, numNeighs);
        listStarts.resize(pData.numLocal);
        size_t sum = 0;
        sum = pData.numLocal * max;

        maxNeighbours = max;

        Iterator::iterate(OffloadExec(),[] DEVCALLABLE (ind_t i, NeighAccel &nList){
            nList.listStarts[i] = i;
        }, pData.numLocal, *this);

        list.resize(sum);

        numPraticlesAllocated = pData.numLocal;

        iterateBlocks(grid, pData, interactDist, [] DEVCALLABLE (pInd_t pInd, pInd_t nInd, NeighAccel &nList, int &neighbours) {
            nList.setPartNeighId(neighbours, pInd, nInd);
            neighbours++;
        });

        Iterator::syncAll();
    }

    template<typename Fun, class... Args>
    DEVCALLABLE
    void iteratePossibleNeighbours(Fun fun, pInd_t pInd, Args& ... args)
    {
        int end = numNeighs[pInd];
        for (int i = 0; i < end; ++i) {
            pInd_t nInd = getPartNeighId(i, pInd);
            fun(nInd,  args...);
        }
    }

    // these should be private but ... cuda...
    DEVCALLABLE
    pInd_t getPartNeighId(int n, pInd_t pInd) {
        size_t target;
        target = pInd + (n * numPraticlesAllocated);
        return list[target];
    }

    DEVCALLABLE
    void setPartNeighId(int n, pInd_t pInd, pInd_t val) {
        size_t target;
        target = pInd + (n * numPraticlesAllocated);
        list[target] = val;
    }

    void generateNumNeigh(double searchDistance2, NeighbourGrid &grid, ParticleData &pData, double interactDist) {
        numNeighs.resize(pData.numLocal);

        Iterator::iterate(OffloadExec(), [] DEVCALLABLE(pInd_t i, NeighAccel &nList) {
            nList.numNeighs[i] = 0;
        }, pData.numLocal, *this);

        iterateBlocks(grid, pData, interactDist, [] DEVCALLABLE (pInd_t pInd, pInd_t nInd, NeighAccel &nList, int &neighbours) {
            neighbours++;
        });
    }

    template<class Fun>
    void iterateBlocks(NeighbourGrid &grid, ParticleData &pData, double interactDist, Fun nFunc) {
        double interactDist2 = interactDist*interactDist;

        pData.iterateOwnedParticles(OffloadExec(), [=] DEVCALLABLE (pInd_t pInd, ParticleData &pData, NeighbourGrid &grid, NeighAccel &nList) {
            //
            int neighbours = 0;

            int64_t color = grid.colours[pInd];

            hpReal_t xCord = pData.xCord[pInd];
            hpReal_t yCord = pData.yCord[pInd];
            hpReal_t zCord = pData.zCord[pInd];

            grid.iteratePossibleNeighbours([&](size_t nInd) {
                hpReal_t xCordOther = pData.xCord[nInd];
                lpReal_t xDist = xCord - xCordOther;
                lpReal_t xDist2 = xDist * xDist;

                hpReal_t yCordOther = pData.yCord[nInd];
                lpReal_t yDist = yCord - yCordOther;
                lpReal_t yDist2 = yDist * yDist;

                hpReal_t zCordOther = pData.zCord[nInd];
                lpReal_t zDist = zCord - zCordOther;
                lpReal_t zDist2 = zDist * zDist;

                lpReal_t particleDistance2 = xDist2 + yDist2 + zDist2;
                lpReal_t particleDistance = sqrt(particleDistance2);
                lpReal_t radSum = pData.radius[pInd] + pData.radius[nInd];
                lpReal_t dist = particleDistance - radSum;

                if (dist < interactDist) {
                    nFunc(pInd, nInd, nList, neighbours);
                }
            }, pInd, color, true);


            nList.numNeighs[pInd] = neighbours;
        }, pData, grid, *this);
    }

};

#endif //PHELL_NEIGHACCEL_H
