//
// Created by Fredrik Robertsen on 16/02/2022.
//

#include "InputGeometry.h"

#include "TupleHelpers.h"
#include "../OutputWriters/VtkHdfWriter.h"


bool InputDEM::withinGeometry(Real3 cord, lpReal_t radius) {
    if (cord.z + radius <= base->interp(cord.x, cord.y))
        return false;
    if (cord.z + radius >= surface->interp(cord.x, cord.y))
        return false;
    return true;
}

void InputDEM::setOffset(Real3 offset) {

    // need to figure out how to set this offset for z as well ?
}

auto InputDEM::getLimits() -> std::tuple<Real3, Real3> {
    Real3 lower = {base->xMin, base->yMin,base->getMin()};
    Real3 upper = {base->xMax, base->yMax,surface->getMax()};
    return std::tuple<Real3, Real3>(lower, upper);
}


void InputDEM::toVTKHDF(std::string path) {
    //
    if (getLocalRank() == 0) {
        ManVec<lpReal_t> xPoints;
        ManVec<lpReal_t> yPoints;
        for (int y = 0; y < base->yPoints; ++y) {
            for (int x = 0; x < base->xPoints; ++x) {
                xPoints.push_back((x * base->xRes) + base->xMin);
                yPoints.push_back((y * base->yRes) + base->yMin);
            }
        }

        VtkHdfWriter writerBed;
        writerBed.createFile("base.vtkhdf", MPI_COMM_SELF);

        writerBed.writeQuadSurfaceSingle(xPoints.data(), yPoints.data(), base->data, base->xPoints, base->yPoints,
                                         std::make_tuple(), packVecsDataPtrs());

        VtkHdfWriter writerSurf;
        writerSurf.createFile("surface.vtkhdf", MPI_COMM_SELF);

        writerSurf.writeQuadSurfaceSingle(xPoints.data(), yPoints.data(), surface->data, surface->xPoints, surface->yPoints,
                                          std::make_tuple(), packVecsDataPtrs());
    }
}


void InputBox::setOffset(Real3 offset) {

    lower = lower + offset;
    upper = upper + offset;
}

auto InputBox::getLimits() -> std::tuple<Real3, Real3> {
    return std::make_tuple(lower, upper);
}

bool InputBox::withinGeometry(Real3 cord, lpReal_t radius) {
    return (
        cord.x - radius >= lower.x &&
        cord.y - radius >= lower.y &&
        cord.z - radius >= lower.z &&
        cord.x + radius <= upper.x &&
        cord.y + radius <= upper.y &&
        cord.z + radius <= upper.z);
}

bool InputBed::withinGeometry(Real3 cord, lpReal_t radius) {
    if (cord.z + radius <= bed->interp(cord.x, cord.y))
        return false;
    return true;
}

void InputBed::setOffset(Real3 offset) {

}

auto InputBed::getLimits() -> std::tuple<Real3, Real3> {
    Real3 lower = {bed->xMin, bed->yMin, bed->getMin()};
    Real3 upper = {bed->xMax, bed->yMax, std::numeric_limits<lpReal_t>::max()};
    return std::tuple<Real3, Real3>(lower, upper);
}

std::tuple<Map2d<lpReal_t>*, Map2d<lpReal_t>*> InputBed::getPtrs() {
    return std::make_tuple(bed, friction);
}

void InputBed::toVTKHDF(std::string path) {
    //
    VtkHdfWriter writer;
    if (getLocalRank() == 0) {
        writer.createFile(path, MPI_COMM_SELF);

        ManVec<lpReal_t> fricOut;
        for (int i = 0; i < bed->xPoints * bed->yPoints; ++i) {
            fricOut.push_back(friction->data[i]);
        }

        ManVec<lpReal_t> xPoints;
        ManVec<lpReal_t> yPoints;
        for (int y = 0; y < bed->yPoints; ++y) {
            for (int x = 0; x < bed->xPoints; ++x) {
                xPoints.push_back((x * bed->xRes) + bed->xMin);
                yPoints.push_back((y * bed->yRes) + bed->yMin);
            }
        }

        writer.writeQuadSurfaceSingle(xPoints.data(), yPoints.data(), bed->data, bed->xPoints, bed->yPoints,
                                      std::make_tuple("friction"), packVecsDataPtrs(fricOut));

    }
}
