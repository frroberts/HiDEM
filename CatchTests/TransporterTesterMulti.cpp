//
// Created by Fredrik Robertsen on 22/01/2021.
//


#include <catch2/catch.hpp>

#include <Iterator.h>
#include <Domain.h>
#include <World.h>

#include <Transport/CommunicatorIndividual.h>
#include <Transport/Transporter.h>
#include <Transport/LinkTransporter.h>
#include <Transport/ParticleTransporter.h>

#include "TestHelpers.h"

#include "../HiDEMLib/HiDEMTransportParticle.h"
#include "../HiDEMLib/HiDEMTransportLink.h"
#include "../HiDEMLib/HiDEMParticleData.h"

TEST_CASE( "Test full 2 rank reassign and transport and halo transport", "[Trasnporter2Rank]" ) {


    World world;
    if(world.size != 2) {
        WARN("MPI test run with incorrect amount of processes, expect it to fail");
    }
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

    std::vector<HiDEMTransportParticle> particles;
    std::vector<HiDEMTransportLink> links;

    if (world.self == 0) {
        for (int i = 0; i < 10; ++i) {
            //
            HiDEMTransportParticle p;
            p.cord = {0.5 + i, 0.5, 0.5};
            p.globalKey = i;
            p.owner = 0;
            particles.push_back(p);
        }
        for (int i = 0; i < 9; ++i) {
            HiDEMTransportLink l;
            l.state = 1;
            l.globalKey = i;
            l.globalKeyParticleA = i;
            l.globalKeyParticleB = i + 1;
            l.owner = 0;
            links.push_back(l);
        }
    }
    HiDEMParticleData pData;
    pData.appendTransportParticles(particles);

    HiDEMLinkData lData;
    lData.appendInitLinks(links);

    GlobalLocalMapper pMapper;
    pData.remap(pMapper);

    lData.connectParticleLinks(pMapper);

    ParticleTrasporter<HiDEMParticleData, HiDEMTransportParticle> pTrans;
    pTrans.reassignAndGetOutgoing(world, pData);

    // reassign and move links here
    LinkTransporter lTrans;
    lTrans.reassignAndGetOutgoing(world, pData, lData);

    pData.dropNonOwned();
    lData.dropNonOwnedLinks();

    pTrans.getIncomming();
    pTrans.appendIncomming(pData);
    pData.remap(pMapper);


    SECTION( "Check initial particle redistribution" ) {
        REQUIRE(pData.numLocal == 5);
        pData.iterateOwnedParticles(SerialExec() ,[&](ind_t pInd, HiDEMParticleData &pData) {
            REQUIRE(pData.owners[pInd] == world.self);
        }, pData);
    }

    lTrans.getIncomming();
    lTrans.appendIncomming(lData);
    //lData.connectParticleLinks(pMapper);
    
    SECTION( "Check initial link redistribution" ) {
        if (world.self == 0) {
            REQUIRE(lData.numLocal == 5);
            lData.iterateAllLinks(SerialExec(), [&](ind_t lInd, ind_t indA, ind_t indB, HiDEMLinkData &lData) {
                REQUIRE(lData.globalKeyEndA[lInd] < 5);
                REQUIRE(lData.owners[lInd] == 0);
            }, lData);
        } else if (world.self == 1) {
            REQUIRE(lData.numLocal == 4);
            lData.iterateAllLinks(SerialExec(), [&](ind_t lInd, ind_t indA, ind_t indB, HiDEMLinkData &lData) {
                REQUIRE(lData.globalKeyEndA[lInd] >= 5);
                REQUIRE(lData.owners[lInd] == 1);
            }, lData);
        }
    }


    ParticleTrasporter<HiDEMParticleData, HiDEMTransportParticle> pTransHalo;
    pTransHalo.getOutgoingHalo(world, pData);
    pTransHalo.getIncomming();
    pTransHalo.appendIncomming(pData);
    pData.remap(pMapper);

    SECTION( "Check halo particle redistribution" ) {
        REQUIRE(pData.numLocal == 6);
        if (world.self == 0) {
            pData.iterateOwnedParticles(SerialExec() ,[&](ind_t pInd, HiDEMParticleData &pData) {
                REQUIRE(pData.xCord[pInd] <= 6);
            }, pData);
        } else if (world.self == 1) {
            pData.iterateOwnedParticles(SerialExec() ,[&](ind_t pInd, HiDEMParticleData &pData) {
                REQUIRE(pData.xCord[pInd] >= 4);
            }, pData);
        }
    }

    lData.connectParticleLinks(pMapper);
    LinkTransporter lTransHalo;
    lTransHalo.getOutgoingHalo(world, pData, lData);
    lTransHalo.getIncomming();
    lTransHalo.appendIncomming(lData);
    lData.connectParticleLinks(pMapper);


    SECTION( "Check halo link redistribution" ) {
        if (world.self == 0) {
            REQUIRE(lData.numLocal == 5);
            REQUIRE(lData.ownedIndexes.size() == 5);
            lData.iterateAllLinks(SerialExec(), [&](ind_t lInd, ind_t indA, ind_t indB, HiDEMLinkData &lData) {
                REQUIRE(lData.globalKeyEndA[lInd] < 6);
            }, lData);
        } else if (world.self == 1) {
            REQUIRE(lData.numLocal == 5);
            REQUIRE(lData.ownedIndexes.size() == 4);
            lData.iterateAllLinks(SerialExec(), [&](ind_t lInd, ind_t indA, ind_t indB, HiDEMLinkData &lData) {
                REQUIRE(lData.globalKeyEndA[lInd] >= 4);
            }, lData);
        }
    }

    SECTION("Check halo link connections") {
        int numIterated = 0;
        pData.iterateOwnedParticles(SerialExec() ,[&](ind_t pInd, HiDEMParticleData &pData) {
            int numLinked = 0;
            if (pData.globalKeys[pInd] == 0 || pData.globalKeys[pInd] == 9)
                return;
            lData.iterateParticleLinks([&](ind_t lInd, ind_t pIndOhter, HiDEMLinkData &ldata){
                numLinked++;
            }, pInd, lData);
            REQUIRE(numLinked == 2);
            numIterated++;
        }, pData);
        REQUIRE(numIterated == 4);

        if(world.self == 0)
        {
            int numLinkedHalo = 0;
            int otherOwner = -1;
            ind_t pInd = pMapper.getLocalInd(4);
            lData.iterateParticleLinks([&](ind_t lInd, ind_t pIndOhter, HiDEMLinkData &ldata){
                if(lData.owners[pIndOhter] != world.self) {
                    numLinkedHalo++;
                    otherOwner = lData.owners[pIndOhter];
                }
            }, pInd, lData);
            REQUIRE(numLinkedHalo == 1);
            REQUIRE(otherOwner == 1);
        }
        if(world.self == 1)
        {
            int numLinkedHalo = 0;
            int otherOwner = -1;
            ind_t pInd = pMapper.getLocalInd(5);
            lData.iterateParticleLinks([&](ind_t lInd, ind_t pIndOhter, HiDEMLinkData &ldata){
                if(lData.owners[pIndOhter] != world.self) {
                    numLinkedHalo++;
                    otherOwner = pData.owners[pIndOhter];
                }
            }, pInd, lData);
            REQUIRE(numLinkedHalo == 1);
            REQUIRE(otherOwner == 0);
        }
    }
}