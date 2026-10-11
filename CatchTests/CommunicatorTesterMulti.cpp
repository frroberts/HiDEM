//
// Created by Fredrik Robertsen on 25/12/2020.
//

#include <catch2/catch.hpp>

#include <Transport/CommunicatorIndividual.h>
#include <Iterator.h>

#include <Domain.h>
#include <World.h>
#include <Transport/Transporter.h>
#include <Transport/LinkTransporter.h>
#include <Transport/ParticleTransporter.h>

#include "../HiDEMLib/HiDEMTransportParticle.h"
#include "../HiDEMLib/HiDEMParticleData.h"

#include "TestHelpers.h"

TEST_CASE( "Generating incomming indexes", "[Communicator2Rank]" ) {
    World world;
    Domain d1;
    d1.lower = {0,0,0};
    d1.upper = {5,1,1};
    d1.lowerHalo = d1.lower - Real3(1);
    d1.upperHalo = d1.upper + Real3(1);
    d1.owner = 0;
    world.domains.push_back(d1);
    Domain d2;
    d2.lower = {5,0,0};
    d2.upper = {10,1,1};
    d2.lowerHalo = d2.lower - Real3(1);
    d2.upperHalo = d2.upper + Real3(1);
    d2.owner = 1;
    world.domains.push_back(d2);
    //

    std::vector<HiDEMTransportParticle> particles;
    for (int i = 0; i < 10; ++i) {
        //
        HiDEMTransportParticle p;
        p.cord = {0.5 + i,0.5,0.5};
        p.globalKey = i;
        for (auto d: world.domains) {
            if (d.within(p.cord)) {
                p.owner = d.owner;
            }
        }
        particles.push_back(p);
    }

    HiDEMParticleData pData;
    pData.appendTransportParticles(particles);

    GlobalLocalMapper mapper;

    pData.remap(mapper);

    ParticleTrasporter<HiDEMParticleData, HiDEMTransportParticle> pTrans;
    pTrans.getOutgoingHalo(world, pData);
    pTrans.getIncomming();

    CommunicatorIndividual<double> pComm = pTrans.getCommunicator<double>();
    pComm.makeLocalIndexes(mapper);

    if(world.self == 0)
    {
        REQUIRE(pComm.incommingIndexes.size() == 1);
        REQUIRE(pComm.incommingIndexes[0] == 5);
    }
    else if(world.self == 1)
    {
        REQUIRE(pComm.incommingIndexes.size() == 1);
        REQUIRE(pComm.incommingIndexes[0] == 4);
    }
    else
    {
        WARN("MPI test run with incorrect amount of processes, expect it to fail");
    }
}


TEST_CASE( "Update remote particle data", "[Communicator2Rank]" ) {
    //
    World world;
    Domain d1;
    d1.lower = {0,0,0};
    d1.upper = {5,1,1};
    d1.lowerHalo = d1.lower - Real3(1);
    d1.upperHalo = d1.upper + Real3(1);
    d1.owner = 0;
    world.domains.push_back(d1);
    Domain d2;
    d2.lower = {5,0,0};
    d2.upper = {10,1,1};
    d2.lowerHalo = d2.lower - Real3(1);
    d2.upperHalo = d2.upper + Real3(1);
    d2.owner = 1;
    world.domains.push_back(d2);
    //

    std::vector<HiDEMTransportParticle> particles;
    for (int i = 0; i < 10; ++i) {
        HiDEMTransportParticle p;
        p.cord = {0.5 + i,0.5,0.5};
        p.globalKey = i;
        for (auto d: world.domains) {
            if (d.within(p.cord)) {
                p.owner = d.owner;
            }
        }
        particles.push_back(p);
    }

    HiDEMParticleData pData;
    pData.appendTransportParticles(particles);

    GlobalLocalMapper mapper;

    pData.remap(mapper);

    ParticleTrasporter<HiDEMParticleData, HiDEMTransportParticle> pTrans;
    pTrans.getOutgoingHalo(world, pData);
    pTrans.getIncomming();

    CommunicatorIndividual<double> pComm = pTrans.getCommunicator<double>();
    pComm.makeLocalIndexes(mapper);

    pComm.setupBuffers(1);
    pComm.packCommData(pData.xCord, 0);

    // see that we actually pack the correct data
    if(world.self == 0)
    {
        REQUIRE(pComm.outgoingBuffers[0][0] == 4.5);
    }
    else if(world.self == 1)
    {
        REQUIRE(pComm.outgoingBuffers[0][0] == 5.5);
    }

    // increment own data
    pData.iterateOwnedParticles(SerialExec(), [=](ind_t pInd, HiDEMParticleData &pData){
        //
        pData.xCord[pInd] += 0.1;
        }, pData);

    // repack data
    pComm.packCommData(pData.xCord, 0);
    if(world.self == 0)
    {
        REQUIRE(pComm.outgoingBuffers[0][0] == 4.6);
        REQUIRE(pComm.outgoingBuffers[0].size() == 1);
    }
    else if(world.self == 1)
    {
        REQUIRE(pComm.outgoingBuffers[0][0] == 5.6);
        REQUIRE(pComm.outgoingBuffers[0].size() == 1);
    }
    pComm.postRecvs();
    pComm.postSends();
    pComm.syncRecvs();
    pComm.syncSends();

    if(world.self == 0)
    {
        REQUIRE(pComm.incommingBuffers[0][0] == 5.6);
        REQUIRE(pComm.incommingBuffers[0].size() == 1);
    }
    else if(world.self == 1)
    {
        REQUIRE(pComm.incommingBuffers[0][0] == 4.6);
        REQUIRE(pComm.incommingBuffers[0].size() == 1);
    }
    pComm.unpackCommData(pData.xCord, 0);

    if(world.self == 0)
    {
        REQUIRE(pComm.incommingIndexes[0] == 5);
        REQUIRE(pData.xCord[5] == 5.6);
    }
    else if(world.self == 1)
    {
        REQUIRE(pComm.incommingIndexes[0] == 4);
        REQUIRE(pData.xCord[4] == 4.6);
    }
}



TEST_CASE( "Update remote link data", "[Communicator2Rank]" ) {

    World world;
    Domain d1;
    d1.lower = {0,0,0};
    d1.upper = {5,1,1};
    d1.lowerHalo = d1.lower - Real3(1);
    d1.upperHalo = d1.upper + Real3(1);
    d1.owner = 0;
    world.domains.push_back(d1);
    Domain d2;
    d2.lower = {5,0,0};
    d2.upper = {10,1,1};
    d2.lowerHalo = d2.lower - Real3(1);
    d2.upperHalo = d2.upper + Real3(1);
    d2.owner = 1;
    world.domains.push_back(d2);
    //

    std::vector<HiDEMTransportParticle> particles;
    for (int i = 0; i < 10; ++i) {
        HiDEMTransportParticle p;
        p.cord = {0.5 + i,0.5,0.5};
        p.globalKey = i;
        for (auto d: world.domains) {
            if (d.within(p.cord)) {
                p.owner = d.owner;
            }
        }
        particles.push_back(p);
    }

    HiDEMParticleData pData;
    pData.appendTransportParticles(particles);

    GlobalLocalMapper pMapper;

    pData.remap(pMapper);

    HiDEMLinkData lData;
    std::vector<HiDEMTransportLink> links;
    for (int i = 0; i < 9; ++i) {
        HiDEMTransportLink l;
        l.state = 1;
        l.globalKey = i;
        l.globalKeyParticleA = i;
        l.globalKeyParticleB = i + 1;
        if(i < 5)
            l.owner = 0;
        else
            l.owner = 1;
        links.push_back(l);
    }
    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);
    //lData.
    // get the lMapper updated here, otherwise it fails
    GlobalLocalMapper lMapper;
    // only need to remap halo links
    lData.remapHalos(lMapper, pData);


    LinkTransporter lTrans;
    lTrans.getOutgoingHalo(world, pData, lData);
    lTrans.getIncomming();
    CommunicatorIndividual<uint8_t> lComm = lTrans.getCommunicator<uint8_t>();
    lComm.makeLocalIndexes(lMapper);

    //std::cout << " size " << lMapper.globalToLocal.size() << std::endl;

    if(world.self == 0)
    {
        REQUIRE(lComm.outgoingIndexes.size() == 1);
        REQUIRE(lComm.outgoingIndexes[0] == 4);


    }
    else if(world.self == 1)
    {
        REQUIRE(lComm.outgoingIndexes.size() == 0);
        REQUIRE(lComm.incommingIndexes.size() == 1);
        REQUIRE(lComm.incommingIndexes[0] == 4);
    }

    REQUIRE(lData.state[4] == 1);

    // change on rank 0
    if(world.self == 0)
    {
        lData.state[4] = 0;
    }

    lComm.setupBuffers(1);
    lComm.packCommData(lData.state, 0);
    lComm.postRecvs();
    lComm.postSends();
    lComm.syncSends();
    lComm.syncRecvs();
    lComm.unpackCommData(lData.state, 0);

    // should have propagated to rank 1
    REQUIRE(lData.state[4] == 0);

}


TEST_CASE( "Reverse update CT data", "[Communicator2Rank]" ) {
    World world;
    Domain d1;
    d1.lower = {0,0,0};
    d1.upper = {5,1,1};
    d1.lowerHalo = d1.lower - Real3(1);
    d1.upperHalo = d1.upper + Real3(1);
    d1.owner = 0;
    world.domains.push_back(d1);
    Domain d2;
    d2.lower = {5,0,0};
    d2.upper = {10,1,1};
    d2.lowerHalo = d2.lower - Real3(1);
    d2.upperHalo = d2.upper + Real3(1);
    d2.owner = 1;
    world.domains.push_back(d2);
    //

    std::vector<HiDEMTransportParticle> particles;
    for (int i = 0; i < 10; ++i) {
        HiDEMTransportParticle p;
        p.cord = {0.5 + i,0.5,0.5};
        p.globalKey = i;
        for (auto d: world.domains) {
            if (d.within(p.cord)) {
                p.owner = d.owner;
            }
        }
        particles.push_back(p);
    }

    HiDEMParticleData pData;
    pData.appendTransportParticles(particles);

    GlobalLocalMapper pMapper;

    pData.remap(pMapper);

    HiDEMLinkData lData;
    std::vector<HiDEMTransportLink> links;
    for (int i = 0; i < 9; ++i) {
        HiDEMTransportLink l;
        l.state = 1;
        l.globalKey = i;
        l.globalKeyParticleA = i;
        l.globalKeyParticleB = i + 1;
        for (int j = 0; j < 2; ++j) {
            for (int k = 0; k < 6; ++k) {
                l.CT[j][k] = 0;
            }
        }
        if(i < 5)
            l.owner = 0;
        else
            l.owner = 1;
        links.push_back(l);
    }
    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);
    //lData.
    // get the lMapper updated here, otherwise it fails
    GlobalLocalMapper lMapper;
    // only need to remap halo links
    lData.remapHalos(lMapper, pData);


    LinkTransporter lTrans;
    lTrans.getOutgoingHalo(world, pData, lData);
    lTrans.getIncomming();
    CommunicatorIndividual<bool> lComm = lTrans.getCommunicator<bool>();
    lComm.makeLocalIndexes(lMapper);


    auto lCTComm = lTrans.getCommunicator<double>();
    lCTComm.invert();
    lCTComm.tagStart = 64 + 12 + 12;
    lCTComm.setNumBuffers(6);
    lCTComm.makeLocalIndexes(lMapper);
    lCTComm.indirectIndexes(lData.sideIndEndA, lData.sideIndEndA);


    for(auto a: lCTComm.outgoingIndexes){
        std::cout << getLocalRank() << " index " << a << std::endl;
    }


    pData.iterateOwnedParticles(SerialExec(), [&](ind_t pInd, HiDEMParticleData &pData){
        //
        lData.iterateParticleLinksSideInd([&](ind_t lInd, ind_t ctInd, ind_t pIndOther, HiDEMLinkData &lData) {
            //
            lData.CT0_flat[ctInd] = 1;
        }, pInd, lData);
        }, pData);
    auto lTest = lData.toTransportLink(4);

        std::cout << getLocalRank() << " " << lTest.CT[0][0] << " " << lTest.CT[1][0] << std::endl;

        // these are reversed ?
    if(getLocalRank() == 0) {
        REQUIRE(lTest.CT[0][0] == 1);
        REQUIRE(lTest.CT[1][0] == 0);
    } else if (getLocalRank() == 1)
    {
        REQUIRE(lTest.CT[1][0] == 1);
        REQUIRE(lTest.CT[0][0] == 0);
    }
    lCTComm.packCommData(lData.CT0_flat, 0);
    Iterator::syncAll();

    lCTComm.postRecvs();
    lCTComm.postSends();
    lCTComm.syncRecvs();
    lCTComm.syncSends();
    lCTComm.unpackCommData(lData.CT0_flat, 0);

    for (int i = 0; i < lCTComm.outgoingBuffers[0].size(); ++i) {


        std::cout << getLocalRank() << " communicating " << lCTComm.outgoingBuffers[0][i] << std::endl;
    }
   for (int i = 0; i < lCTComm.incommingBuffers[0].size(); ++i) {


        std::cout << getLocalRank() << " communicating " << lCTComm.incommingBuffers[0][i] << std::endl;
    }

    lTest = lData.toTransportLink(4);

   if(getLocalRank() == 0) {
        REQUIRE(lTest.CT[0][0] == 0);
        REQUIRE(lTest.CT[1][0] == 0);
    } else if (getLocalRank() == 1)
    {
        REQUIRE(lTest.CT[1][0] == 1);
        REQUIRE(lTest.CT[0][0] == 0);
    }
    //std::cout << getLocalRank() << " " << lTest.CT[0][0] << " " << lTest.CT[1][0] << std::endl;

    // assign them to different processes
    // one link between them
    // update CT on the one not owning the link
    // reverse syncAll it

}