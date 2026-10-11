//
// Created by Fredrik Robertsen on 05/01/2021.
//

#include <catch2/catch.hpp>

#include <Transport/Transporter.h>
#include <Transport/Communicator.h>
#include <Transport/ParticleTransporter.h>

#include <Domain.h>
#include <World.h>

#include "TestHelpers.h"

#include "../HiDEMLib/HiDEMTransportParticle.h"
#include "../HiDEMLib/HiDEMParticleData.h"

TEST_CASE( "Generating outgoing Halo Particles", "[TransporterSingle]" ) {
    //
    World world;
    //world.self = 0;
    Domain d1;
    d1.lower = {0,0,0};
    d1.upper = {1,1,1};
    d1.lowerHalo = {-1,-1,-1};
    d1.upperHalo = {2,2,2};
    d1.owner = 0;
    world.domains.push_back(d1);
    Domain d2;
    d2.lower = {0,0,1};
    d2.upper = {1,1,2};
    d2.lowerHalo = {-1,-1,0};
    d2.upperHalo = {2,2,3};
    d2.owner = 1;
    world.domains.push_back(d2);
    //
    HiDEMTransportParticle p1;
    p1.cord = {0.5};
    p1.owner = 0;
    p1.globalKey = 0;
    HiDEMTransportParticle p2;
    p2.cord = {0.5,0.5,1.5};
    p2.owner = 1;
    p2.globalKey = 1;


    ManVec<HiDEMTransportParticle> particles;
    particles.push_back(p1);
    //particles.push_back(p2);

    HiDEMParticleData pData;
    pData.appendTransportParticles(particles);

    ParticleTrasporter<HiDEMParticleData, HiDEMTransportParticle> pTrans;
    pTrans.getOutgoingHalo(world, pData);

    // just one target
    REQUIRE(pTrans.outTargets.size() == 1);
    // just one index going there
    REQUIRE(pTrans.outTargets[0].objects.size() == 1);
    // that index is 0
    REQUIRE(pTrans.outTargets[0].objects[0].globalKey == 0);
    // rank the data goes to
    REQUIRE(pTrans.outTargets[0].targetRank == 1);
}

