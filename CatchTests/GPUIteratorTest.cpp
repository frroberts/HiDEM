//
// Created by Fredrik Robertsen on 08/11/2020.
//

#include <catch2/catch.hpp>
#include "Generators/FCCParticleGenerator.h"
#include "../HiDEMLib/HiDEMParticleData.h"
#include "../PLiB/Geometry/Map2d.h"
#include "Generators/LinkGenerator.h"
#include "../Iterators/DevCall.h"

#include "TestHelpers.h"

TEST_CASE( "GPU not enabled", "[GPU iterator]" ) {
#ifndef GPU
    WARN("NOT built with GPU support");
#endif
}
#ifdef GPU

/*
TEST_CASE( "Owned particles iteration test", "[GPU iterator]" ) {
    //

    std::vector<HiDEMTransportParticle> particles;
    HiDEMTransportParticle p1;
    p1.cord = {1,1,1};
    p1.owner = 0;
    p1.globalKey = 0;
    particles.push_back(p1);


    HiDEMParticleData &pData = *new HiDEMParticleData();
    pData.appendTransportParticles(particles);

    GPUIterator::iterateAllParticles([=] DEVCALLABLE(size_t pInd, HiDEMParticleData &pData) {
        // only write to pData.XXX[pInd]
        pData.xCord[pInd]++;
        pData.yCord[pInd]++;
        pData.zCord[pInd]++;
    }, pData);

    GPUIterator::sync();

    REQUIRE(pData.xCord[0] == 2);
    REQUIRE(pData.yCord[0] == 2);
    REQUIRE(pData.zCord[0] == 2);
}


TEST_CASE( "Neighbour particle iteration test", "[GPU iterator]" ) {

    HiDEMParticleData &pData = *new HiDEMParticleData();
    generateBox(&pData, 3, 1, true);

    NeighbourGrid& grid = *new NeighbourGrid(1*2);
    grid.generateNeighbourGrid(pData);
    grid.colourParticles(pData);
    grid.generateIterationList(pData);

    GPUIterator::iterateOwnedParticles([=] DEVCALLABLE (size_t pInd, HiDEMParticleData &pData) mutable {
        // only write to pData.XXX[pInd]
        pData.xCordPrev[pInd] = 0;
        pData.yCordPrev[pInd] = 0;
        pData.zCordPrev[pInd] = 0;
        int colour = grid.colours[pInd];
        GPUIterator::iteratePossibleNeighbours([&](size_t nInd, NeighbourGrid &grid) {
            pData.xCordPrev[pInd] += pData.xCord[nInd];
            pData.yCordPrev[pInd] += pData.yCord[nInd];
            pData.zCordPrev[pInd] += pData.zCord[nInd];
        }, colour, true, grid);
    }, pData);

    GPUIterator::sync();

    SerialIterator::iterateOwnedParticles([=](size_t pInd, HiDEMParticleData &pData) {
        REQUIRE(pData.xCordPrev[pInd] == 27);
        REQUIRE(pData.yCordPrev[pInd] == 27);
        REQUIRE(pData.zCordPrev[pInd] == 27);
    }, pData);
}

TEST_CASE( "Linked particle iteration test", "[GPU iterator]" ) {

    HiDEMParticleData &pData = *new HiDEMParticleData();
    HiDEMLinkData lData;
    std::vector<HiDEMTransportParticle> particles;

    HiDEMTransportParticle p1;
    p1.globalKey = 0;
    p1.owner = 0;
    p1.cord = {0};
    particles.push_back(p1);

    HiDEMTransportParticle p2;
    p2.globalKey = 1;
    p2.owner = 1;
    p2.cord = {0.1};
    particles.push_back(p2);

    HiDEMTransportParticle p3;
    p3.globalKey = 2;
    p3.owner = 1;
    p3.cord = {1,0,0};
    particles.push_back(p3);

    pData.appendTransportParticles(particles);
    lData.setNumParticles(pData.numLocal);

    GlobalLocalMapper pMapper;
    pData.remap(pMapper);

    std::vector<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    links.push_back(l1);

    lData.addLinks(links, pMapper);

    REQUIRE(pData.xCord[pMapper.getLocalInd(0)] == 0);
    REQUIRE(pData.xCord[pMapper.getLocalInd(1)] == 0.1);

    int iteratedLinked = 0;
    GPUIterator::iterateOwnedParticles([=] DEVCALLABLE(size_t pInd, HiDEMParticleData &pData) mutable {
        GPUIterator::iterateParticleLinks([&](ind_t lInd, ind_t pIndOther, HiDEMLinkData &lData) {
            iteratedLinked++;
            pData.xCord[pInd] = 1;
            pData.xCord[pIndOther] = 1;
        }, pInd, lData);
    }, pData);

    GPUIterator::sync();

    REQUIRE(pData.xCord[pMapper.getLocalInd(0)] == 1);
    REQUIRE(pData.xCord[pMapper.getLocalInd(1)] == 1);
}

TEST_CASE( "Link iteration test", "[GPU iterator]" ) {

    HiDEMParticleData &pData = *new HiDEMParticleData();
    HiDEMLinkData &lData = *new HiDEMLinkData();
    std::vector<HiDEMTransportParticle> particles;

    HiDEMTransportParticle p1;
    p1.globalKey = 0;
    p1.owner = 0;
    p1.cord = {0};
    particles.push_back(p1);

    HiDEMTransportParticle p2;
    p2.globalKey = 1;
    p2.owner = 1;
    p2.cord = {0.1};
    particles.push_back(p2);

    HiDEMTransportParticle p3;
    p3.globalKey = 2;
    p3.owner = 1;
    p3.cord = {1,0,0};
    particles.push_back(p3);

    pData.appendTransportParticles(particles);
    lData.setNumParticles(pData.numLocal);

    GlobalLocalMapper pMapper;
    pData.remap(pMapper);

    std::vector<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    links.push_back(l1);

    lData.appendTransportLinks(links, pMapper);

    GPUIterator::iterateOwnedLinks([=] DEVCALLABLE (ind_t lInd, ind_t indA, ind_t indB, HiDEMLinkData &lData){

        lData.state[lInd] = false;

    }, lData);
    GPUIterator::syncAll();

    REQUIRE(lData.state[0] == false);
}

TEST_CASE( "Owned particles iteration test", "[GPU iterator]" ) {
    //

    std::vector<HiDEMTransportParticle> particles;
    HiDEMTransportParticle p1;
    p1.cord = {1,1,1};
    p1.owner = 0;
    p1.globalKey = 0;
    particles.push_back(p1);


    HiDEMParticleData &pData = *new HiDEMParticleData();
    pData.appendTransportParticles(particles);

    GPUIterator::iterateAllParticles([=] DEVCALLABLE(size_t pInd, HiDEMParticleData &pData) {
        // only write to pData.XXX[pInd]
        pData.xCord[pInd]++;
        pData.yCord[pInd]++;
        pData.zCord[pInd]++;
    }, pData);

    GPUIterator::syncRecvs();

    REQUIRE(pData.xCord[0] == 2);
    REQUIRE(pData.yCord[0] == 2);
    REQUIRE(pData.zCord[0] == 2);
}

*/
#endif
