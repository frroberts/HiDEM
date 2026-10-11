//
// Created by Fredrik Robertsen on 09/10/2020.
//

#ifndef PHELL_SPHTRANSPORTPARTICLE_H
#define PHELL_SPHTRANSPORTPARTICLE_H

#include <vector>
#include "../Util/Real3.h"
#include "../PLiB/Global.h"

class SPHTransportParticle {
public:
    //
    Real3 cord; // the coordinate, this is needed as a minimum
    Real3 cordPrev; // the coordinate
    Real3 vel; // the coordinate
    Real3 acc; // the coordinate
    Real3 force; // the coordinate

    int owner; // who owns it, this is needed as a minimum
    gKey_t globalKey; // is not nessecarily consecutive, this is needed as a minimum

    void initConstruct(Real3 cord_, int owner_, gKey_t globalKey_)
    {
        //
        cord = cord_;
        owner = owner_;
        globalKey = globalKey_;
        cordPrev = cord;
        vel = 0;
        acc = 0;
    }
};


#endif //PHELL_SPHTRANSPORTPARTICLE_H
