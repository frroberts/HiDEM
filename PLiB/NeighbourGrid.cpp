//
// Created by Fredrik Robertsen on 09/10/2020.
//
#include <algorithm>
#include <functional>
#include <numeric>
#include <cassert>
#include <random>

#include "NeighbourGrid.h"
#include "../Iterators/Iterator.h"
#include "Logger/Logger.h"

#include "Util/Util.h"
#include "Global.h"

#ifdef GPU
#include "DevPortabilityLayer.h"
#endif

DEVCALLABLE
int NeighbourGrid::getGridColor(int x, int y, int z) {
    return ((x))+((y) * xGridSize) + ((z) * xGridSize * yGridSize);
}

DEVCALLABLE
std::tuple<double, double, double> NeighbourGrid::getGridId(double x, double y, double z) {
    return std::make_tuple((int)std::floor((x - xMin) / gridSpacing), (int)std::floor((y - yMin) / gridSpacing), (int)std::floor((z - zMin) / gridSpacing));
}

DEVCALLABLE
int NeighbourGrid::getPraticleColor(pInd_t pInd)
{
    return colours[pInd];
}

std::tuple<double, double, double> NeighbourGrid::getGridSize(double x, double y, double z) {
    //
    //std::cout << x << " " << y << " " << z << std::endl;
    int xGridS = 1 + (int)std::floor((x) / gridSpacing);
    int yGridS = 1 + (int)std::floor((y) / gridSpacing);
    int zGridS = 1 + (int)std::floor((z) / gridSpacing);

    //int blockSize = 4;
    // pad for block size
    //xGridSize += blockSize + (xGridSize % blockSize);
    //yGridSize += blockSize + (yGridSize % blockSize);
    //zGridSize += blockSize + (zGridSize % blockSize);

    return std::make_tuple(xGridS, yGridS, zGridS);
}

void NeighbourGrid::generateNeighbourGrid(ParticleData &data)
{
    //

    auto [_xMax, _yMax, _zMax, _xMin, _yMin, _zMin] = data.getLimits();

    xMax = _xMax;
    yMax = _yMax;
    zMax = _zMax;
    xMin = _xMin;
    yMin = _yMin;
    zMin = _zMin;
    //std::cout << xMax << " " << yMax << " " << zMax << " ";
    //std::cout << xMin << " " << yMin << " " << zMin << std::endl;

    auto [_xGridSize, _yGridSize, _zGridSize] = getGridSize(xMax-xMin, yMax-yMin, zMax-zMin);
    xGridSize = _xGridSize;
    yGridSize = _yGridSize;
    zGridSize = _zGridSize;

    int prevTotalCells = totalCells;
    totalCells = xGridSize*yGridSize*zGridSize;
    if ((prevTotalCells != 0) && ((totalCells/10) > prevTotalCells)) {
        std::cout << getLocalRank() << " new (" << totalCells << ") grid 10x larger than old (" << prevTotalCells << ") , did something explode ?" << std::endl;
    }
    // if we get an empty domain set num cells to 0 to avoid crashing
    if(data.numLocal == 0)
        totalCells = 0;
    //std::cout << _xGridSize << " " << _yGridSize << " " << _zGridSize << " " << totalCells << std::endl;

    targetBuckets.resize(totalCells*27, false);
    numTargetBuckets.resize(totalCells, false);


    Iterator::iterate3D(OffloadExec(), [] DEVCALLABLE (int x, int y, int z, NeighbourGrid &grid) {
        int gridColor = grid.getGridColor(x, y, z);

        int i = 0;
        for (int zD = -1; zD < 2; zD++) {
            for (int yD = -1; yD < 2; yD++) {
                for (int xD = -1; xD < 2; xD++) {
                    int xT;
                    int yT;
                    int zT;
                    xT = x + xD;
                    yT = y + yD;
                    zT = z + zD;
                    if (xT >= 0 && xT < grid.xGridSize && yT >= 0 && yT < grid.yGridSize && zT >= 0 && zT < grid.zGridSize) {
                        int targetBucket = grid.getGridColor(xT, yT, zT);
                        grid.targetBuckets[(gridColor) + (i * grid.totalCells)] = targetBucket;
                        i++;
                    } else {
                        grid.targetBuckets[(gridColor) + (i * grid.totalCells)] = -1;
                        i++;
                    }
                }
            }
        }

        grid.numTargetBuckets[gridColor] = i;
    }, xGridSize, yGridSize, zGridSize, *this);
    Iterator::syncAll();

}

void NeighbourGrid::colourParticles(ParticleData &data)
{
    //

    colours.resize(data.numLocal, false);
    bucketSizes.resize(totalCells, false);
    bucketStarts.resize(totalCells, false);

    data.iterateAllParticles(OffloadExec(), [] DEVCALLABLE (pInd_t pInd, ParticleData &data, NeighbourGrid &grid) {

        //
        auto[xGridCord, yGridCord, zGridCord] = grid.getGridId(data.xCord[pInd], data.yCord[pInd], data.zCord[pInd]);
        int colour = grid.getGridColor(xGridCord, yGridCord, zGridCord);

        grid.colours[pInd] = colour;
    }, data, *this);

    Iterator::syncAll();

    Iterator::iterate(OffloadExec(), [] DEVCALLABLE (pInd_t i, NeighbourGrid &grid){
        grid.bucketSizes[i] = 0;
        }, bucketSizes.size(), *this);

    Iterator::iterate(OffloadExec(), [] DEVCALLABLE (pInd_t i, NeighbourGrid &grid){
        int c = grid.colours[i];
        //atomicAdd(&(bucketSizes[c]), 1);
        Atomic::atomicIncVal(grid.bucketSizes[c]);
        }, colours.size(), *this);


    Iterator::syncAll();

    ParallelScan::exclusive(bucketSizes, bucketStarts);
    Iterator::syncAll();

}

void NeighbourGrid::generateIterationList(ParticleData &data)
{
        xvtxRangePush("generateIterationList");

    iterationOrder.resize(data.numLocal, false);
    data.iterateAllParticles(OffloadExec(), []DEVCALLABLE(pInd_t pInd, ParticleData &data, NeighbourGrid &grid) {
        grid.iterationOrder[pInd] = pInd;
    }, data, *this);
    Iterator::syncAll();

    // sort iterationOrder based on colour
    // todo add parallel stl here once its properly available
    // todo does not really need to be sort, more a histogram of values and i know the bucket sizes
    // look at how the renderer does it
    auto keyPtr = colours.data();
    stx::sort(EXECPOLICY iterationOrder.begin(), iterationOrder.end(),
        [=] DEVCALLABLE (int a, int b) { return keyPtr[a] < keyPtr[b]; });

    // set up the collected iteration orders here
    bucketSizesColor.resize(totalCells, false);
    bucketStartsColor.resize(totalCells, false);

    Iterator::iterate(OffloadExec(),[] DEVCALLABLE (ind_t cell, NeighbourGrid &grid){
        int particlesInColor = 0;
        for (int n = 0; n < 27; ++n) {
            int targetBucket = grid.targetBuckets[cell + (n * grid.totalCells)];
            // skip if the bucket does not exist, i.e. if the target bucket is -1
            if (targetBucket == -1)
                continue;
            particlesInColor += grid.bucketSizes[targetBucket];
        }
        grid.bucketSizesColor[cell] = particlesInColor;
        }, totalCells, *this);
    Iterator::syncAll();
    xvtxRangePush("a");

    size_t sum = reducer.sum(bucketSizesColor);

    stx::exclusive_scan(EXECPOLICY bucketSizesColor.begin(), bucketSizesColor.end(), bucketStartsColor.begin(), 0);

    iterationOrderPerColor.resize(sum, false);
    xvtxRangePop();


    Iterator::iterate(OffloadExec(),[] DEVCALLABLE (ind_t cell, NeighbourGrid &grid){
        int particlesInColor = 0;
        for (int n = 0; n < 27; ++n) {
            int targetBucket = grid.targetBuckets[cell + (n * grid.totalCells)];
            // skip if the bucket does not exist, i.e. if the target bucket is -1
            if (targetBucket == -1)
                continue;

            for (size_t i = grid.bucketStarts[targetBucket]; i < grid.bucketStarts[targetBucket] + grid.bucketSizes[targetBucket]; ++i) {
                pInd_t nInd = grid.iterationOrder[i];
                grid.iterationOrderPerColor[grid.bucketStartsColor[cell]+ particlesInColor] = nInd;
                particlesInColor++;
            }
        }
    },totalCells, *this);
    Iterator::syncAll();
        xvtxRangePop();

}

void NeighbourGrid::reoder(ManVec<pInd_t> &order){
    reorderDevVec(colours,order);
}