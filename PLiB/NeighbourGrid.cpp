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
#include <thrust/unique.h>
#include <thrust/execution_policy.h>

#endif

DEVCALLABLE
int64_t NeighbourGrid::getGridColor(int x, int y, int z) {
    return (static_cast<int64_t>(x)) + ((static_cast<int64_t>(y)) * static_cast<int64_t>(xGridSize)) +
        ((static_cast<int64_t>(z)) * static_cast<int64_t>(xGridSize) * static_cast<int64_t>(yGridSize));
}

DEVCALLABLE
std::tuple<double, double, double> NeighbourGrid::getGridId(double x, double y, double z) {
    return std::make_tuple((int) std::floor((x - xMin) / gridSpacing), (int) std::floor((y - yMin) / gridSpacing), (int) std::floor((z - zMin) / gridSpacing));
}

DEVCALLABLE
int64_t NeighbourGrid::getParticleColor(pInd_t pInd) {
    return colours[pInd];
}

std::tuple<double, double, double> NeighbourGrid::getGridSize(double x, double y, double z) {
    int xGridS = 1 + (int) std::floor((x) / gridSpacing);
    int yGridS = 1 + (int) std::floor((y) / gridSpacing);
    int zGridS = 1 + (int) std::floor((z) / gridSpacing);

    return std::make_tuple(xGridS, yGridS, zGridS);
}

void NeighbourGrid::generate(ParticleData &pData, double searchDistance) {
    xvtxRangePush("grid regen");

    gridSpacing = searchDistance;
    generateNeighbourGrid(pData);
    colourParticles(pData);
    generateIterationList(pData);


    int maxVolume;
    int minVolume;
    int maxUsed;
    int minUsed;
    int ownUsed = colorOffset.numActive();
    MPI_Reduce(&ownUsed, &minUsed, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&ownUsed, &maxUsed, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&totalCells, &minVolume, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&totalCells, &maxVolume, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);

    Logger::writeAtFirst(fmt::format("Grid regen out: min volume {} max volume {}, max active cells {} \n", minVolume, maxVolume, maxUsed), true);

    xvtxRangePop();
}

void NeighbourGrid::generateNeighbourGrid(ParticleData &data) {
    xvtxRangePush("generateNeighbourGrid");

    auto [_xMax, _yMax, _zMax, _xMin, _yMin, _zMin] = data.getLimits();

    xMax = _xMax;
    yMax = _yMax;
    zMax = _zMax;
    xMin = _xMin;
    yMin = _yMin;
    zMin = _zMin;

    auto [_xGridSize, _yGridSize, _zGridSize] = getGridSize(xMax - xMin, yMax - yMin, zMax - zMin);
    xGridSize = _xGridSize;
    yGridSize = _yGridSize;
    zGridSize = _zGridSize;

    int prevTotalCells = totalCells;
    totalCells = xGridSize * yGridSize * zGridSize;
    if ((prevTotalCells != 0) && ((totalCells / 10) > prevTotalCells)) {
        std::cout << getLocalRank() << " new (" << totalCells << ") grid 10x larger than old (" << prevTotalCells << ") , did something explode ?" << std::endl;
    }
    // if we get an empty domain set num cells to 0 to avoid crashing
    if (data.numLocal == 0)
        totalCells = 0;
    //std::cout << _xGridSize << " " << _yGridSize << " " << _zGridSize << " " << totalCells << std::endl;
    xvtxRangePush("this 1");

    int i = 0;
    for (int zD = -1; zD < 2; zD++) {
        for (int yD = -1; yD < 2; yD++) {
            for (int xD = -1; xD < 2; xD++) {
                dirOffsets[i] = getGridColor(xD, yD, zD);
                i++;
            }
        }
    }

    Iterator::syncAll();
    xvtxRangePop();
    xvtxRangePop();

}

void NeighbourGrid::colourParticles(ParticleData &data) {
    xvtxRangePush("colourParticles");
    colorOffset.clear();

    colours.resize(data.numLocal, false);
    coloursUniq.resize(data.numLocal, false);

    data.iterateAllParticles(OffloadExec(), [=] DEVCALLABLE (pInd_t pInd, ParticleData &data, NeighbourGrid &grid) {
        //
        auto [xGridCord, yGridCord, zGridCord] = grid.getGridId(data.xCord[pInd], data.yCord[pInd], data.zCord[pInd]);
        int colour = grid.getGridColor(xGridCord, yGridCord, zGridCord);

        grid.colours[pInd] = colour;
        grid.coloursUniq[pInd] = colour;
    }, data, *this);
    Iterator::syncAll();
    // sort uniq colors would get the count of colors
    // bulk create map

    stx::sort(EXECPOLICY coloursUniq.begin(), coloursUniq.end());
    auto newEnd = stx::unique(EXECPOLICY coloursUniq.begin(), coloursUniq.end());
    auto numColors = std::distance(coloursUniq.begin(), newEnd);

    coloursUniq.resize(numColors);

    colorOffset.setSize(numColors);

    colorOffset.bulkCreate(coloursUniq);

    bucketSizes.resize(numColors, false);
    bucketStarts.resize(numColors, false);

    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (pInd_t i, NeighbourGrid &grid) {
        grid.bucketSizes[i] = 0;
    }, bucketSizes.size(), *this);

    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (pInd_t i, NeighbourGrid &grid) {
        int c = grid.colorOffset.getValue(grid.colours[i]);
        Atomic::atomicIncVal(grid.bucketSizes[c]);
    }, colours.size(), *this);
    Iterator::syncAll();
    stx::exclusive_scan(EXECPOLICY bucketSizes.begin(), bucketSizes.end(), bucketStarts.begin(), 0);

    Iterator::syncAll();
    xvtxRangePop();
}

void NeighbourGrid::generateIterationList(ParticleData &data) {
    xvtxRangePush("generateIterationList");

    iterationOrder.resize(data.numLocal, false);
    data.iterateAllParticles(OffloadExec(), []DEVCALLABLE(pInd_t pInd, ParticleData &data, NeighbourGrid &grid) {
        grid.iterationOrder[pInd] = pInd;
    }, data, *this);

    auto keyPtr = colours.data();
    stx::sort(EXECPOLICY iterationOrder.begin(), iterationOrder.end(),
              [=] DEVCALLABLE (int a, int b) {
        if (keyPtr[a] == keyPtr[b]) {
            // check what the actual order here should be ?
            return a < b;
        }
        return keyPtr[a] < keyPtr[b];
    });

    Iterator::syncAll();
    xvtxRangePop();

}

void NeighbourGrid::reorder(ManVec<pInd_t> &order) {
    reorderDevVec(colours, order);
}

void NeighbourGrid::toVTKHDF(std::string fileName) {
        HDF5IO io;
        io.createFile(fileName, MPI_COMM_SELF);

        io.openGroup("VTKHDF");
        io.writeAttributeStr("Type", "UnstructuredGrid");
        std::vector<long long int> version = {2, 2};
        io.writeAttributeArr("Version", version);

        ManVec<hpReal_t> points;
        ManVec<int> connectivity;
        ManVec<unsigned char> types;
        ManVec<int> cellLoad;

        int conn = 0;
        int numCells = 0;
        Iterator::iterate3D(SerialExec(), [&] (int x, int y, int z, NeighbourGrid &grid) {
            int64_t gridColor = grid.getGridColor(x, y, z);

            int64_t targetBucket = colorOffset.getValue(gridColor);
            if (targetBucket == colorOffset.notFound())
                return;

            cellLoad.push_back(grid.bucketSizes[targetBucket]);

            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            types.push_back(11);
            numCells++;

            points.push_back(grid.xMin + (x * grid.gridSpacing));
            points.push_back(grid.yMin + (y * grid.gridSpacing));
            points.push_back(grid.zMin + (z * grid.gridSpacing));

            points.push_back(grid.xMin + ((x + 1) * grid.gridSpacing));
            points.push_back(grid.yMin + (y * grid.gridSpacing));
            points.push_back(grid.zMin + (z * grid.gridSpacing));

            points.push_back(grid.xMin + (x * grid.gridSpacing));
            points.push_back(grid.yMin + ((y + 1) * grid.gridSpacing));
            points.push_back(grid.zMin + (z * grid.gridSpacing));

            points.push_back(grid.xMin + ((x + 1) * grid.gridSpacing));
            points.push_back(grid.yMin + ((y + 1) * grid.gridSpacing));
            points.push_back(grid.zMin + (z * grid.gridSpacing));

            points.push_back(grid.xMin + (x * grid.gridSpacing));
            points.push_back(grid.yMin + (y * grid.gridSpacing));
            points.push_back(grid.zMin + ((z + 1) * grid.gridSpacing));

            points.push_back(grid.xMin + ((x + 1) * grid.gridSpacing));
            points.push_back(grid.yMin + (y * grid.gridSpacing));
            points.push_back(grid.zMin + ((z + 1) * grid.gridSpacing));

            points.push_back(grid.xMin + (x * grid.gridSpacing));
            points.push_back(grid.yMin + ((y + 1) * grid.gridSpacing));
            points.push_back(grid.zMin + ((z + 1) * grid.gridSpacing));

            points.push_back(grid.xMin + ((x + 1) * grid.gridSpacing));
            points.push_back(grid.yMin + ((y + 1) * grid.gridSpacing));
            points.push_back(grid.zMin + ((z + 1) * grid.gridSpacing));

        }, xGridSize, yGridSize, zGridSize, *this);
        Iterator::syncAll();

        io.writeVectorAll2D(points, "Points", 3);
        ManVec<long long int> NumberOfPoints;
        NumberOfPoints.push_back(points.size()/3);
        io.writeVectorFirst(NumberOfPoints, "NumberOfPoints");

        ManVec<int> NumberOfConnectivityIds;
        NumberOfConnectivityIds.push_back(numCells * 8);
        io.writeVectorFirst(NumberOfConnectivityIds, "NumberOfConnectivityIds");

        ManVec<int> NumberOfCells;
        NumberOfCells.push_back(numCells);
        io.writeVectorFirst(NumberOfCells, "NumberOfCells");

        ManVec<int> Offsets;
        for (int i = 0; i < numCells + 1; i++) {
            Offsets.push_back((i) * 8);
        }
        io.writeVectorFirst(Offsets, "Offsets");

        io.writeVectorFirst(connectivity, "Connectivity");
        io.writeVectorFirst(types, "Types");

        io.openGroup("CellData");
        io.writeVectorFirst(cellLoad, "numParticles");
        io.closeGroup();
        io.closeGroup();

        io.closeFile();
    }
