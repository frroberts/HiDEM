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

    std::vector<std::vector<int>> cells;

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

        xCellSize = cellSize;
        yCellSize = cellSize;
        zCellSize = cellSize;

        xCellSizeInv = 1.f / xCellSize;
        yCellSizeInv = 1.f / yCellSize;
        zCellSizeInv = 1.f / zCellSize;

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

        xCount = 1 + (xSize / xCellSize);
        yCount = 1 + (ySize / yCellSize);
        zCount = 1 + (zSize / zCellSize);

        cells.clear();
        sizes.clear();
        starts.clear();
        values.clear();

        lpReal_t maxDist =
                (((xCellSize * xCellSize) + (yCellSize * yCellSize) + (zCellSize * zCellSize)) * 0.5f) + searchDistance;

        starts.resize(xCount * yCount * zCount);
        sizes.resize(xCount * yCount * zCount);


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
        // print stats about grid

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
        // returns the square distance, to get the read distance compute sqrt(triSDF2)


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

        auto clamp = [](Real3f vec, lpReal_t hi, lpReal_t lo) {
            return Real3f(std::clamp((lpReal_t) vec.x, hi, lo),
                         std::clamp((lpReal_t) vec.y, hi, lo),
                         std::clamp((lpReal_t) vec.z, hi, lo));
        };

        return (
                (sign(Real3f::dot(Real3f::cross(ba, nor), pa)) +
                 sign(Real3f::dot(Real3f::cross(cb, nor), pb)) +
                 sign(Real3f::dot(Real3f::cross(ac, nor), pc)) < 2.0)
                ?
                std::min(std::min(
                                 dot2(ba * clamp(Real3f::dot(ba, pa) / dot2(ba), 0.0, 1.0) - pa),
                                 dot2(cb * clamp(Real3f::dot(cb, pb) / dot2(cb), 0.0, 1.0) - pb)),
                         dot2(ac * clamp(Real3f::dot(ac, pc) / dot2(ac), 0.0, 1.0) - pc))
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
            return std::numeric_limits<float>::infinity();


        float inv_det = 1.f / det;
        Real3 tvec = orig - v1;
        float u = Real3::dot(tvec, pvec) * inv_det;

        if ((u < 0.f || u > 1.f))
            return std::numeric_limits<float>::infinity();

        Real3 qvec = Real3::cross(tvec, e1);
        float v = Real3::dot(dir, qvec) * inv_det;
        if (((v < 0.f || u + v > 1.f)))
            return std::numeric_limits<float>::infinity();

        r = Real3::dot(e2, qvec) * inv_det;
        return r;

    }


        // unaccelerated, need to think how to accelerate this, triangles can be in multiple cells
    bool withinGeometry(Real3 cord, double radius,
                                lpReal_t *xVtxCord, lpReal_t *yVtxCord, lpReal_t *zVtxCord, int numTris) {
        // check bounding box
        if (!inBB(cord.x, cord.y, cord.z)) {
            return false;
        }

        // check if inside geometry
        Real3 dir{0,1,0.5};
        lpReal_t closestHit = std::numeric_limits<float>::infinity();
        int hits = 0;
        for (int i = 0; i < numTris; i++) {
            auto p1 = Real3(xVtxCord[i * 3], yVtxCord[(i * 3)], zVtxCord[(i * 3)]);
            auto p2 = Real3{xVtxCord[(i * 3) + 1], yVtxCord[(i * 3) + 1], zVtxCord[(i * 3) + 1]};
            auto p3 = Real3{xVtxCord[(i * 3) + 2], yVtxCord[(i * 3) + 2], zVtxCord[(i * 3) + 2]};

            lpReal_t dist = intersectDist(cord, dir, p1, p2, p3);

            if (dist != std::numeric_limits<lpReal_t>::infinity() && dist > 0) {
                hits++;
                closestHit = std::min(dist, closestHit);
            }
        }

        bool inMesh = hits % 2;

        // todo reenable from clear zone around meshses

        // check distance to geometry
        auto [closest, dist, searched] = getClosestTriAccel(cord.x, cord.y, cord.z, radius,
                                         xVtxCord, yVtxCord, zVtxCord);
        //std::cout << closest << std::endl;
        if(dist < radius)
            inMesh = true;


        //std::cout << "out" << std::endl;

        return inMesh;

    }

};

#endif //PHELL_MESHACCEL_H
