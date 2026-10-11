//
// Created by Fredrik Robertsen on 24/01/2021.
//

#ifndef PHELL_DISTRIBUTION_H
#define PHELL_DISTRIBUTION_H


#include <memory>
#include <World.h>
#include "HiDEMParticleData.h"
#include "HiDEMLinkData.h"
#include <Transport/Transporter.h>
#include <Geometry/Map2d.h>
#include <NeighbourGrid.h>

#define maxBB 1000

class HiDEMDistribution : public ManagedParent{
    //
public:
    Communicator<ctReal_t> lCTComm;
    Communicator<lpReal_t> pForceComm;
    Domain bounding;

    bool reBalanceActive;

    ManVec<double> xCord;
    ManVec<double> yCord;
    ManVec<double> zCord;
    ManVec<double> weights;

    NeighbourGrid *grids[4];

    virtual void syncCT(HiDEMLinkData &lData, GlobalLocalMapper<gKey_t, lInd_t> &lMapper);
public:
    World world;

    explicit HiDEMDistribution(bool reBalance, double searchDistance);

    //
    void distribute(HiDEMParticleData &pData, HiDEMLinkData &lData, GlobalLocalMapper<gKey_t, pInd_t> &pMapper, GlobalLocalMapper<gKey_t, lInd_t> &lMapper);

    void reDistribute(HiDEMParticleData &pData, HiDEMLinkData &lData, GlobalLocalMapper<gKey_t, pInd_t> &pMapper,
                              GlobalLocalMapper<gKey_t, lInd_t> &lMapper, double haloSize, double redistTolerance, bool CTSync);
    void gatherHaloParticles(OffloadExec &execPolicy, HiDEMParticleData &pData);

    void scatterHaloParticles(OffloadExec &execPolicy, HiDEMParticleData &pData);

    void sendReceiveParticles();

    void syncHaloParticles();


    HiDEMDistribution(const HiDEMDistribution &old_obj) = delete;

    HiDEMDistribution(HiDEMDistribution &&) = delete;

    HiDEMDistribution() = default;

};


#endif //PHELL_DISTRIBUTION_H
