//
// Created by Fredrik Robertsen on 25/02/2022.
//

#include "CompoundInput.h"

void InputOR::setOffset(Real3 offset) {
    // do nothing, offsets should be set to the underlying objects separately
}

auto InputOR::getLimits() -> std::tuple<Real3, Real3> {
    auto [aMin, aMax] = a->getLimits();
    auto [bMin, bMax] = b->getLimits();

    Real3 min;
    min.x = std::min(aMin.x, bMin.x);
    min.y = std::min(aMin.y, bMin.y);
    min.z = std::min(aMin.z, bMin.z);

    Real3 max;
    max.x = std::max(aMax.x, bMax.x);
    max.y = std::max(aMax.y, bMax.y);
    max.z = std::max(aMax.z, bMax.z);
    return std::make_tuple(min, max);
}

bool InputOR::withinGeometry(Real3 cord, lpReal_t radius) {
    return a->withinGeometry(cord, radius) || b->withinGeometry(cord, radius);
}



void InputAND::setOffset(Real3 offset) {
    // do nothing, offsets should be set to the underlying objects separately
}

auto InputAND::getLimits() -> std::tuple<Real3, Real3> {
    auto [aMin, aMax] = a->getLimits();
    auto [bMin, bMax] = b->getLimits();

    Real3 min;
    min.x = std::min(aMin.x, bMin.x);
    min.y = std::min(aMin.y, bMin.y);
    min.z = std::min(aMin.z, bMin.z);

    Real3 max;
    max.x = std::max(aMax.x, bMax.x);
    max.y = std::max(aMax.y, bMax.y);
    max.z = std::max(aMax.z, bMax.z);
    return std::make_tuple(min, max);
}

bool InputAND::withinGeometry(Real3 cord, lpReal_t radius) {
    return a->withinGeometry(cord, radius) && b->withinGeometry(cord, radius);
}


void InputNOT::setOffset(Real3 offset) {
    // do nothing, offsets should be set to the underlying objects separately
}

auto InputNOT::getLimits() -> std::tuple<Real3, Real3> {
    // just the bounding of A, we don't need to account for Bs size
    return a->getLimits();
}

bool InputNOT::withinGeometry(Real3 cord, lpReal_t radius) {
    return a->withinGeometry(cord, radius) && !b->withinGeometry(cord, radius);
}
