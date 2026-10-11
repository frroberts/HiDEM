//
// Created by Fredrik Robertsen on 25/12/2020.
//

#include <catch2/catch.hpp>

#include <Transport/Communicator.h>
#include <Transport/Transporter.h>

#include <Domain.h>
#include <World.h>

#include <Transport/ParticleTransporter.h>
#include <Transport/LinkTransporter.h>

#include "TestHelpers.h"

#include "../HiDEMLib/HiDEMTransportParticle.h"
#include "../HiDEMLib/HiDEMParticleData.h"
#include "../HiDEMLib/HiDEMLinkData.h"

TEST_CASE( "Copy Particle data to outgoing buffers", "[CommunicatorSingle]" ) {

    HiDEMParticleData pData;

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

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ParticleTrasporter<HiDEMParticleData, HiDEMTransportParticle> pTrans;
    pTrans.getOutgoingHalo(world, pData);

    auto pComm = pTrans.getCommunicator<double>();
    pComm.makeLocalIndexes(pMapper);

    REQUIRE(pComm.outgoingIndexes.size() == 1);
    REQUIRE(pComm.outgoingIndexes[0] == 0);

    pComm.setNumBuffers(1);

    pComm.packCommData(pData.xCord, 0);

    REQUIRE(pComm.outgoingBuffers[0].size() == 1);
    REQUIRE(pComm.outgoingBuffers[0][0] == 0.5);


}


TEST_CASE( "Generating outgoing Link indexes", "[CommunicatorSingle]" ) {
    //

    World world;
    world.self = 0;
    Domain d1;
    d1.lower = {0, 0, 0};
    d1.upper = {1, 1, 1};
    d1.lowerHalo = {-1, -1, -1};
    d1.upperHalo = {2, 2, 2};
    d1.owner = 0;
    world.domains.push_back(d1);
    Domain d2;
    d2.lower = {0, 0, 1};
    d2.upper = {1, 1, 2};
    d2.lowerHalo = {-1, -1, 0};
    d2.upperHalo = {2, 2, 3};
    d2.owner = 1;
    world.domains.push_back(d2);
    //
    HiDEMTransportParticle p1;
    p1.cord = {0.5};
    p1.owner = 0;
    p1.globalKey = 0;
    HiDEMTransportParticle p2;
    p2.cord = {0.5, 0.5, 1.5};
    p2.owner = 1;
    p2.globalKey = 1;


    ManVec<HiDEMTransportParticle> particles;
    particles.push_back(p1);
    particles.push_back(p2);

    HiDEMParticleData pData;
    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    HiDEMLinkData lData;

    HiDEMTransportLink l1;
    l1.globalKeyParticleA = 0;
    l1.globalKeyParticleB = 1;
    l1.owner = 0;
    l1.state = 1;
    l1.globalKey = 0;
    ManVec<HiDEMTransportLink> links;
    links.push_back(l1);

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    GlobalLocalMapper<gKey_t, lInd_t> lMapper;

    LinkTransporter<HiDEMParticleData, HiDEMLinkData, HiDEMTransportLink>  lTrans;
    lTrans.getOutgoingHalo(world, pData, lData);
    lData.remap(lMapper);

    auto lComm = lTrans.getCommunicator<double>();
    lComm.makeLocalIndexes(lMapper);
    // just one target
    REQUIRE(lComm.outgoingTargets.size() == 1);
    // rank the data goes to
    REQUIRE(lComm.outgoingTargets.begin()->first == 1);
    // index of that rank
    REQUIRE(lComm.outgoingTargets.begin()->second == 0);

    REQUIRE(lComm.outgoingIndexes.size() == 1);
    REQUIRE(lComm.outgoingIndexes[0] == 0);
}
