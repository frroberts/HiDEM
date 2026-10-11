//
// Created by Fredrik Robertsen on 24/10/2020.
//

#include <catch2/catch.hpp>

#include "Generators/FCCParticleGenerator.h"
#include <Geometry/Map2d.h>
#include <NeighbourGrid.h>

#include "../HiDEMLib/HiDEMParticleData.h"

TEST_CASE("FCC generator box test", "[Generator]") {

    FCCParticleGenerator<HiDEMTransportParticle> gen({0}, {2}, 1.5874, 1);
    HiDEMParticleData data;
    auto bed = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 2, 2);
    bed->setPointInd(0, 0, 0);
    bed->setPointInd(1, 0, 0);
    bed->setPointInd(0, 1, 0);
    bed->setPointInd(1, 1, 0);

    auto surf = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 2, 2);
    surf->setPointInd(0, 0, 1);
    surf->setPointInd(1, 0, 1);
    surf->setPointInd(0, 1, 1);
    surf->setPointInd(1, 1, 1);

    Domain d;
    d.lower = {0, 0, 0};
    d.upper = {2, 2, 2};
    auto geometry = std::make_shared<InputDEM>();
    geometry->base = bed.get();
    geometry->surface = surf.get();

    std::vector<Map<double> *> prob;
    prob.push_back(new MapSingle<double>(1));
    ManVec<HiDEMTransportParticle> particles;
    gen.generate(d, {geometry}, prob, 0, particles);
    data.appendTransportParticles(particles);

    /*
    NeighbourGrid grid(0.5);
    grid.generateNeighbourGrid(&data);
    grid.colourParticles(&data);
    grid.generateIterationList(&data);
*/

    // test that this builds it correctly
}


// todo make a better check of the generated geometry
TEST_CASE("FCC generator test 1", "[Generator]") {

    FCCParticleGenerator<HiDEMTransportParticle> gen({0}, {2}, 1, 1);
    HiDEMParticleData data;

    auto bed = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 2, 2);
    bed->setPointInd(0, 0, 0);
    bed->setPointInd(1, 0, 0);
    bed->setPointInd(0, 1, 1);
    bed->setPointInd(1, 1, 1);

    auto surf = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 2, 2);
    surf->setPointInd(0, 0, 2);
    surf->setPointInd(1, 0, 2);
    surf->setPointInd(0, 1, 4);
    surf->setPointInd(1, 1, 4);

    Domain d;
    d.lower = {0, 0, 0};
    d.upper = {2, 2, 2};
    auto geometry = std::make_shared<InputDEM>();
    geometry->base = bed.get();
    geometry->surface = surf.get();

    std::vector<Map<double> *> prob;
    prob.push_back(new MapSingle<double>(1));
    ManVec<HiDEMTransportParticle> particles;
    gen.generate(d, {geometry}, prob, 0, particles);
    data.appendTransportParticles(particles);

    double zMin = std::numeric_limits<double>::max();
    double zMax = std::numeric_limits<double>::min();
    data.iterateAllParticles(SerialExec(), [&](size_t index, HiDEMParticleData &data) {
        zMin = std::min(data.zCord[index], zMin);
        zMax = std::max(data.zCord[index], zMax);
        //std::cout << data.xCord[index] << "," << data.yCord[index] << "," << data.zCord[index] << std::endl;
    }, data);

    REQUIRE(zMin >= 0);
    REQUIRE(zMax <= 4);
}

// todo make a better check of the generated geometry
TEST_CASE("FCC generator test surface at 0", "[Generator]") {

    FCCParticleGenerator<HiDEMTransportParticle> gen({0}, {2}, 1.5874, 1);
    HiDEMParticleData data;

    auto bed = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 2, 2);
    bed->setPointInd(0, 0, 0);
    bed->setPointInd(1, 0, 0);
    bed->setPointInd(0, 1, 1);
    bed->setPointInd(1, 1, 1);

    auto surf = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 2, 2);
    surf->setPointInd(0, 0, 0);
    surf->setPointInd(1, 0, 0);
    surf->setPointInd(0, 1, 0);
    surf->setPointInd(1, 1, 0);

    Domain d;
    d.lower = {0, 0, 0};
    d.upper = {2, 2, 2};
    auto geometry = std::make_shared<InputDEM>();
    geometry->base = bed.get();
    geometry->surface = surf.get();

    std::vector<Map<double> *> prob;
    prob.push_back(new MapSingle<double>(1));
    ManVec<HiDEMTransportParticle> particles;
    gen.generate(d, {geometry}, prob, 0, particles);
    data.appendTransportParticles(particles);

    double zMin = 0;
    double zMax = 0;
    int particleCount = 0;
    data.iterateAllParticles(SerialExec(), [&](size_t index, HiDEMParticleData &data) {
        zMin = std::min(data.zCord[index], zMin);
        zMax = std::max(data.zCord[index], zMax);
        particleCount++;
    }, data);

    REQUIRE(particleCount == 0);
    REQUIRE(zMin == 0);
    REQUIRE(zMax == 0);
}


TEST_CASE("FCC generator neighbour iteratior test ", "[Generator]") {
    FCCParticleGenerator<HiDEMTransportParticle> gen({0}, {2}, 0.5, 1);
    HiDEMParticleData data;

    auto bed = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 2, 2);
    bed->setPointInd(0, 0, 0);
    bed->setPointInd(1, 0, 0);
    bed->setPointInd(0, 1, 1);
    bed->setPointInd(1, 1, 1);

    auto surf = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 2, 2);
    surf->setPointInd(0, 0, 2);
    surf->setPointInd(1, 0, 2);
    surf->setPointInd(0, 1, 4);
    surf->setPointInd(1, 1, 4);

    Domain d;
    d.lower = {0, 0, 0};
    d.upper = {2, 2, 2};
    auto geometry = std::make_shared<InputDEM>();
    geometry->base = bed.get();
    geometry->surface = surf.get();

    std::vector<Map<double>*> prob;
    prob.push_back(new MapSingle<double>(1));
    ManVec<HiDEMTransportParticle> particles;
    gen.generate(d, {geometry}, prob, 0, particles);
    data.appendTransportParticles(particles);

    NeighbourGrid grid;
    grid.generate(data, 0.25);
    grid.colourParticles(data);
    grid.generateIterationList(data);

    int iteratedCount = 0;
    int neigbourCount = 0;

    data.iterateAllParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        if (data.xCord[pInd] == 1 && data.yCord[pInd] == 1 && data.zCord[pInd] == 1) {
            iteratedCount++;
            //auto [xId, yId, zId] = grid.getGridId(data.xCord[pInd],data.xCord[pInd],data.xCord[pInd]);
            int colour = grid.getPraticleColor(pInd);//getGridColor(xId, yId, zId);

            grid.iteratePossibleNeighbours([&](size_t nInd) {
                neigbourCount++;
                REQUIRE(fabs(data.xCord[nInd] - 1) <= 0.25);
                REQUIRE(fabs(data.yCord[nInd] - 1) <= 0.25);
                REQUIRE(fabs(data.zCord[nInd] - 1) <= 0.25);
            }, pInd, colour, true);
        }
    }, data);
    REQUIRE(iteratedCount == 1);
    // is this correct ?
    REQUIRE(neigbourCount == 12);
}

TEST_CASE("FCC generator neighbour iteratior test 2", "[Generator]") {
    FCCParticleGenerator<HiDEMTransportParticle> gen({0}, {2}, 0.5, 1);
    HiDEMParticleData data;

    auto bed = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 2, 2);
    bed->setPointInd(0, 0, 0);
    bed->setPointInd(1, 0, 0);
    bed->setPointInd(0, 1, 1);
    bed->setPointInd(1, 1, 1);

    auto surf = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 2, 2);
    surf->setPointInd(0, 0, 2);
    surf->setPointInd(1, 0, 2);
    surf->setPointInd(0, 1, 4);
    surf->setPointInd(1, 1, 4);

    Domain d;
    d.lower = {0, 0, 0};
    d.upper = {2, 2, 2};
    auto geometry = std::make_shared<InputDEM>();
    geometry->base = bed.get();
    geometry->surface = surf.get();

    std::vector<Map<double> *> prob;
    prob.push_back(new MapSingle<double>(1));
    ManVec<HiDEMTransportParticle> particles;
    gen.generate(d, {geometry}, prob, 0, particles);
    data.appendTransportParticles(particles);

    NeighbourGrid grid;
    grid.generate(data, 0.25);
    grid.colourParticles(data);
    grid.generateIterationList(data);

    int iteratedCount = 0;
    int neigbourCount = 0;

    data.iterateAllParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        if (data.xCord[pInd] == 1 && data.yCord[pInd] == 1 && data.zCord[pInd] == 1) {
            iteratedCount++;
            //auto [xId, yId, zId] = grid.getGridId(data.xCord[pInd],data.xCord[pInd],data.xCord[pInd]);
            int colour = grid.getPraticleColor(pInd);//getGridColor(xId, yId, zId);

            grid.iteratePossibleNeighbours([&](size_t nInd) {
                neigbourCount++;
                // todo renable this
                /*
                REQUIRE(fabs(data.xCord[nInd]-1) <= 0.25);
                REQUIRE(fabs(data.yCord[nInd]-1) <= 0.25);
                REQUIRE(fabs(data.zCord[nInd]-1) <= 0.25);
                 */
            }, pInd, colour, true);
        }
    }, data);
    REQUIRE(iteratedCount == 1);
    // is this correct ?
    //REQUIRE(neigbourCount == 13);
}



