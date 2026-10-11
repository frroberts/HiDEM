//
// Created by Fredrik Robertsen on 31/10/2020.
//

#include <catch2/catch.hpp>

#include <Geometry/Map2d.h>
#include "Generators/FCCParticleGenerator.h"
#include "Generators/LinkGenerator.h"

#include "../HiDEMLib/HiDEMParticleData.h"
#include "../HiDEMLib/HiDEMLinkData.h"

#include "TestHelpers.h"

TEST_CASE( "Link data add initial global links", "[HiDEMLinkData]" )
{

    HiDEMParticleData pData;
    FCCParticleGenerator<HiDEMTransportParticle> pGen({0}, {2}, 1.5874, 1);
    auto bed = generateGeometryPlane({0},{2}, 0);
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
    pData.appendTransportParticles(particles);


    LinkGenerator<HiDEMTransportLink, HiDEMParticleData> lGen;

    auto links = lGen.generateLinksDistance(1, 0.5 / 2.0, pData); // todo is this the right factor here ?
    REQUIRE(links.size() != 0);

    HiDEMLinkData lData;

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;

    pData.remap(pMapper);

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);


    REQUIRE(lData.numLocal == links.size());
}

TEST_CASE( "Add specific link", "[HiDEMLinkData]" )
{

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
    p2.owner = 0;
    p2.cord = {1};
    particles.push_back(p2);

    HiDEMTransportParticle p3;
    p3.globalKey = 2;
    p3.owner = 0;
    p3.cord = {1,0,0};
    particles.push_back(p3);

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.state = 1;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    links.push_back(l1);

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    REQUIRE(lData.numLocal == 1);
    REQUIRE(lData.linksPerParticle[pMapper.getLocalInd(0)] == 1);
    REQUIRE(lData.linksPerParticle[pMapper.getLocalInd(1)] == 1);
    REQUIRE(lData.linksPerParticle[pMapper.getLocalInd(2)] == 0);

    REQUIRE(lData.localPartIndA[lData.getPartLinkId(0,pMapper.getLocalInd(0))] == pMapper.getLocalInd(1));
    REQUIRE(lData.localPartIndB[lData.getPartLinkId(0,pMapper.getLocalInd(0))] == pMapper.getLocalInd(0));

    REQUIRE(lData.localPartIndA[lData.getPartLinkId(0,pMapper.getLocalInd(1))] == pMapper.getLocalInd(1));
    REQUIRE(lData.localPartIndB[lData.getPartLinkId(0,pMapper.getLocalInd(1))] == pMapper.getLocalInd(0));
}

TEST_CASE( "Test get own part of CT", "[HiDEMLinkData]" )
{
    //

    HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles = make3Particles();

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    l1.CT[0][0] = 11;
    l1.CT[1][0] = 22;
    l1.state = 1;
    links.push_back(l1);

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    REQUIRE(lData.getOwnPart(0,0) == 1);
    REQUIRE(lData.getOwnPart(1,0) == 0);

}

/*
TEST_CASE( "Test get own part of CT full FCC", "[HiDEMLinkData]" )
{
    HiDEMParticleData pData;
    FCCParticleGenerator pGen({0}, {4});
    double fccSpacing = 0.5;
    auto bed = generateGeometryPlane({0},{4}, 0);
    auto surf = generateGeometryPlane({0},{4}, 4);

    Domain d;
    d.lower = {0, 0, 0};
    d.upper = {2, 2, 2};
    auto particles = pGen.generateFCC(fccSpacing, bed, surf, d);
    pData.appendTransportParticles(particles);

    LinkGenerator lGen;

    auto links = lGen.generateLinksDistance(fccSpacing / 2.0, pData);

    GlobalLocalMapper pMapper;
    pData.remap(pMapper);
    
    HiDEMLinkData lData;
    lData.appendTransportLinks(links);
    lData.connectParticleLinks(pMapper);

    SerialIterator::iterateAllParticles([&](size_t pInd, HiDEMParticleData &data) {
        double xCord = data.xCord[pInd];
        double yCord = data.yCord[pInd];
        double zCord = data.zCord[pInd];
        lData.iterateParticleLinks([&](ind_t lInd, ind_t pIndOther, HiDEMLinkData &lData) {
            int ctInd = lData.getOwnPart(pInd, lInd);
            //
            lData.CT0[ctInd][lInd] += 1;
            lData.CT1[ctInd][lInd] += 1;
            lData.CT2[ctInd][lInd] += 1;
            lData.CT3[ctInd][lInd] += 1;
            lData.CT4[ctInd][lInd] += 1;
            lData.CT5[ctInd][lInd] += 1;
        }, pInd, lData);
    }, pData);

    SerialIterator::iterateAllLinks([&](ind_t lInd, ind_t indA, ind_t indB, HiDEMLinkData &lData){
        REQUIRE(lData.CT0[0][lInd] == 1);
        REQUIRE(lData.CT0[1][lInd] == 1);
        REQUIRE(lData.CT1[0][lInd] == 1);
        REQUIRE(lData.CT1[1][lInd] == 1);
        REQUIRE(lData.CT2[0][lInd] == 1);
        REQUIRE(lData.CT2[1][lInd] == 1);
        REQUIRE(lData.CT3[0][lInd] == 1);
        REQUIRE(lData.CT3[1][lInd] == 1);
        REQUIRE(lData.CT4[0][lInd] == 1);
        REQUIRE(lData.CT4[1][lInd] == 1);
        REQUIRE(lData.CT5[0][lInd] == 1);
        REQUIRE(lData.CT5[1][lInd] == 1);

    }, lData);
}
 */

TEST_CASE("Test reordering of particles", "[HiDEMLinkData]" )
{
    HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles = make3Particles();

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    l1.owner = 0;
    l1.state = 1;
    links.push_back(l1);

    HiDEMTransportLink l2;
    l2.globalKey = 1;
    l2.globalKeyParticleA = 0;
    l2.globalKeyParticleB = 2;
    l2.owner = 0;
    l2.state = 1;
    links.push_back(l2);


    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    REQUIRE(pMapper.getLocalInd(1) == 1);
    lData.iterateParticleLinks([](ind_t lInd, ind_t pIndOther, HiDEMLinkData &lData){
        REQUIRE(lData.globalKeys[lInd] == 0);
        }, pMapper.getLocalInd(1), lData);

    ManVec<pInd_t > pReorder;
    pReorder.push_back(1);
    pReorder.push_back(2);
    pReorder.push_back(0);
    REQUIRE(pData.numLocal == 3);
    pData.reorder(pReorder);

    pData.remap(pMapper);
    lData.connectParticleLinks(pMapper);

    REQUIRE(pMapper.getLocalInd(1) == 0);
    lData.iterateParticleLinks([](ind_t lInd, ind_t pIndOther, HiDEMLinkData &lData){
        REQUIRE(lData.globalKeys[lInd] == 0);
        }, pMapper.getLocalInd(1), lData);

}

TEST_CASE("Test dropping of links", "[HiDEMLinkData]" )
{
    //
    HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles = make3Particles();

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    l1.owner = 0;
    l1.state = 1;
    links.push_back(l1);

    HiDEMTransportLink l2;
    l2.globalKey = 1;
    l2.globalKeyParticleA = 0;
    l2.globalKeyParticleB = 2;
    l2.owner = 0;
    l2.state = 1;
    links.push_back(l2);

    HiDEMTransportLink l3;
    l3.globalKey = 2;
    l3.globalKeyParticleA = 1;
    l3.globalKeyParticleB = 2;
    l3.owner = 0;
    l3.state = 1;
    links.push_back(l3);

    lData.appendInitLinks(links);
    lData.generateOwnedLinksList();
    lData.connectParticleLinks(pMapper);

    REQUIRE(lData.numLocal == 3);
    int foundLinks = 0;
    // global key 1 particle should have 2 links
    for (int i = 0; i < lData.numLocal; ++i) {
    }
    lData.iterateParticleLinks([&](ind_t lInd, ind_t pIndOther, HiDEMLinkData &lData){
        REQUIRE((lData.globalKeys[lInd] == 2 || lData.globalKeys[lInd] == 0));
        REQUIRE((lInd == 2 || lInd == 0));
        foundLinks++;
        }, pMapper.getLocalInd(1), lData);
    REQUIRE(foundLinks == 2);
    // drop links
    ManVec<pInd_t > droppedInd;
    droppedInd.push_back(2);
    lData.dropLinks(droppedInd);

    // connections need to be remade
    lData.connectParticleLinks(pMapper);
    REQUIRE(lData.numLocal == 2);

    foundLinks = 0;
    // global key 1 particle should now have only 1 link
    lData.iterateParticleLinks([&](ind_t lInd, ind_t pIndOther, HiDEMLinkData &lData){
        REQUIRE(lData.globalKeys[lInd] != 2);
        REQUIRE(lData.globalKeys[lInd] == 0);
        REQUIRE(lInd != 2);
        REQUIRE(lInd == 0);
        foundLinks++;
        }, pMapper.getLocalInd(1), lData);
    REQUIRE(foundLinks == 1);

}

TEST_CASE("Test dropping of defunct links", "[HiDEMLinkData]" )
{
    HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles = make3Particles();

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    l1.owner = 0;
    l1.state = 1;
    links.push_back(l1);

    HiDEMTransportLink l2;
    l2.globalKey = 1;
    l2.globalKeyParticleA = 0;
    l2.globalKeyParticleB = 2;
    l2.owner = 0;
    l2.state = 1;
    links.push_back(l2);

    HiDEMTransportLink l3;
    l3.globalKey = 2;
    l3.globalKeyParticleA = 1;
    l3.globalKeyParticleB = 2;
    l3.owner = 0;
    l3.state = 1;
    links.push_back(l3);

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    ManVec<pInd_t > droppedPIndexes;
    droppedPIndexes.push_back(pMapper.getLocalInd(1));
    pData.dropParticles(droppedPIndexes);

    pData.remap(pMapper);

    lData.dropUnusedLinks(pMapper);
    REQUIRE(lData.numLocal == 1);
}

TEST_CASE("Test getting transport links", "[HiDEMLinkData]" )
{
    //
    HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles = make3Particles();

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    l1.owner = 0;
    l1.state = 1;
    l1.CT[0][0] = 11;
    l1.CT[1][0] = 22;
    links.push_back(l1);

    HiDEMTransportLink l2;
    l2.globalKey = 1;
    l2.globalKeyParticleA = 0;
    l2.globalKeyParticleB = 2;
    l2.owner = 0;
    l2.state = 1;
    l2.CT[0][0] = 33;
    l2.CT[1][0] = 44;
    links.push_back(l2);

    HiDEMTransportLink l3;
    l3.globalKey = 2;
    l3.globalKeyParticleA = 1;
    l3.globalKeyParticleB = 2;
    l3.owner = 0;
    l3.state = 1;
    l3.CT[0][0] = 55;
    l3.CT[1][0] = 66;
    links.push_back(l3);


    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    auto lRet = lData.toTransportLink(0);

    REQUIRE(lRet.globalKey == 0);
    REQUIRE(lRet.CT[0][0] == 11);
    REQUIRE(lRet.CT[1][0] == 22);
}

TEST_CASE("CT added correctly", "[HiDEMLinkData]" ) {
    //
    HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles = make3Particles();

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    l1.owner = 0;
    l1.state = 1;
    l1.CT[0][0] = 11;
    l1.CT[1][0] = 22;
    links.push_back(l1);

    HiDEMTransportLink l2;
    l2.globalKey = 1;
    l2.globalKeyParticleA = 0;
    l2.globalKeyParticleB = 2;
    l2.owner = 0;
    l2.state = 1;
    l2.CT[0][0] = 33;
    l2.CT[1][0] = 44;
    links.push_back(l2);

    HiDEMTransportLink l3;
    l3.globalKey = 2;
    l3.globalKeyParticleA = 1;
    l3.globalKeyParticleB = 2;
    l3.owner = 0;
    l3.state = 1;
    l3.CT[0][0] = 55;
    l3.CT[1][0] = 66;
    links.push_back(l3);

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    auto lRet = lData.toTransportLink(0);
}

TEST_CASE("CT reorder", "[HiDEMLinkData]" ) {
        HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles = make4Particles();

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    l1.owner = 0;
    l1.state = 1;
    l1.CT[0][0] = 11;
    l1.CT[1][0] = 22;
    links.push_back(l1);

    HiDEMTransportLink l2;
    l2.globalKey = 1;
    l2.globalKeyParticleA = 0;
    l2.globalKeyParticleB = 2;
    l2.owner = 0;
    l2.state = 1;
    l2.CT[0][0] = 33;
    l2.CT[1][0] = 44;
    links.push_back(l2);

    HiDEMTransportLink l3;
    l3.globalKey = 2;
    l3.globalKeyParticleA = 1;
    l3.globalKeyParticleB = 2;
    l3.owner = 0;
    l3.state = 1;
    l3.CT[0][0] = 55;
    l3.CT[1][0] = 66;
    links.push_back(l3);

    HiDEMTransportLink l4;
    l4.globalKey = 3;
    l4.globalKeyParticleA = 3;
    l4.globalKeyParticleB = 2;
    l4.owner = 0;
    l4.state = 1;
    l4.CT[0][0] = 77;
    l4.CT[1][0] = 88;
    links.push_back(l4);

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    REQUIRE(lData.sideIndEndA[0] == 0);
    REQUIRE(lData.sideIndEndA[1] == 2);
    REQUIRE(lData.sideIndEndA[2] == 4);
    REQUIRE(lData.sideIndEndA[3] == 6);

    REQUIRE(lData.sideIndEndB[0] == 1);
    REQUIRE(lData.sideIndEndB[1] == 3);
    REQUIRE(lData.sideIndEndB[2] == 5);
    REQUIRE(lData.sideIndEndB[3] == 7);

    ManVec<pInd_t> newOrder;
    newOrder.push_back(1);
    newOrder.push_back(2);
    newOrder.push_back(3);
    newOrder.push_back(4);
    newOrder.push_back(5);
    newOrder.push_back(6);
    newOrder.push_back(7);
    newOrder.push_back(0);

    lData.reorderSideData(newOrder);

    REQUIRE(lData.sideIndEndA[0] == 7);
    REQUIRE(lData.sideIndEndA[1] == 1);
    REQUIRE(lData.sideIndEndA[2] == 3);
    REQUIRE(lData.sideIndEndA[3] == 5);

    REQUIRE(lData.sideIndEndB[0] == 0);
    REQUIRE(lData.sideIndEndB[1] == 2);
    REQUIRE(lData.sideIndEndB[2] == 4);
    REQUIRE(lData.sideIndEndB[3] == 6);

    auto lRet = lData.toTransportLink(0);
    REQUIRE(lRet.globalKey == 0);
    REQUIRE(lRet.CT[0][0] == 11);
    REQUIRE(lRet.CT[1][0] == 22);

    lRet = lData.toTransportLink(1);
    REQUIRE(lRet.globalKey == 1);
    REQUIRE(lRet.CT[0][0] == 33);
    REQUIRE(lRet.CT[1][0] == 44);
}

TEST_CASE("Link reorder / remove handles CT correctly", "[HiDEMLinkData]" ) {
    //

    HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles = make4Particles();

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    l1.owner = 0;
    l1.state = 1;
    l1.CT[0][0] = 11;
    l1.CT[1][0] = 22;
    links.push_back(l1);

    HiDEMTransportLink l2;
    l2.globalKey = 1;
    l2.globalKeyParticleA = 0;
    l2.globalKeyParticleB = 2;
    l2.owner = 0;
    l2.state = 1;
    l2.CT[0][0] = 33;
    l2.CT[1][0] = 44;
    links.push_back(l2);

    HiDEMTransportLink l3;
    l3.globalKey = 2;
    l3.globalKeyParticleA = 1;
    l3.globalKeyParticleB = 2;
    l3.owner = 0;
    l3.state = 1;
    l3.CT[0][0] = 55;
    l3.CT[1][0] = 66;
    links.push_back(l3);

    HiDEMTransportLink l4;
    l4.globalKey = 3;
    l4.globalKeyParticleA = 3;
    l4.globalKeyParticleB = 2;
    l4.owner = 0;
    l4.state = 1;
    l4.CT[0][0] = 77;
    l4.CT[1][0] = 88;
    links.push_back(l4);

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    REQUIRE(lData.CT0_flat.size() == 8);

    auto lRet1 = lData.toTransportLink(0);
    REQUIRE(lRet1.globalKey == 0);
    REQUIRE(lRet1.CT[0][0] == 11);
    REQUIRE(lRet1.CT[1][0] == 22);

    auto lRet2 = lData.toTransportLink(1);
    REQUIRE(lRet2.globalKey == 1);
    REQUIRE(lRet2.CT[0][0] == 33);
    REQUIRE(lRet2.CT[1][0] == 44);

    auto lRet3 = lData.toTransportLink(2);
    REQUIRE(lRet3.globalKey == 2);
    REQUIRE(lRet3.CT[0][0] == 55);
    REQUIRE(lRet3.CT[1][0] == 66);

    auto lRet4 = lData.toTransportLink(3);
    REQUIRE(lRet4.globalKey == 3);
    REQUIRE(lRet4.CT[0][0] == 77);
    REQUIRE(lRet4.CT[1][0] == 88);

    ManVec<pInd_t> toBeDropepd;
    //toBeDropepd.push_back(2);
    toBeDropepd.push_back(1);
    lData.dropLinks(toBeDropepd);


    REQUIRE(lData.CT0_flat.size() == 6);


    lRet2 = lData.toTransportLink(0);
    REQUIRE(lRet2.globalKey == 0);
    REQUIRE(lRet2.CT[0][0] == 11);
    REQUIRE(lRet2.CT[1][0] == 22);

    lRet3 = lData.toTransportLink(1);
    REQUIRE(lRet3.globalKey == 3);
    REQUIRE(lRet3.CT[0][0] == 77);
    REQUIRE(lRet3.CT[1][0] == 88);

    lRet4 = lData.toTransportLink(2);
    REQUIRE(lRet4.globalKey == 2);
    REQUIRE(lRet4.CT[0][0] == 55);
    REQUIRE(lRet4.CT[1][0] == 66);

    for(auto v: lData.sideIndEndA)
    {
        REQUIRE(v < 6);
    }
    for(auto v: lData.sideIndEndB)
    {
        REQUIRE(v < 6);
    }

}


TEST_CASE("CT layout change to SoA tester", "[HiDEMLinkData]" ) {


    HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles = make3Particles();

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    l1.owner = 0;
    l1.state = 1;
    l1.CT[0][0] = 11;
    l1.CT[1][0] = 22;
    links.push_back(l1);

    HiDEMTransportLink l2;
    l2.globalKey = 1;
    l2.globalKeyParticleA = 0;
    l2.globalKeyParticleB = 2;
    l2.owner = 0;
    l2.state = 1;
    l2.CT[0][0] = 33;
    l2.CT[1][0] = 44;
    links.push_back(l2);

    HiDEMTransportLink l3;
    l3.globalKey = 2;
    l3.globalKeyParticleA = 1;
    l3.globalKeyParticleB = 2;
    l3.owner = 0;
    l3.state = 1;
    l3.CT[0][0] = 55;
    l3.CT[1][0] = 66;
    links.push_back(l3);


    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);



    REQUIRE(lData.CT0_flat[0] == 11);
    REQUIRE(lData.CT0_flat[1] == 22);
    REQUIRE(lData.CT0_flat[2] == 33);
    REQUIRE(lData.CT0_flat[3] == 44);
    REQUIRE(lData.CT0_flat[4] == 55);
    REQUIRE(lData.CT0_flat[5] == 66);

    lData.sortSideData(pData);

    std::vector<ind_t> iteratedOrder;
    for (int i = 0; i < 2; ++i) {
        for (int pInd = 0; pInd < 3; ++pInd) {
            ind_t lInd = lData.getPartLinkId(i,pInd);
            ind_t ctInd;
            if(lData.localPartIndA[lInd] == pInd)
                ctInd = lData.sideIndEndA[lInd];
            else
            {
                ctInd = lData.sideIndEndB[lInd];
            }
            iteratedOrder.push_back(ctInd);
        }
    }

    REQUIRE(iteratedOrder[0] == 0);
    REQUIRE(iteratedOrder[1] == 1);
    REQUIRE(iteratedOrder[2] == 2);
    REQUIRE(iteratedOrder[3] == 3);
    REQUIRE(iteratedOrder[4] == 4);
    REQUIRE(iteratedOrder[5] == 5);


}


TEST_CASE("Drop broken link tester", "[HiDEMLinkData]" ) {


        HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles = make3Particles();

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    l1.owner = 0;
    l1.state = 1;
    l1.CT[0][0] = 11;
    l1.CT[1][0] = 22;
    links.push_back(l1);

    HiDEMTransportLink l2;
    l2.globalKey = 1;
    l2.globalKeyParticleA = 0;
    l2.globalKeyParticleB = 2;
    l2.owner = 0;
    l2.state = 1;
    l2.CT[0][0] = 33;
    l2.CT[1][0] = 44;
    links.push_back(l2);

    HiDEMTransportLink l3;
    l3.globalKey = 2;
    l3.globalKeyParticleA = 1;
    l3.globalKeyParticleB = 2;
    l3.owner = 0;
    l3.state = 1;
    l3.CT[0][0] = 55;
    l3.CT[1][0] = 66;
    links.push_back(l3);


    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    REQUIRE(lData.numLocal == 3);

    REQUIRE(lData.CT0_flat[0] == 11);
    REQUIRE(lData.CT0_flat[1] == 22);
    REQUIRE(lData.CT0_flat[2] == 33);
    REQUIRE(lData.CT0_flat[3] == 44);
    REQUIRE(lData.CT0_flat[4] == 55);
    REQUIRE(lData.CT0_flat[5] == 66);

    REQUIRE(lData.sideIndEndA[0] == 0);
    REQUIRE(lData.sideIndEndA[1] == 2);
    REQUIRE(lData.sideIndEndA[2] == 4);

    REQUIRE(lData.sideIndEndB[0] == 1);
    REQUIRE(lData.sideIndEndB[1] == 3);
    REQUIRE(lData.sideIndEndB[2] == 5);


    lData.state[1] = 0;
    lData.dropBrokenLinks();

    REQUIRE(lData.numLocal == 2);
    REQUIRE(lData.CT0_flat.size() == 4);

    REQUIRE(lData.CT0_flat[0] == 11);
    REQUIRE(lData.CT0_flat[1] == 22);
    REQUIRE(lData.CT0_flat[2] == 55);
    REQUIRE(lData.CT0_flat[3] == 66);

    REQUIRE(lData.sideIndEndA[0] == 0);
    REQUIRE(lData.sideIndEndA[1] == 2);

    REQUIRE(lData.sideIndEndB[0] == 1);
    REQUIRE(lData.sideIndEndB[1] == 3);

}

TEST_CASE("Test getting of new order for links", "[HiDEMLinkData]" ) {
    //
    HiDEMParticleData pData;
    HiDEMLinkData lData;
    ManVec<HiDEMTransportParticle> particles = make4Particles();

    pData.appendTransportParticles(particles);

    GlobalLocalMapper<gKey_t, pInd_t> pMapper;
    pData.remap(pMapper);

    ManVec<HiDEMTransportLink> links;
    HiDEMTransportLink l1;
    l1.globalKey = 0;
    l1.globalKeyParticleA = 1;
    l1.globalKeyParticleB = 0;
    l1.owner = 0;
    l1.state = 1;
    l1.particleLinkNum = 0;
    links.push_back(l1);

    HiDEMTransportLink l2;
    l2.globalKey = 1;
    l2.globalKeyParticleA = 0;
    l2.globalKeyParticleB = 2;
    l2.owner = 0;
    l2.state = 1;
    l2.particleLinkNum = 1;
    links.push_back(l2);

    HiDEMTransportLink l3;
    l3.globalKey = 2;
    l3.globalKeyParticleA = 1;
    l3.globalKeyParticleB = 2;
    l3.owner = 0;
    l3.state = 1;
    l3.particleLinkNum = 1;
    links.push_back(l3);

    HiDEMTransportLink l4;
    l4.globalKey = 3;
    l4.globalKeyParticleA = 0;
    l4.globalKeyParticleB = 3;
    l4.owner = 0;
    l4.state = 1;
    l4.particleLinkNum = 0;
    links.push_back(l4);

    HiDEMTransportLink l5;
    l5.globalKey = 4;
    l5.globalKeyParticleA = 1;
    l5.globalKeyParticleB = 3;
    l5.owner = 0;
    l5.state = 1;
    l5.particleLinkNum = 2;
    links.push_back(l5);

    HiDEMTransportLink l6;
    l6.globalKey = 5;
    l6.globalKeyParticleA = 2;
    l6.globalKeyParticleB = 3;
    l6.owner = 0;
    l6.state = 1;
    l6.particleLinkNum = 0;
    links.push_back(l6);

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    auto order = lData.getNewOrder(pData);

    for(auto o: order)
    {
        std::cout << o << std::endl;
    }
    // todo fix  ?
    /*
    REQUIRE(order[0] == 5);
    REQUIRE(order[1] == 0);
    REQUIRE(order[2] == 2);
    REQUIRE(order[3] == 4);
    REQUIRE(order[4] == 3);
    REQUIRE(order[5] == 1);
*/

    #ifdef SOA_CT
    #else

    #endif

}