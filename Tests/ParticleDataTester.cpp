//
// Created by Fredrik Robertsen on 09/10/2020.
//

#include <catch2/catch.hpp>

#include <Iterator.h>
#include <Util/Util.h>

#include "TestHelpers.h"

#include "../HiDEMLib/HiDEMTransportParticle.h"
#include "../HiDEMLib/HiDEMParticleData.h"


TEST_CASE( "Get limit test", "[Particle data]" ) {
    HiDEMParticleData data;

    generateBox(&data, 6, 1);

    auto [xMax, yMax, zMax, xMin, yMin, zMin] = data.getLimits();

    REQUIRE( xMax == 5 );
    REQUIRE( yMax == 5 );
    REQUIRE( zMax == 5 );
    REQUIRE( xMin == 0 );
    REQUIRE( yMin == 0 );
    REQUIRE( zMin == 0 );
}

TEST_CASE( "Test Reorder", "[Particle data]" ) {
    //

    HiDEMParticleData pData;

    ManVec<HiDEMTransportParticle> particles;

    HiDEMTransportParticle p;
    p.owner = 0;
    p.globalKey = 0;
    p.cord = {0,0,0};
    particles.push_back(p);
    p.globalKey = 1;
    p.cord = {1,0,0};
    particles.push_back(p);
    p.globalKey = 3;
    p.cord = {3,0,0};
    particles.push_back(p);
    p.owner = 1;
    p.globalKey = 2;
    p.cord = {2,0,0};
    particles.push_back(p);
    pData.appendTransportParticles(particles);

    REQUIRE(pData.globalKeys[0] == 0);
    REQUIRE(pData.globalKeys[1] == 1);
    REQUIRE(pData.globalKeys[2] == 3);
    REQUIRE(pData.globalKeys[3] == 2);

    ManVec<pInd_t > newOrder;
    newOrder.push_back(3);
    newOrder.push_back(1);
    newOrder.push_back(2);
    newOrder.push_back(0);
    pData.reorder(newOrder);

    REQUIRE(pData.globalKeys[0] == 2);
    REQUIRE(pData.globalKeys[1] == 1);
    REQUIRE(pData.globalKeys[2] == 3);
    REQUIRE(pData.globalKeys[3] == 0);

    pData.iterateOwnedParticles(SerialExec(), [](ind_t pInd, HiDEMParticleData &pData){
        REQUIRE(pData.owners[pInd] == 0);
    }, pData);

}

TEST_CASE( "Test dropping particles 1", "[Particle data]" ) {
    //
    HiDEMParticleData pData;

    ManVec<HiDEMTransportParticle> particles;

    HiDEMTransportParticle p;
    p.owner = 0;
    p.globalKey = 0;
    p.cord = {0,0,0};
    particles.push_back(p);
    p.globalKey = 1;
    p.cord = {1,0,0};
    particles.push_back(p);
    p.globalKey = 3;
    p.cord = {3,0,0};
    particles.push_back(p);
    p.owner = 1;
    p.globalKey = 2;
    p.cord = {2,0,0};
    particles.push_back(p);
    pData.appendTransportParticles(particles);

    ManVec<pInd_t > droppedInd;
    droppedInd.push_back(3);
    droppedInd.push_back(0);

    pData.dropParticles(droppedInd);

    int pCout = 0;
    pData.iterateAllParticles(SerialExec(), [&](ind_t pInd, HiDEMParticleData &pData){
        pCout++;
        REQUIRE(pData.globalKeys[pInd] != 0);
        REQUIRE(pData.globalKeys[pInd] != 2);
        }, pData);
    REQUIRE(pCout == 2);

}

TEST_CASE( "Test dropping particles non-owned", "[Particle data]" ) {
    //
    HiDEMParticleData pData;

    ManVec<HiDEMTransportParticle> particles;

    HiDEMTransportParticle p;
    p.owner = 0;
    p.globalKey = 0;
    p.cord = {0,0,0};
    particles.push_back(p);
    p.owner = 1;
    p.globalKey = 1;
    p.cord = {1,0,0};
    particles.push_back(p);
    p.owner = 2;
    p.globalKey = 3;
    p.cord = {3,0,0};
    particles.push_back(p);
    p.owner = 3;
    p.globalKey = 2;
    p.cord = {2,0,0};
    particles.push_back(p);
    pData.appendTransportParticles(particles);
    REQUIRE(pData.numLocal == 4);

    pData.dropNonOwned();
    REQUIRE(pData.numLocal == 1);
}

TEST_CASE( "Test getting particle map", "[Particle data]" ) {
    //
    HiDEMParticleData data;

    ManVec<HiDEMTransportParticle> particles;
    int key = 10;
    for (int x = 0; x < 6; ++x) {
        for (int y = 0; y < 6; ++y) {
            for (int z = 0; z < 6; ++z) {
                //
                HiDEMTransportParticle p;
                if (x<1 || x >= 6-1 ||
                    y<1 || y >= 6-1 ||
                    z<1 || z >= 6-1)
                {
                    p.owner = 1;
                }
                else
                {
                    p.owner = 0;
                }
                p.cord = {(double)x,(double)y,(double)z};
                p.globalKey = key;
                particles.push_back(p);
                key++;
            }
        }
    }

    data.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    data.remap(pMapper);

    REQUIRE(pMapper.getLocalInd(10) == 0);
}

TEST_CASE( "Test from and to HiDEMTransportParticle", "[Particle data]" ) {
    HiDEMParticleData data;

    ManVec<HiDEMTransportParticle> particlesIn;
    ManVec<HiDEMTransportParticle> particlesOut;
    int key = 10;
    for (int x = 0; x < 6; ++x) {
        for (int y = 0; y < 6; ++y) {
            for (int z = 0; z < 6; ++z) {
                //
                HiDEMTransportParticle p;
                if (x<1 || x >= 6-1 ||
                    y<1 || y >= 6-1 ||
                    z<1 || z >= 6-1)
                {
                    p.owner = 1;
                }
                else
                {
                    p.owner = 0;
                }
                p.cord = {(double)x,(double)y,(double)z};
                p.globalKey = key;
                particlesIn.push_back(p);
                key++;
            }
        }
    }

    data.appendTransportParticles(particlesIn);
    //

    for (int i = 0; i < particlesIn.size(); ++i) {
        particlesOut.push_back(data.toTransportParticle(i));
    }
    REQUIRE(particlesOut.size() == particlesIn.size());
    for (int i = 0; i < particlesIn.size(); ++i) {
        REQUIRE(particlesIn[i].cord.x == particlesOut[i].cord.x);
        REQUIRE(particlesIn[i].cord.y == particlesOut[i].cord.y);
        REQUIRE(particlesIn[i].cord.z == particlesOut[i].cord.z);

        REQUIRE(particlesIn[i].cordPrev.x == particlesOut[i].cordPrev.x);
        REQUIRE(particlesIn[i].cordPrev.y == particlesOut[i].cordPrev.y);
        REQUIRE(particlesIn[i].cordPrev.z == particlesOut[i].cordPrev.z);

        REQUIRE(particlesIn[i].theta.x == particlesOut[i].theta.x);
        REQUIRE(particlesIn[i].theta.y == particlesOut[i].theta.y);
        REQUIRE(particlesIn[i].theta.z == particlesOut[i].theta.z);

        REQUIRE(particlesIn[i].thetaPrev.x == particlesOut[i].thetaPrev.x);
        REQUIRE(particlesIn[i].thetaPrev.y == particlesOut[i].thetaPrev.y);
        REQUIRE(particlesIn[i].thetaPrev.z == particlesOut[i].thetaPrev.z);

        REQUIRE(particlesIn[i].owner == particlesOut[i].owner);
        REQUIRE(particlesIn[i].globalKey == particlesOut[i].globalKey);

    }
}