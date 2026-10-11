//
// Created by Fredrik Robertsen on 6/15/24.
//

#ifndef PHELL_ELEMENTARYPARTICLE_H
#define PHELL_ELEMENTARYPARTICLE_H


#include "../Util/Real3.h"
#include "../Global.h"
#include <cstdint>


class ElementaryParticle {
public:
    Real3 cord; // the coordinate, this is needed as a minimum
    double radius;
    uint8_t type;
    int owner;
    gKey_t globalKey;

        void initConstruct(Real3 cord, int owner, double radius)
    {
        //
        this->cord = cord;
        this->owner = owner;
        this->type = 0;
        this->radius = radius;
    }
};


#endif //PHELL_ELEMENTARYPARTICLE_H
