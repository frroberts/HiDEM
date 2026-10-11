//
// Created by Fredrik Robertsen on 09/10/2020.
//

#ifndef PHELL_NEIGHBOURGRID_H
#define PHELL_NEIGHBOURGRID_H

#include <vector>
#include <tuple>

#include "ParticleData.h"
#include "../Iterators/ManVec.h"
#include "../Iterators/Allocator.h"
#include "../Iterators/DevCall.h"

#include "../Iterators/ParallelScan.h"
#include "../Iterators/ParallelSort.h"
#include "../Iterators/ParallelReduce.h"


class NeighbourGrid : public ManagedParent {
public:

    //DevVec<int> gridX;
    //DevVec<int> gridY;
    //DevVec<int> gridZ;
    ManVec<size_t> offsets;
    ManVec<int> targetBuckets;
    ManVec<int> numTargetBuckets;
    ManVec<int> colours;

    ManVec<int> bucketSizes;
    ManVec<int> bucketStarts;
    ManVec<pInd_t > iterationOrder;

    ManVec<size_t> bucketStartsColor; // sized num buckets
    ManVec<size_t> bucketSizesColor; // sized num buckets
    ManVec<pInd_t > iterationOrderPerColor;

    ParallelReduce<size_t> reducer;

    int xGridSize;
    int yGridSize;
    int zGridSize;

    double xMin;
    double yMin;
    double zMin;
    double xMax;
    double yMax;
    double zMax;

    #ifdef GPU
    void     *devTempStorage = NULL;
	size_t   allocatedDevTempBytes = 0;
    #endif


    int totalCells;
    double gridSpacing;
    NeighbourGrid() {

    };


    void generate(ParticleData &pData, double searchDistance){
        gridSpacing = searchDistance;
        generateNeighbourGrid(pData);
        colourParticles(pData);
        generateIterationList(pData);
    }

    DEVCALLABLE
    int getGridColor(int x, int y, int z);

    DEVCALLABLE
    std::tuple<int, int, int> getCord(int color) {
        int x = color % xGridSize;
        int y = (color/xGridSize) % yGridSize;
        int z = (color/(xGridSize*yGridSize));
        //     return ((x))+((y) * xGridSize) + ((z) * xGridSize * yGridSize);
        return std::tuple<int, int, int>(x,y,z);
    }

    DEVCALLABLE
    std::tuple<double, double, double> getGridId(double x, double y, double z);

    DEVCALLABLE
    int getPraticleColor(pInd_t pInd);

    std::tuple<double, double, double> getGridSize(double x, double y, double z);

    void generateNeighbourGrid(ParticleData &data);

    void colourParticles(ParticleData &data);

    void generateIterationList(ParticleData &data);

    void reoder(ManVec<pInd_t> &order);

    template<typename Fun, class... Args>
    DEVCALLABLE
    void iteratePossibleNeighbours(Fun fun, pInd_t pInd, int colour, bool indirect, Args &... args) {

        // if the particles are not reordered else assume they are ordered
        if (indirect) {
            for (int n = 0; n < 27; ++n) {
                int targetBucket = targetBuckets[(colour) + (n * totalCells)];
                // skip if the bucket does not exist, i.e. if the target bucket is -1
                if (targetBucket == -1)
                    continue;

                for (size_t i = bucketStarts[targetBucket]; i < bucketStarts[targetBucket] + bucketSizes[targetBucket]; ++i) {
                    pInd_t nInd = iterationOrder[i];
                    if (nInd == pInd) {
                        continue;
                    }

                    fun(nInd, args...);
                }
            }
        } else {
            for (int n = 0; n < 27; ++n) {
                int targetBucket = targetBuckets[(colour) + (n * totalCells)];
                // skip if the bucket does not exist
                if (targetBucket == -1)
                    continue;

                for (size_t i = bucketStarts[targetBucket]; i < bucketStarts[targetBucket] + bucketSizes[targetBucket]; ++i) {
                    pInd_t nInd = i;
                    if (nInd == pInd) {
                        continue;
                    }

                    fun(nInd, args...);
                }
            }
        }
    }
};


#endif //PHELL_NEIGHBOURGRID_H
