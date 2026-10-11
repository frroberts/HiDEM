//
// Created by Fredrik Robertsen on 16/02/2022.
//

#include "InputGeometry.h"


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
