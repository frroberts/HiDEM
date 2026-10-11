//
// Created by Fredrik Robertsen on 6/26/24.
//

#ifndef PHELL_ELEMENTRAYLINK_H
#define PHELL_ELEMENTRAYLINK_H

#include "../Global.h"

class ElementaryLink
{
public:
    //
    DEVCALLABLE
    ElementaryLink() { };
    DEVCALLABLE
    ElementaryLink(gKey_t aInd, gKey_t bInd): globalKeyParticleA(aInd),globalKeyParticleB(bInd)
    {
        //
    }
    gKey_t globalKey;
    gKey_t globalKeyParticleA;
    gKey_t globalKeyParticleB;
    int particleLinkNum;
    int owner;
    lpReal_t initialLen;
    uint8_t state; // inactive links can essentially be dropped at transport times..
};

#endif //PHELL_ELEMENTRAYLINK_H
