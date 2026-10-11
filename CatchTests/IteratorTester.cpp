//
// Created by Fredrik Robertsen on 09/10/2020.
//

#include <catch2/catch.hpp>

#include <iostream>
#include <set>

#include <Iterator.h>
#include <NeighbourGrid.h>
#include <Util/Util.h>

#include "../HiDEMLib/HiDEMTransportParticle.h"
#include "../HiDEMLib/HiDEMParticleData.h"
#include "../HiDEMLib/HiDEMLinkData.h"

#include "TestHelpers.h"


TEST_CASE( "Owned particle iterator tester", "[Particle iterators]" ) {
//    REQUIRE( Factorial(10) == 3628800 );

    HiDEMParticleData data;
    ManVec<HiDEMTransportParticle> particles;
    // make a non owned partile
    HiDEMTransportParticle p0;
    p0.cord = {0,1,0};
    p0.owner = 1;
    p0.globalKey = 3;
    particles.push_back(p0);

    // make an owned particle
    HiDEMTransportParticle p1;
    p1.cord = {0,0,0};
    p1.owner = 0;
    p1.globalKey = 0;
    particles.push_back(p1);

    // make a non owned partile
    HiDEMTransportParticle p2;
    p2.cord = {1,0,0};
    p2.owner = 1;
    p2.globalKey = 1;
    particles.push_back(p2);

    data.appendTransportParticles(particles);

    int count = 0;

    data.iterateOwnedParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        count++;
        REQUIRE(data.owners[pInd] == 0);
    }, data);
    REQUIRE(count == 1);
}

TEST_CASE( "Box generator tester", "[Particle iterators]" ) {
    //
    HiDEMParticleData data;
    generateBox(&data, 3, 1);

    // there should be 27 particles
    REQUIRE(data.numLocal == 27);
    int count = 0;
    data.iterateOwnedParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        count++;
        REQUIRE(data.owners[pInd] == 0);
    }, data);
    // only one should be owned
    REQUIRE(count == 1);
}

TEST_CASE( "All particle iterator tester", "[Particle iterators]" ) {
    HiDEMParticleData data;
    generateBox(&data, 3, 1);

    // there should be 27 particles
    REQUIRE(data.numLocal == 27);
    int count = 0;
    data.iterateAllParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        count++;
    }, data);
    // only one should be owned
    REQUIRE(count == 27);
}

TEST_CASE( "Iterate possible iteratedNeighbours indirect tester", "[Particle iterators]" ) {
    HiDEMParticleData data;
    generateBox(&data, 5, 2, true);
    //
    NeighbourGrid grid;
    grid.generate(data, 1.0);
    grid.colourParticles(data);
    grid.generateIterationList(data);

    int ownedCount = 0;
    int neighbourCount = 0;
    // since the only owned article is the center one, this should only iterate over the center particle

    data.iterateOwnedParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        ownedCount++;
        REQUIRE(data.owners[pInd] == 0);
        REQUIRE(data.xCord[pInd] == 2);
        REQUIRE(data.yCord[pInd] == 2);
        REQUIRE(data.zCord[pInd] == 2);

        int colour = grid.getPraticleColor(pInd);
        //std::cout << colour << std::endl;
        grid.iteratePossibleNeighbours([&](size_t nInd) {
            neighbourCount++;
            // neighbour particles should be within +-1 of the current one
            REQUIRE(data.xCord[nInd] <= data.xCord[pInd] + 1);
            REQUIRE(data.xCord[nInd] >= data.xCord[pInd] - 1);
            REQUIRE(data.yCord[nInd] <= data.yCord[pInd] + 1);
            REQUIRE(data.yCord[nInd] >= data.yCord[pInd] - 1);
            REQUIRE(data.zCord[nInd] <= data.zCord[pInd] + 1);
            REQUIRE(data.zCord[nInd] >= data.zCord[pInd] - 1);

        }, pInd, colour, true);
    }, data);
    // only one should be owned
    REQUIRE(ownedCount == 1);
    // there should be 27 iteratedNeighbours, counting it self
    REQUIRE(neighbourCount == 26);
}


TEST_CASE( "Iterate possible iteratedNeighbours reordered tester", "[Particle iterators]" ) {
    HiDEMParticleData data;
    generateBox(&data, 5, 2, true);

    NeighbourGrid grid;
    grid.generate(data, 1.0);
    grid.colourParticles(data);
    grid.generateIterationList(data);

    int ownedCount = 0;
    int neighbourCount = 0;
    std::vector<Real3> neighbourCordsExpected;


    data.iterateOwnedParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        int colour = grid.getPraticleColor(pInd);
        grid.iteratePossibleNeighbours([&](size_t nInd) {
            // neighbour particles should be within +-1 of the current one
            neighbourCordsExpected.push_back({data.xCord[nInd], data.yCord[nInd], data.zCord[nInd]});
        }, pInd, colour, true);
    }, data);

    REQUIRE(grid.iterationOrder.size() == data.xCord.size());

    data.reorder(grid.iterationOrder);

    grid.reoder(grid.iterationOrder);

    std::vector<Real3> neighbourCords;

    data.iterateOwnedParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        ownedCount++;
        REQUIRE(data.owners[pInd] == 0);
        REQUIRE(data.xCord[pInd] == 2);
        REQUIRE(data.yCord[pInd] == 2);
        REQUIRE(data.zCord[pInd] == 2);

        int colour = grid.getPraticleColor(pInd);

        grid.iteratePossibleNeighbours([&](size_t nInd) {
            neighbourCount++;
            neighbourCords.push_back({data.xCord[nInd], data.yCord[nInd], data.zCord[nInd]});
        }, pInd, colour, false);
    }, data);

    for(Real3 p: neighbourCordsExpected)
    {
        auto pos = std::find_if(neighbourCords.begin(), neighbourCords.end(), [&](Real3 cord){
            return cord.x == p.x && cord.y == p.y && cord.z == p.z;});
        REQUIRE(pos != neighbourCords.end());
    }

    // only one should be owned
    REQUIRE(ownedCount == 1);
    // there should be 27 iteratedNeighbours, counting it self
    REQUIRE(neighbourCount == 26);



}

TEST_CASE( "Iterate possible iteratedNeighbours reordered indirect tester", "[Particle iterators]" ) {
    HiDEMParticleData data;
    generateBox(&data, 5, 2, true);

    NeighbourGrid grid;
    grid.generate(data, 1.0);
    grid.colourParticles(data);
    grid.generateIterationList(data);

    int ownedCount = 0;
    int neighbourCount = 0;
    std::vector<Real3> neighbourCordsExpected;


    data.iterateOwnedParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        int colour = grid.getPraticleColor(pInd);
        grid.iteratePossibleNeighbours([&](size_t nInd) {
            // neighbour particles should be within +-1 of the current one
            neighbourCordsExpected.push_back({data.xCord[nInd], data.yCord[nInd], data.zCord[nInd]});
        }, pInd, colour, true);
    }, data);

    REQUIRE(grid.iterationOrder.size() == data.xCord.size());

    data.reorder(grid.iterationOrder);
    // reorder full grid, enables generic reordering at this stage
    grid.reoder(grid.iterationOrder);
    grid.generateIterationList(data);

    std::vector<Real3> neighbourCords;

    data.iterateOwnedParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        ownedCount++;
        REQUIRE(data.owners[pInd] == 0);
        REQUIRE(data.xCord[pInd] == 2);
        REQUIRE(data.yCord[pInd] == 2);
        REQUIRE(data.zCord[pInd] == 2);

        int colour = grid.getPraticleColor(pInd);

        grid.iteratePossibleNeighbours([&](size_t nInd) {
            neighbourCount++;
            neighbourCords.push_back({data.xCord[nInd], data.yCord[nInd], data.zCord[nInd]});
        }, pInd, colour, true);
    }, data);
    for(Real3 p: neighbourCordsExpected)
    {
        auto pos = std::find_if(neighbourCords.begin(), neighbourCords.end(), [&](Real3 cord){
            return cord.x == p.x && cord.y == p.y && cord.z == p.z;});
        REQUIRE(pos != neighbourCords.end());
    }
    // only one should be owned
    REQUIRE(ownedCount == 1);
    // there should be 27 iteratedNeighbours, counting it self
    REQUIRE(neighbourCount == 26);

}

TEST_CASE( "Iterate linked particle", "[Particle iterators]" ) {
    HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles;

    HiDEMTransportParticle p1;
    p1.globalKey = 0;
    p1.owner = 0;
    p1.cord = {0};
    particles.push_back(p1);

    HiDEMTransportParticle p2;
    p2.globalKey = 1;
    p2.owner = 1;
    p2.cord = {1};
    particles.push_back(p2);

    HiDEMTransportParticle p3;
    p3.globalKey = 2;
    p3.owner = 1;
    p3.cord = {1,0,0};
    particles.push_back(p3);

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    l1.state = 1;
    links.push_back(l1);

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);
    REQUIRE(lData.numLocal == 1);

    int iteratedLinked = 0;
    pData.iterateOwnedParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        REQUIRE(pInd == 0);
        lData.iterateParticleLinks([&](ind_t lInd, ind_t pIndOther, HiDEMLinkData &lData) {
            REQUIRE(pIndOther == 1);
            iteratedLinked++;
        }, pInd, lData);
    }, pData);
    REQUIRE(iteratedLinked == 1);
}


TEST_CASE( "Iterate links", "[Particle iterators]" ) {
    HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles;

    HiDEMTransportParticle p1;
    p1.globalKey = 0;
    p1.owner = 0;
    p1.cord = {0};
    particles.push_back(p1);

    HiDEMTransportParticle p2;
    p2.globalKey = 1;
    p2.owner = 1;
    p2.cord = {1};
    particles.push_back(p2);

    HiDEMTransportParticle p3;
    p3.globalKey = 2;
    p3.owner = 1;
    p3.cord = {1,0,0};
    particles.push_back(p3);

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    links.push_back(l1);

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    REQUIRE(lData.numLocal == 1);

    int iteratedLinked = 0;

    lData.iterateAllLinks(SerialExec(), [&] (ind_t lInd, ind_t indA, ind_t indB, HiDEMLinkData &lData){
        iteratedLinked ++;
        REQUIRE(lInd == 0);

        REQUIRE(indA == pMapper.getLocalInd(1));
        REQUIRE(indB == pMapper.getLocalInd(0));

    }, lData);
    REQUIRE(iteratedLinked == 1);
}

