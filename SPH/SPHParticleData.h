//
// Created by Fredrik Robertsen on 08/07/2021.
//

#ifndef PHELL_SPHPARTICLEDATA_H
#define PHELL_SPHPARTICLEDATA_H

#include "../PLiB/ParticleData.h"
#include "SPHTransportParticle.h"

class SPHParticleData : public ParticleData{
public:
    ManVec<double> xVel;
    ManVec<double> yVel;
    ManVec<double> zVel;

    ManVec<double> xAcc;
    ManVec<double> yAcc;
    ManVec<double> zAcc;

    void appendTransportParticles(const std::vector<SPHTransportParticle> &particles);
    void appendTransportParticles(ManVec<SPHTransportParticle> &particles);

    void appendTransportParticle(const SPHTransportParticle &p);

    SPHTransportParticle toTransportParticle(ind_t pInd);


};

void SPHParticleData::appendTransportParticles(const std::vector<SPHTransportParticle> &particles)
{
    //
    for(const auto &p: particles) {
        appendTransportParticle(p);
    }
}

void SPHParticleData::appendTransportParticles(ManVec<SPHTransportParticle> &particles)
{
    //
    for(auto &p: particles) {
        appendTransportParticle(p);
    }
}


void SPHParticleData::appendTransportParticle(const SPHTransportParticle &p) {
    xCord.push_back(p.cord.x);
    yCord.push_back(p.cord.y);
    zCord.push_back(p.cord.z);

    xCordPrev.push_back(p.cordPrev.x);
    yCordPrev.push_back(p.cordPrev.y);
    zCordPrev.push_back(p.cordPrev.z);

    xVel.push_back(0);
    yVel.push_back(0);
    zVel.push_back(0);

    xAcc.push_back(0);
    yAcc.push_back(0);
    zAcc.push_back(0);

    xForce.push_back(0);
    yForce.push_back(0);
    zForce.push_back(0);

    // if owned
    if (p.owner == getLocalRank()) {
        ownedIndexes.push_back(numLocal);
    } else {
        nonOwnedIndexes.push_back(numLocal);
    }

    owners.push_back(p.owner);
    globalKeys.push_back(p.globalKey);
    // finally increment the local count

    numLocal++;
}

SPHTransportParticle SPHParticleData::toTransportParticle(ind_t pInd) {
    SPHTransportParticle ret;

    ret.cord = {xCord[pInd], yCord[pInd], zCord[pInd]};
    ret.cordPrev = {xCordPrev[pInd], yCordPrev[pInd], zCordPrev[pInd]};
    ret.acc = {xAcc[pInd], yAcc[pInd], zAcc[pInd]};
    ret.vel = {xVel[pInd], yVel[pInd], zVel[pInd]};
    ret.force = {xForce[pInd], yForce[pInd], zForce[pInd]};
    ret.globalKey = globalKeys[pInd];
    ret.owner = owners[pInd];

    return ret;
}

#endif //PHELL_SPHPARTICLEDATA_H
