//
// Created by Fredrik Robertsen on 5/23/24.
//

#ifndef PHELL_MESHACCEL_H
#define PHELL_MESHACCEL_H

#include "ManVec.h"
#include <Iterator.h>
#include <ParallelScan.h>
#include <mpi.h>
#include "../Util/Util.h"


class MeshAccel : public ManagedParent{
public:
    lpReal_t xUpper;
    lpReal_t yUpper;
    lpReal_t zUpper;

    lpReal_t xLower;
    lpReal_t yLower;
    lpReal_t zLower;

    lpReal_t xSize;
    lpReal_t ySize;
    lpReal_t zSize;

    lpReal_t xCellSize;
    lpReal_t yCellSize;
    lpReal_t zCellSize;

    lpReal_t xCellSizeInv;
    lpReal_t yCellSizeInv;
    lpReal_t zCellSizeInv;

    int xCount;
    int yCount;
    int zCount;

    float rangeFactor = 1;


    ManVec<int> starts;
    ManVec<int> sizes;
    ManVec<int> values;

    DEVCALLABLE
    bool inBB(lpReal_t xCord, lpReal_t yCord, lpReal_t zCord) {
        if (xCord > xUpper || yCord > yUpper || zCord > zUpper || xCord < xLower || yCord < yLower || zCord < zLower) {
            return false;
        }
        return true;
    }


    void generateAccel(lpReal_t cellSize, lpReal_t searchDistance,
                       lpReal_t *xVtxCord, lpReal_t *yVtxCord, lpReal_t *zVtxCord, int numTris) {
        xUpper = std::numeric_limits<lpReal_t>::lowest();
        yUpper = std::numeric_limits<lpReal_t>::lowest();
        zUpper = std::numeric_limits<lpReal_t>::lowest();

        xLower = std::numeric_limits<lpReal_t>::max();
        yLower = std::numeric_limits<lpReal_t>::max();
        zLower = std::numeric_limits<lpReal_t>::max();

        for (int i = 0; i < numTris*3; i++){
            auto x = xVtxCord[i];
            auto y = yVtxCord[i];
            auto z = zVtxCord[i];
            xUpper = std::max(x, xUpper);
            xLower = std::min(x, xLower);
            yUpper = std::max(y, yUpper);
            yLower = std::min(y, yLower);
            zUpper = std::max(z, zUpper);
            zLower = std::min(z, zLower);
        }

        xUpper += cellSize;
        yUpper += cellSize;
        zUpper += cellSize;
        xLower -= cellSize;
        yLower -= cellSize;
        zLower -= cellSize;
        xSize = xUpper - xLower;
        ySize = yUpper - yLower;
        zSize = zUpper - zLower;

        xCellSize = cellSize;
        yCellSize = cellSize;
        zCellSize = cellSize;

        xCellSizeInv = 1.f / xCellSize;
        yCellSizeInv = 1.f / yCellSize;
        zCellSizeInv = 1.f / zCellSize;

        xCount = 1 + (xSize / xCellSize);
        yCount = 1 + (ySize / yCellSize);
        zCount = 1 + (zSize / zCellSize);

        std::cout << cellSize << std::endl;

        sizes.clear();
        starts.clear();
        values.clear();

        // should be enough to fit all
        lpReal_t maxDist = (xCellSize * xCellSize) + (yCellSize * yCellSize) + (zCellSize * zCellSize) + searchDistance;

        starts.resize(xCount * yCount * zCount);
        sizes.resize(xCount * yCount * zCount);

        if (sizes.size() ==0 ) {
            std::cout << "mesh generation failed" << std::endl;
        }

        Iterator::iterate3D(OffloadExec(), [=]  DEVCALLABLE(int x, int y, int z, MeshAccel &accel) {
            lpReal_t xPos = (x * accel.xCellSize + (0.5f * accel.xCellSize)) + accel.xLower;
            lpReal_t yPos = (y * accel.yCellSize + (0.5f * accel.yCellSize)) + accel.yLower;
            lpReal_t zPos = (z * accel.zCellSize + (0.5f * accel.zCellSize)) + accel.zLower;

            Real3f cord = Real3f{xPos, yPos, zPos};

            int numFound = 0;
            for (int i = 0; i < numTris; i++) {
                auto p1 = Real3f(xVtxCord[i * 3], yVtxCord[(i * 3)], zVtxCord[(i * 3)]);
                auto p2 = Real3f{xVtxCord[(i * 3) + 1], yVtxCord[(i * 3) + 1], zVtxCord[(i * 3) + 1]};
                auto p3 = Real3f{xVtxCord[(i * 3) + 2], yVtxCord[(i * 3) + 2], zVtxCord[(i * 3) + 2]};
                lpReal_t dist = accel.triSDF2(cord, p1, p2, p3);
                if (dist < maxDist) {
                    numFound++;
                }
            }
            accel.sizes[accel.getCell(xPos, yPos, zPos)] = numFound;
        }, xCount, yCount, zCount, *this);
        Iterator::syncAll();

        ParallelScan::exclusive(sizes, starts);

        values.resize(sizes[sizes.size() - 1] + starts[starts.size() - 1]);

        Iterator::iterate3D(OffloadExec(), [=]  DEVCALLABLE(int x, int y, int z, MeshAccel &accel) {
            //
            lpReal_t xPos = (x * accel.xCellSize + (0.5f * accel.xCellSize)) + accel.xLower;
            lpReal_t yPos = (y * accel.yCellSize + (0.5f * accel.yCellSize)) + accel.yLower;
            lpReal_t zPos = (z * accel.zCellSize + (0.5f * accel.zCellSize)) + accel.zLower;

            int cell = accel.getCell(xPos, yPos, zPos);

            Real3f cord = Real3f{xPos, yPos, zPos};

            int numFound = 0;
            for (int i = 0; i < numTris; i++) {
                auto p1 = Real3f(xVtxCord[i * 3], yVtxCord[(i * 3)], zVtxCord[(i * 3)]);
                auto p2 = Real3f{xVtxCord[(i * 3) + 1], yVtxCord[(i * 3) + 1], zVtxCord[(i * 3) + 1]};
                auto p3 = Real3f{xVtxCord[(i * 3) + 2], yVtxCord[(i * 3) + 2], zVtxCord[(i * 3) + 2]};
                lpReal_t dist = accel.triSDF2(cord, p1, p2, p3);
                if (dist < maxDist) {
                    accel.values[accel.starts[cell] + numFound] = i;
                    numFound++;
                }
            }
        }, xCount, yCount, zCount, *this);
        Iterator::syncAll();
        if(getLocalRank() == 0) {
            std::cout << "Mesh grid size " << xCount * yCount * zCount << std::endl;
            std::cout << "Mesh grid size " << xSize << " " << ySize << " " << zSize << std::endl;
            std::cout << "Cell size " << cellSize << " " << xCellSize << " " << yCellSize << " " << zCellSize << std::endl;
        }
    }

    void toVTKHDF(std::string fileName) {
        HDF5IO io;
        io.createFile(fileName, MPI_COMM_WORLD);

        io.openGroup("VTKHDF");
        io.writeAttributeStr("Type", "UnstructuredGrid");
        std::vector<long long int> version = {2, 2};
        io.writeAttributeArr("Version", version);

        ManVec<hpReal_t> points;
        ManVec<int> connectivity;
        ManVec<unsigned char> types;
        ManVec<int> trisInCell;

        int conn = 0;
        int numCells = 0;
        Iterator::iterate3D(SerialExec(), [&] (int x, int y, int z, MeshAccel &accel) {

            int size = accel.sizes[(x * accel.yCount * zCount) + (y * accel.zCount) + (z)];
            trisInCell.push_back(size);

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

            points.push_back(accel.xLower + (x * accel.xCellSize));
            points.push_back(accel.yLower + (y * accel.yCellSize));
            points.push_back(accel.zLower + (z * accel.zCellSize));

            points.push_back(accel.xLower + ((x + 1) * accel.xCellSize));
            points.push_back(accel.yLower + (y * accel.yCellSize));
            points.push_back(accel.zLower + (z * accel.zCellSize));

            points.push_back(accel.xLower + (x * accel.xCellSize));
            points.push_back(accel.yLower + ((y + 1) * accel.yCellSize));
            points.push_back(accel.zLower + (z * accel.zCellSize));

            points.push_back(accel.xLower + ((x + 1) * accel.xCellSize));
            points.push_back(accel.yLower + ((y + 1) * accel.yCellSize));
            points.push_back(accel.zLower + (z * accel.zCellSize));

            points.push_back(accel.xLower + (x * accel.xCellSize));
            points.push_back(accel.yLower + (y * accel.yCellSize));
            points.push_back(accel.zLower + ((z + 1) * accel.zCellSize));

            points.push_back(accel.xLower + ((x + 1) * accel.xCellSize));
            points.push_back(accel.yLower + (y * accel.yCellSize));
            points.push_back(accel.zLower + ((z + 1) * accel.zCellSize));

            points.push_back(accel.xLower + (x * accel.xCellSize));
            points.push_back(accel.yLower + ((y + 1) * accel.yCellSize));
            points.push_back(accel.zLower + ((z + 1) * accel.zCellSize));

            points.push_back(accel.xLower + ((x + 1) * accel.xCellSize));
            points.push_back(accel.yLower + ((y + 1) * accel.yCellSize));
            points.push_back(accel.zLower + ((z + 1) * accel.zCellSize));

        }, xCount, yCount, zCount, *this);
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
        io.writeVectorFirst(trisInCell, "trisInCell");
        io.closeGroup();
        io.closeGroup();

        io.closeFile();
    }

    void flattenZ() {

        // builds on the fact that the grid is laid out with Z as the inner dimmension

        ManVec<int> startsNew;
        ManVec<int> sizesNew;
        ManVec<int> valuesNew;

        startsNew.resize(xCount * yCount * 1);
        sizesNew.resize(xCount * yCount * 1);

        for (int xCell = 0; xCell < xCount; xCell++) {
            for (int yCell = 0; yCell < yCount; yCell++) {
                std::vector<int> newZCell;
                for (int zCell = 0; zCell < zCount; zCell++) {
                    int cell = (xCell * yCount * zCount) + (yCell * zCount) + zCell;

                    int start = starts[cell];
                    int end = start + sizes[cell];
                    for (int i = start; i < end; i++) {
                        //
                        int tri = values[i];
                        newZCell.push_back(tri);
                    }
                }

                std::sort( newZCell.begin(), newZCell.end() );
                newZCell.erase( std::unique( newZCell.begin(), newZCell.end() ), newZCell.end() );
                int newZCellSize = newZCell.size();

                sizesNew[(xCell * yCount * 1) + (yCell * 1) + (0)] = newZCellSize;

            }
        }

        ParallelScan::exclusive(sizesNew, startsNew);
        valuesNew.resize(sizesNew[sizesNew.size() - 1] + startsNew[startsNew.size() - 1]);

        for (int xCell = 0; xCell < xCount; xCell++) {
            for (int yCell = 0; yCell < yCount; yCell++) {
                std::vector<int> newZCell;
                for (int zCell = 0; zCell < zCount; zCell++) {
                    int cell = (xCell * yCount * zCount) + (yCell * zCount) + zCell;
                    int start = starts[cell];
                    int end = start + sizes[cell];
                    for (int i = start; i < end; i++) {
                        //
                        int tri = values[i];
                        newZCell.push_back(tri);
                    }
                }
                std::sort( newZCell.begin(), newZCell.end() );
                newZCell.erase( std::unique( newZCell.begin(), newZCell.end() ), newZCell.end() );

                int start = startsNew[(xCell * yCount * 1) + (yCell * 1) + (0)];
                for (int i = start; i < start+sizesNew[(xCell * yCount * 1) + (yCell * 1) + (0)]; i++) {
                    valuesNew[i] = newZCell[i-start];
                }
            }
        }

        // set z to just one bucket
        zCount = 1;
        zCellSize = zSize;
        zCellSizeInv = 1.f / zCellSize;

        std::swap(starts,startsNew);
        std::swap(sizes,sizesNew);
        std::swap(values,valuesNew);
    }

    DEVCALLABLE
    std::tuple<int, int, int> getCellCord(Real3f cord) {
        cord.x -= xLower;
        cord.y -= yLower;
        cord.z -= zLower;

        int xCell = cord.x * xCellSizeInv;
        int yCell = cord.y * yCellSizeInv;
        int zCell = cord.z * zCellSizeInv;

        return std::make_tuple(xCell, yCell, zCell);
    }

    DEVCALLABLE
    int getCell(lpReal_t xCord, lpReal_t yCord, lpReal_t zCord) {

        auto [xCell, yCell, zCell] = getCellCord({xCord, yCord, zCord});

        return (xCell * yCount * zCount) + (yCell * zCount) + (zCell);
    }

    DEVCALLABLE
    int getCellBB(lpReal_t xCord, lpReal_t yCord, lpReal_t zCord) {
        if (!inBB(xCord, yCord, zCord))
            return -1;
        return getCell(xCord, yCord, zCord);
    }

    DEVCALLABLE
    auto triSDF2(Real3f &p, Real3f &a, Real3f &b, Real3f &c) -> lpReal_t {
        // returns the square distance, to get the real distance compute sqrt(triSDF2)


        auto dot2 = [](Real3f vec) {
            return Real3f::dot(vec, vec);
        };
        // can sort of be partially optimized away, trading compute for memory access
        Real3f ba = b - a;
        Real3f pa = p - a;
        Real3f cb = c - b;
        Real3f pb = p - b;
        Real3f ac = a - c;
        Real3f pc = p - c;
        Real3f nor = Real3f::cross(ba, ac);

        auto sign = [](float x) {
            if (x > 0) return 1.f;
            if (x < 0) return -1.f;
            return 0.f;
        };

        auto clamp3 = [](Real3f vec, lpReal_t hi, lpReal_t lo) {
            return Real3f(clamp((lpReal_t) vec.x, hi, lo),
                         clamp((lpReal_t) vec.y, hi, lo),
                         clamp((lpReal_t) vec.z, hi, lo));
        };

        return (
                (sign(Real3f::dot(Real3f::cross(ba, nor), pa)) +
                 sign(Real3f::dot(Real3f::cross(cb, nor), pb)) +
                 sign(Real3f::dot(Real3f::cross(ac, nor), pc)) < 2.0)
                ?
                std::min(std::min(
                                 dot2(ba * clamp3(Real3f::dot(ba, pa) / dot2(ba), 0.0, 1.0) - pa),
                                 dot2(cb * clamp3(Real3f::dot(cb, pb) / dot2(cb), 0.0, 1.0) - pb)),
                         dot2(ac * clamp3(Real3f::dot(ac, pc) / dot2(ac), 0.0, 1.0) - pc))
                :
                Real3f::dot(nor, pa) * Real3f::dot(nor, pa) / dot2(nor));
    };


    DEVCALLABLE
    auto getClosestTriAccel(lpReal_t xCord, lpReal_t yCord, lpReal_t zCord, lpReal_t interactDist,
                            lpReal_t *xVtxCord, lpReal_t *yVtxCord, lpReal_t *zVtxCord) {

        int cell = getCellBB(xCord, yCord, zCord);

        if (cell == -1)
            return std::make_tuple(-2, lpReal_t(100000.0f),0);
        if (sizes[cell] == 0)
            return std::make_tuple(-3,lpReal_t(100000.0f),0);

        int start = starts[cell];

        auto cord = Real3f(xCord, yCord, zCord);
        lpReal_t interactDist2 = interactDist * interactDist;
        lpReal_t minDist = std::numeric_limits<lpReal_t>::max();
        int closest = -1;
        int searched = 0;
        for (int i = start; i < start + sizes[cell]; i++) {
            //
            int tri = values[i];

            auto p1 = Real3f(xVtxCord[tri * 3], yVtxCord[(tri * 3)], zVtxCord[(tri * 3)]);
            auto p2 = Real3f{xVtxCord[(tri * 3) + 1], yVtxCord[(tri * 3) + 1], zVtxCord[(tri * 3) + 1]};
            auto p3 = Real3f{xVtxCord[(tri * 3) + 2], yVtxCord[(tri * 3) + 2], zVtxCord[(tri * 3) + 2]};

            lpReal_t currDist = triSDF2(cord, p1, p2, p3);
            //std::cout << currDist << std::endl;

            if (interactDist2 > currDist) {
                if (currDist < minDist) {
                    minDist = currDist;
                    closest = tri;
                }
            }
            searched++;
        }
        return std::make_tuple(closest, minDist, searched);

    }

        float intersectDist(
            const Real3 &orig, const Real3 &dir,
            const Real3 &v1, const Real3 &v2, const Real3 &v3) {

        float r = 0;
        Real3 e1 = v2 - v1;
        Real3 e2 = v3 - v1;
        Real3 pvec = Real3::cross(dir, e2);
        float det = Real3::dot(e1, pvec);
        if ((det < 1e-8f && det > -1e-8f))
            return std::numeric_limits<float>::max();


        float inv_det = 1.f / det;
        Real3 tvec = orig - v1;
        float u = Real3::dot(tvec, pvec) * inv_det;

        if ((u < 0.f || u > 1.f))
            return std::numeric_limits<float>::max();

        Real3 qvec = Real3::cross(tvec, e1);
        float v = Real3::dot(dir, qvec) * inv_det;
        if (((v < 0.f || u + v > 1.f)))
            return std::numeric_limits<float>::max();

        r = Real3::dot(e2, qvec) * inv_det;
        return r;

    }

    bool withinGeometryShellCheck(Real3 cord, double radius, bool outerShell,
                                lpReal_t *xVtxCord, lpReal_t *yVtxCord, lpReal_t *zVtxCord, int numTris) 
    {
        bool inMesh = withinGeometry(cord, xVtxCord, yVtxCord, zVtxCord, numTris);

        if (radius == 0) {
            return inMesh;
        }

        if (inMesh && !outerShell) {
            // inside and we need to do a reversed shell
            // check distance to geometry
            auto [closest, dist, searched] = getClosestTriAccel(cord.x, cord.y, cord.z, radius,
                                                                xVtxCord, yVtxCord, zVtxCord);
            if (dist < radius*radius) {
                // std::cout << "hit in" << std::endl;
                inMesh = false;
            }
        } else if (!inMesh && outerShell) {
            // outside and we need to do a shell
            // check distance to geometry
            auto [closest, dist, searched] = getClosestTriAccel(cord.x, cord.y, cord.z, radius,
                                                                xVtxCord, yVtxCord, zVtxCord);
            //std::cout << closest << std::endl;
            if (dist < radius*radius) {
                // std::cout << "hit out" << std::endl;
                inMesh = true;
            }
        }

        return inMesh;

                                }

        // unaccelerated, need to think how to accelerate this, triangles can be in multiple cells
    bool withinGeometry(Real3 cord, lpReal_t *xVtxCord, lpReal_t *yVtxCord, lpReal_t *zVtxCord, int numTris) {
        // check bounding box
        // bounding box might be too small ?
        if (!inBB(cord.x, cord.y, cord.z)) {
            return false;
        }

        // check if inside geometry

        Real3 dir{0,0,1};
        lpReal_t closestHit = std::numeric_limits<float>::max();
        int hits = 0;


        // assumes we have flattened Z dir
        auto [xCell, yCell, zCell] = getCellCord(Real3f(cord.x, cord.y, cord.z));

        int cell = (xCell * yCount * zCount) + (yCell * zCount) + (0);

        int start = starts[cell];
        int end = start + sizes[cell];

        for (int i = start; i < end; i++) {
            //
            int tri = values[i];

            auto p1 = Real3(xVtxCord[tri * 3], yVtxCord[(tri * 3)], zVtxCord[(tri * 3)]);
            auto p2 = Real3{xVtxCord[(tri * 3) + 1], yVtxCord[(tri * 3) + 1], zVtxCord[(tri * 3) + 1]};
            auto p3 = Real3{xVtxCord[(tri * 3) + 2], yVtxCord[(tri * 3) + 2], zVtxCord[(tri * 3) + 2]};

            lpReal_t dist = intersectDist(cord, dir, p1, p2, p3);

            if (dist != std::numeric_limits<lpReal_t>::max() && dist > 0) {
                hits++;
                closestHit = std::min(dist, closestHit);
            }
        }


        bool inMesh = hits % 2;

        return inMesh;

    }

};

#endif //PHELL_MESHACCEL_H
