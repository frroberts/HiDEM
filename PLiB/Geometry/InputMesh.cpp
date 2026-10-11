//
// Created by robertse on 20/05/23.
//

//#include "InputMesh.h"
/*
InputMesh::InputMesh() {

}

InputMesh::~InputMesh() {
    //
}

bool InputMesh::withinGeometry(Real3 cord, lpReal_t radius) {
    if (cord.x > max.x || cord.y > max.y || cord.z > max.z ||
        cord.x < min.x || cord.y < min.y || cord.z < min.z)
        return false;
    Real3 dir{1, 0, 0};
    lpReal_t closestHit = std::numeric_limits<lpReal_t>::infinity();
    int hits = 0;

    auto [x,y,z] = getCords(cord);

    for (auto ind: accelStruct[getBin(x, y, z)]) {
        auto&t = triangles[ind];
        lpReal_t dist = t.intersectDist(cord, dir, t.points[0], t.points[1], t.points[2]);
        if (dist != std::numeric_limits<lpReal_t>::infinity() && dist > 0) { // maybe - radius here ?
            hits++;
            closestHit = std::min(dist, closestHit);
        }
    }


    bool inMesh = hits % 2;

    return inMesh;
}

void InputMesh::setOffset(Real3 offset) {
    for (auto&t: triangles) {
        for (auto&p: t.points)
            p = p + offset;
    }
}

auto InputMesh::getLimits() -> std::tuple<Real3, Real3> {
    // maybe code these to be the static ones...
    Real3 min{std::numeric_limits<lpReal_t>::max()};
    Real3 max{std::numeric_limits<lpReal_t>::lowest()};
    for (const auto&t: triangles) {
        for (const auto p: t.points) {
            min.x = std::min(min.x, p.x);
            min.y = std::min(min.y, p.y);
            min.z = std::min(min.z, p.z);

            max.x = std::max(max.x, p.x);
            max.y = std::max(max.y, p.y);
            max.z = std::max(max.z, p.z);
        }
    }
    return std::tuple<Real3, Real3>(min, max);
}

void InputMesh::generateAccelStruct() {
    //
    auto [min_, max_] = getLimits();
    min = min_;
    max = max_;
    Real3 size = max - min;
    // todo figure out bins better
    xBins = 1; // (max.x - min.x)/
    yBins = 25; // (max.y - min.y)/
    zBins = 25; // (max.z - min.z)/

    binSize = size / Real3(xBins, yBins, zBins);

    numBuckets = (xBins + 1) * (yBins + 1) * (zBins + 1);

    accelStruct.resize(numBuckets);

    for (int i = 0; i < triangles.size(); ++i) {
        Real3 bbLow = std::numeric_limits<lpReal_t>::max();
        Real3 bbHigh = std::numeric_limits<lpReal_t>::lowest();

        //
        for (auto&p: triangles[i].points) {
            //
            bbLow.x = std::min(bbLow.x, p.x);
            bbLow.y = std::min(bbLow.y, p.y);
            bbLow.z = std::min(bbLow.z, p.z);

            bbHigh.x = std::max(bbHigh.x, p.x);
            bbHigh.y = std::max(bbHigh.y, p.y);
            bbHigh.z = std::max(bbHigh.z, p.z);
        }

        bbLow = bbLow - min;
        bbHigh = bbHigh - min;
        int xStart = bbLow.x / binSize.x;
        int yStart = bbLow.y / binSize.y;
        int zStart = bbLow.z / binSize.z;

        int xEnd = 1 + (bbHigh.x / binSize.x);
        int yEnd = 1 + (bbHigh.y / binSize.y);
        int zEnd = 1 + (bbHigh.z / binSize.z);

        // todo needs to strip out the boxes not intersecting
        // todo stencil around the tri +1
        for (int x = xStart; x < xEnd; ++x) {
            for (int y = yStart; y < yEnd; ++y) {
                for (int z = zStart; z < zEnd; ++z) {
                    //
                    int bin = getBin(x, y, z);
                    accelStruct[bin].push_back(i);
                }
            }
        }
    }
}

std::tuple<int, int, int> InputMesh::getCords(Real3 cord) {
    cord = cord - min;

    int x = cord.x / binSize.x;
    int y = cord.y / binSize.y;
    int z = cord.z / binSize.z;

    return std::make_tuple(x, y, z);
}

int InputMesh::getBin(int x, int y, int z) {
    return (x * yBins * zBins) + (y * zBins) + z;
}

void InputMesh::scale(Real3 val) {
    for (auto&t: triangles)
        for (auto&p: t.points) {
            p = p * val;
        }
}
*/