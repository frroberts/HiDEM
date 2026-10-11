    //
// Created by Fredrik Robertsen on 02/11/2020.
//

#include <catch2/catch.hpp>

#include <algorithm>
#include <random>

#include "Generators/FCCParticleGenerator.h"
#include <Geometry/Map2d.h>
#include "Generators/LinkGenerator.h"

#include "TestHelpers.h"

#include "../HiDEMLib/HiDEMParticleData.h"
#include "../HiDEMLib/HiDEMLinkData.h"

/*
TEST_CASE( "Test stripping of duplicate links", "[Link Generator]" ) {

    HiDEMTransportLink l1;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 4;
    HiDEMTransportLink l2;
    l2.globalKeyParticleA = 4;
    l2.globalKeyParticleB = 1;
    HiDEMTransportLink l3;
    l3.globalKeyParticleA = 2;
    l3.globalKeyParticleB = 1;

    std::vector<HiDEMTransportLink> links;
    links.push_back(l2);
    links.push_back(l3);
    links.push_back(l1);
    links.push_back(l3);
    links.push_back(l2);
    links.push_back(l1);
    links.push_back(l3);

    LinkGenerator gen;

    gen.stripDuplicates(links);

    REQUIRE(links.size() == 2);
}

/*
TEST_CASE( "Test stripping of duplicate links 2", "[Link Generator]" ) {

    std::vector<HiDEMTransportLink> links;
    links.push_back({34,292});
    links.push_back({7,292});
    links.push_back({38,292});
    links.push_back({39,292});
    links.push_back({39,292});
    links.push_back({34,292});
    links.push_back({38,292});
    links.push_back({7,292});
    links.push_back({257,292});
    links.push_back({261,292});
    links.push_back({261,292});
    links.push_back({263,292});
    links.push_back({263,292});
    links.push_back({257,292});
    links.push_back({292,295});
    links.push_back({289,292});
    links.push_back({290,292});
    links.push_back({292,293});
    links.push_back({292,294});
    links.push_back({292,293});
    links.push_back({292,294});
    links.push_back({292,295});
    LinkGenerator gen;

    auto rng = std::default_random_engine {1};
    std::shuffle(std::begin(links), std::end(links), rng);

    gen.stripDuplicates(links);

    REQUIRE(links.size() == 12);
}
 */

TEST_CASE("Test linking particles within distance", "[Link Generator]") {

    HiDEMParticleData data;
    FCCParticleGenerator<HiDEMTransportParticle> pGen({0}, {2}, 1, 0.25);
    auto bed = generateGeometryPlane({0}, {2}, 0);
    auto surf = generateGeometryPlane({0}, {2}, 2);

    Domain d;
    d.lower = {0, 0, 0};
    d.upper = {2, 2, 2};
    auto geometry = std::make_shared<InputDEM>();
    geometry->base = bed.get();
    geometry->surface = surf.get();

    std::vector<Map<double>*> prob;
    prob.push_back(new MapSingle<double>(1));
    ManVec<HiDEMTransportParticle> particles;
    pGen.generate(d, {geometry}, prob, 0, particles);
    data.appendTransportParticles(particles);

    LinkGenerator<HiDEMTransportLink, HiDEMParticleData>  lGen;

    auto links = lGen.generateLinksDistance(1.57, 0.25, data);

    // find cord 1 1 1
    int foundLinks = 0;
    bool foundCenter = false;
    data.iterateAllParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        double xCord = data.xCord[pInd];
        double yCord = data.yCord[pInd];
        double zCord = data.zCord[pInd];
        if (xCord == 1.0 && yCord == 1.0 && zCord == 1.0) {
            ind_t pIndGlobal = data.globalKeys[pInd];
            foundCenter = true;
            // dumb search for links
            for (auto l: links) {
                if (l.globalKeyParticleA == pIndGlobal || l.globalKeyParticleB == pIndGlobal) {
                    foundLinks++;
                }
            }
            return;
        }
    }, data);

    REQUIRE(foundLinks == 12);
    REQUIRE(foundCenter);
}

// not really useful anymore
TEST_CASE("Test linking particles within distance fcc small distance", "[Link Generator]") {

    double spacing = 0.25;


    HiDEMParticleData data;
    FCCParticleGenerator<HiDEMTransportParticle> pGen({0}, {2}, spacing, spacing / 2.0);
    auto bed = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 1, 1);
    bed->setPointInd(0, 0, 0);
    bed->setPointInd(1, 0, 0);
    bed->setPointInd(0, 1, 0);
    bed->setPointInd(1, 1, 0);

    auto surf = std::make_shared<Map2d<lpReal_t>>(2, 2, 0, 0, 1, 1);
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
std::vector<Map<double>*> prob;
    prob.push_back(new MapSingle<double>(1));
    ManVec<HiDEMTransportParticle> particles;
    pGen.generate(d, {geometry}, prob, 0, particles);
    data.appendTransportParticles(particles);

    LinkGenerator<HiDEMTransportLink, HiDEMParticleData>  lGen;

    auto links = lGen.generateLinksDistance(0.85, spacing / 2.0, data);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    data.remap(pMapper);


    HiDEMLinkData lData;

        lData.appendInitLinks(links);

    // find cord 0.5 0.5 0.5
    int foundLinks = 0;
    bool foundCenter = false;

    data.iterateAllParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        double xCord = data.xCord[pInd];
        double yCord = data.yCord[pInd];
        double zCord = data.zCord[pInd];
        if (xCord == 0.5 && yCord == 0.5 && zCord == 0.5) {
            ind_t pIndGlobal = data.globalKeys[pInd];
            foundCenter = true;
            // dumb search for links
            for (auto l: links) {
                if (l.globalKeyParticleA == pIndGlobal || l.globalKeyParticleB == pIndGlobal) {
                    foundLinks++;
                }
            }
        }
    }, data);
    REQUIRE(foundCenter);
    REQUIRE(foundLinks == 12);

}


TEST_CASE("Test iterate linked particles FCC", "[Link Generator]") {

    HiDEMParticleData pData;
    double fccSpacing = 0.5;
    FCCParticleGenerator<HiDEMTransportParticle> pGen({0}, {4}, fccSpacing, fccSpacing/4);
    auto bed = generateGeometryPlane({0}, {4}, 0);
    auto surf = generateGeometryPlane({0}, {4}, 4);

    Domain d;
    d.lower = {0, 0, 0};
    d.upper = {4, 4, 4};
    auto geometry = std::make_shared<InputDEM>();
    geometry->base = bed.get();
    geometry->surface = surf.get();

std::vector<Map<double>*> prob;
    prob.push_back(new MapSingle<double>(1));
    ManVec<HiDEMTransportParticle> particles;
    pGen.generate(d, {geometry}, prob, 0, particles);
    pData.appendTransportParticles(particles);

    LinkGenerator<HiDEMTransportLink, HiDEMParticleData>  lGen;

    auto links = lGen.generateLinksDistance(1.6, fccSpacing / 4.0, pData);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    HiDEMLinkData lData;
        lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    // find cord 2 2 2
    int foundLinks = 0;
    bool foundMiddle = false;
    pData.iterateAllParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        double xCord = data.xCord[pInd];
        double yCord = data.yCord[pInd];
        double zCord = data.zCord[pInd];
        if (xCord == 2.0 && yCord == 2.0 && zCord == 2.0) {
            foundMiddle = true;
            lData.iterateParticleLinks([&](ind_t lInd, ind_t pIndOther, HiDEMLinkData &lData) {
                REQUIRE(fabs(pData.xCord[pIndOther] - xCord) <= (fccSpacing / 2.0));
                REQUIRE(fabs(pData.yCord[pIndOther] - yCord) <= (fccSpacing / 2.0));
                REQUIRE(fabs(pData.yCord[pIndOther] - zCord) <= (fccSpacing / 2.0));
                foundLinks++;
                //std::cout << pData.xCord[pIndOther] - xCord << ", \t" << std::endl;
                //std::cout << pData.yCord[pIndOther] - yCord << ", \t" << std::endl;
                //std::cout << pData.zCord[pIndOther] - zCord << ", \t" << std::endl;
                //std::cout << std::endl;
            }, pInd, lData);
        }
    }, pData);
    REQUIRE(foundMiddle);
    REQUIRE(foundLinks == 12);
}