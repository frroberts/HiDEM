//
// Created by Fredrik Robertsen on 22/11/22.
//

#ifndef PHELL_NEIGHACCEL_H
#define PHELL_NEIGHACCEL_H


#include "../Global.h"
#include "../ParticleData.h"
#include "../NeighbourGrid.h"
#include "../Util/Layout.h"


//template<class Layout>
class NeighAccel : public ManagedParent {

public:
    //Layout layout;

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
        // todo needs to be size_t, fix reducer at some point
        ParallelReduce<int> reducer;
        int sum = 0;
        int max = 0;
        int min = 0;
        sum = reducer.sum(numNeighs);
        reducer.minMax(min, max, numNeighs);
        listStarts.resize(pData.numLocal);
        sum = pData.numLocal * max;

        maxNeighbours = max;

#ifdef SOA_NLIST
        Iterator::iterate(OffloadExec(),[] DEVCALLABLE (ind_t i, NeighAccel &nList){
                    nList.listStarts[i] = i;
        }, pData.numLocal, *this);
#else
        //ParallelScan scanner;
        //scanner.exclusive(numNeighs, listStarts);
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(pInd_t i, NeighAccel &nList) {
            nList.listStarts[i] = i * max;
        }, pData.numLocal, *this);
#endif

        list.resize(sum);

        numPraticlesAllocated = pData.numLocal;

        interateBlocks(grid, pData, interactDist, [] DEVCALLABLE (pInd_t pInd, pInd_t nInd, NeighAccel &nList, int &neighbours) {
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
#ifdef SOA_NLIST
        target = pInd + (n * numPraticlesAllocated);
#else
        target = n + (pInd * maxNeighbours);
#endif
        return list[target];
    }

    DEVCALLABLE
    void setPartNeighId(int n, pInd_t pInd, pInd_t val) {
        size_t target;
#ifdef SOA_NLIST
        target = pInd + (n * numPraticlesAllocated);
#else
        target = n + (pInd * maxNeighbours);
#endif
        list[target] = val;
    }

    void generateNumNeigh(double searchDistance2, NeighbourGrid &grid, ParticleData &pData, double interactDist) {
        numNeighs.resize(pData.numLocal);


        Iterator::iterate(OffloadExec(), [] DEVCALLABLE(pInd_t i, NeighAccel &nList) {
            nList.numNeighs[i] = 0;
        }, pData.numLocal, *this);

        interateBlocks(grid, pData, interactDist, [] DEVCALLABLE (pInd_t pInd, pInd_t nInd, NeighAccel &nList, int &neighbours) {
            neighbours++;
        });
    }

    template<class Fun>
    void interateBlocks(NeighbourGrid &grid, ParticleData &pData, double interactDist, Fun nFunc) {
        pData.iterateOwnedParticles(OffloadExec(), [=] DEVCALLABLE(pInd_t pInd, ParticleData &pData, NeighbourGrid &grid, NeighAccel &nList) {
            //
            int neighbours = 0;

            int color = grid.colours[pInd];

            double xCord = pData.xCord[pInd];
            double yCord = pData.yCord[pInd];
            double zCord = pData.zCord[pInd];

            for (int n = 0; n < 27; ++n) {
                int targetBucket = grid.targetBuckets[(color) + (n * grid.totalCells)];
                // skip if the bucket does not exist, i.e. if the target bucket is -1
                if (targetBucket == -1)
                    continue;
                ind_t i = grid.bucketStarts[targetBucket];
                ind_t end = i + grid.bucketSizes[targetBucket];
                for (; i < end; ++i) {

                    pInd_t nInd = grid.iterationOrder[i];

                    if (nInd == pInd) {
                        continue;
                    }

                    double xCordOther = pData.xCord[nInd];
                    double xDist = xCord - xCordOther;
                    double xDist2 = xDist * xDist;

                    double yCordOther = pData.yCord[nInd];
                    double yDist = yCord - yCordOther;
                    double yDist2 = yDist * yDist;

                    double zCordOther = pData.zCord[nInd];
                    double zDist = zCord - zCordOther;
                    double zDist2 = zDist * zDist;

                    double particleDistance = sqrt(xDist2 + yDist2 + zDist2);
                    double radSum = pData.radius[pInd] + pData.radius[nInd];
                    double dist = particleDistance - radSum;

                    if (dist < interactDist) {
                        nFunc(pInd, nInd, nList, neighbours);
                    }
                }
            }


            nList.numNeighs[pInd] = neighbours;
        }, pData, grid, *this);
    }

};

#endif //PHELL_NEIGHACCEL_H
