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
#include "H5IO/HDF5IO.h"
#include "Logger/Logger.h"


class NeighbourGrid : public ManagedParent {
public:

    ManVec<int64_t> colours;
    ManVec<int64_t> coloursUniq;

    ManVec<int> bucketSizes;
    ManVec<int> bucketStarts;
    ManVec<pInd_t > iterationOrder;
    OffloadHashMap<int64_t, pInd_t> colorOffset;

    std::array<int64_t, 27> dirOffsets;

    int xGridSize;
    int yGridSize;
    int zGridSize;

    double xMin;
    double yMin;
    double zMin;
    double xMax;
    double yMax;
    double zMax;

    int totalCells = 0;
    double gridSpacing;

    NeighbourGrid() {

    };

    void generate(ParticleData &pData, double searchDistance);

    DEVCALLABLE
    int64_t getGridColor(int x, int y, int z);

    DEVCALLABLE
    std::tuple<int, int, int> getCord(int color) {
        int x = color % xGridSize;
        int y = (color/xGridSize) % yGridSize;
        int z = (color/(xGridSize*yGridSize));
        return std::tuple<int, int, int>(x,y,z);
    }

    DEVCALLABLE
    std::tuple<double, double, double> getGridId(double x, double y, double z);

    DEVCALLABLE
    int64_t getParticleColor(pInd_t pInd);

    std::tuple<double, double, double> getGridSize(double x, double y, double z);

    void generateNeighbourGrid(ParticleData &data);

    void colourParticles(ParticleData &data);

    void generateIterationList(ParticleData &data);

    void reorder(ManVec<pInd_t> &order);

    void toVTKHDF(std::string fileName);

    template<typename Fun, class... Args>
    DEVCALLABLE
    void iteratePossibleNeighbours(Fun fun, pInd_t pInd, int colour, bool indirect, Args &... args) {
        // if the particles are not reordered else assume they are ordered

        for (int n = 0; n < 27; ++n) {
            int targetColour = colour + dirOffsets[n];
            int targetBucket = colorOffset.getValue(targetColour);

            if (targetBucket != colorOffset.notFound()) {
                for (int i = bucketStarts[targetBucket]; i < bucketStarts[targetBucket] + bucketSizes[targetBucket]; ++i) {
                    pInd_t nInd = iterationOrder[i];
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
