//
// Created by robertse on 03/10/23.
//

#include <catch2/catch.hpp>

#include <thread>

#include <Generators/BlockParticleGenerator.h>

#include "../HiDEMLib/HiDEMParticleData.h"

TEST_CASE( "Test reading unit cell from csv", "[Unit cell generator]" ) { // deprecate this eventually

    BlocParticleGenerator<HiDEMTransportParticle> gen({0},{200}, 1, {20});

    gen.loadUnitCell("../Samples/testp.csv");

    REQUIRE(gen.unitCell.size() == 1300);
}

TEST_CASE( "Test generating box unit cell", "[Unit cell generator]" ) { // deprecate this eventually

    BlocParticleGenerator<HiDEMTransportParticle> gen({0},{200}, 1, {20});

    gen.scale = 2;

    std::vector<Map<double> *> prob;
    prob.push_back(new MapSingle<double>(1));

    Domain dom;
    dom.lower = {0, 0, 0};
    dom.upper = {250, 250, 250};
    auto bed = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 201, 201);
    bed->setPointInd(0, 0, 0);
    bed->setPointInd(200, 0, 0);
    bed->setPointInd(0, 200, 0);
    bed->setPointInd(200, 200, 0);

    auto surf = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 201, 201);
    surf->setPointInd(0, 0, 100);
    surf->setPointInd(200, 0, 100);
    surf->setPointInd(0, 200, 200);
    surf->setPointInd(200, 200, 200);

    auto geometry = std::make_shared<InputDEM>();
    geometry->base = bed.get();
    geometry->surface = surf.get();

    ManVec<HiDEMTransportParticle> particles;
    gen.generate(dom, {geometry}, prob, {0}, particles);

    for(auto p : particles){
        //std::cout << p.cord.x << " " << p.cord.y << " " << p.cord.z << " " << p.globalKey << std::endl;
    }


    //std::cout << particles.size() << std::endl;
}