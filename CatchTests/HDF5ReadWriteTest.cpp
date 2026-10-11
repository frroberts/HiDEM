//

// Created by Fredrik Robertsen on 07/03/2021.
//
#include <catch2/catch.hpp>

#include "TestHelpers.h"

#include "Generators/FCCParticleGenerator.h"
#include "Generators/LinkGenerator.h"
#include <H5IO/H5Checkpoint.h>
#include <H5IO/HDF5IO.h>

#include "../HiDEMLib/HiDEMLinkData.h"

/*
TEST_CASE( "Test writing/reading data with HDF5", "[H5IO]" ) {
    //
    HDF5IO h5;
    h5.createFile("h5Test.h5");

    ManVec<int> outData;
    outData.push_back(1);
    outData.push_back(4);
    outData.push_back(3);
    outData.push_back(2);

    h5.writeVectorAll(outData, "singleTest");
    h5.closeFile();

    h5.openFile("h5Test.h5");

    ManVec<int> inData;

    inData = h5.readVectorOffset<int>(4,0, "singleTest");

    REQUIRE(outData.size() == inData.size());

    for (int i = 0; i < 4; ++i) {
        REQUIRE(outData[i] == inData[i]);
    }
    h5.closeFile();

}

TEST_CASE( "Test writing/reading data from group with HDF5", "[H5IO]" ) {
    //
    HDF5IO h5;
    h5.createFile("h5Test.h5");

    ManVec<int> outData;
    outData.push_back(1);
    outData.push_back(4);
    outData.push_back(3);
    outData.push_back(2);

    h5.openGroup("testGroup");
    h5.writeVectorAll(outData, "singleTest");
    h5.closeGroup();
    h5.closeFile();


    h5.openFile("h5Test.h5");
    h5.openGroup("testGroup");

    ManVec<int> inData;

    inData = h5.readVectorOffset<int>(4,0, "singleTest");

    REQUIRE(outData.size() == inData.size());

    for (int i = 0; i < 4; ++i) {
        REQUIRE(outData[i] == inData[i]);
    }
    h5.closeGroup();
    h5.closeFile();

}
 */

TEST_CASE( "Test writing/reading attribute with HDF5", "[H5IO]" ) {
    //
    HDF5IO h5;
    h5.createFile("h5Test.h5");
    h5.writeAttribute("attribute", 1.412);
    h5.closeFile();

    h5.openFile("h5Test.h5");
    double val = h5.readAttribute<double>("attribute");
    REQUIRE(val == 1.412);
    h5.closeFile();
}

TEST_CASE( "Test writing/reading attribute array with HDF5", "[H5IO]" ) {
    //
    HDF5IO h5;
    h5.createFile("h5Test.h5");
    std::vector<double> outData;
    outData.push_back(1);
    outData.push_back(4);
    outData.push_back(3);
    outData.push_back(2);
    h5.writeAttributeArr("attribute", outData);
    h5.closeFile();

    h5.openFile("h5Test.h5");
    std::vector inData = h5.readAttributeArr<double>("attribute");

    REQUIRE(outData.size() == inData.size());

    for (int i = 0; i < 4; ++i) {
        REQUIRE(outData[i] == inData[i]);
    }

    h5.closeFile();
}

/*
TEST_CASE( "Test writing indexed /reading data with HDF5", "[H5IO]" ) {
    //
    HDF5IO h5;
    h5.createFile("h5Test.h5");

    ManVec<int> outData;
    outData.push_back(1);
    outData.push_back(4);
    outData.push_back(3);
    outData.push_back(2);

    ManVec<ind_t> indexes;
    indexes.push_back(2);
    indexes.push_back(1);

    h5.writeVectorIndexed(outData, indexes, "singleTest");
    h5.closeFile();

    h5.openFile("h5Test.h5");

    ManVec<int> inData;

    inData = h5.readVectorOffset<int>(2,0, "singleTest");

    REQUIRE(inData.size() == 2);

    REQUIRE(inData[0] == outData[2]);
    REQUIRE(inData[1] == outData[1]);
    h5.closeFile();
}


/*
TEST_CASE( "Test writing/reading data with HDF5 parallel", "[H5IO]" ) {
    //
    HDF5IO h5;
    h5.createFile("h5TestAttrArr.h5");

    ManVec<int> outData;
    outData.push_back(1);
    outData.push_back(4);
    outData.push_back(3);
    outData.push_back(2);

    h5.writeVectorAll(outData, "h5attrArr", true);
    h5.closeFile();

    h5.openFile("h5TestAttrArr.h5");

    ManVec<int> inData;

    inData = h5.readVectorOwn<int>("h5attrArr");

    REQUIRE(outData.size() == inData.size());

    for (int i = 0; i < 4; ++i) {
        REQUIRE(outData[i] == inData[i]);
    }
    h5.closeFile();

}
*/


TEST_CASE( "Test writing/reading HDF5Span", "[H5IO]" ) {
    //
    HDF5IO h5;
    h5.createFile("h5Test.h5");

    HDF5Span spann;
    spann.generate(100);

    h5.writeSpan(spann);
    h5.closeFile();
    
    h5.openFile("h5Test.h5");

    auto spanns = h5.readSpan();

    REQUIRE(spanns.size() == 1);

    REQUIRE(spanns[0].globalCount == 100);
    REQUIRE(spanns[0].localCount == 100);
    REQUIRE(spanns[0].localOffset == 0);

    h5.closeFile();
}

TEST_CASE( "Test writing/reading vector with HDF5Span", "[H5IO]" ) {
    //
    HDF5IO h5;
    h5.createFile("h5TestSpann.h5");

    std::vector<int> vec;
    for (size_t i = 0; i < 10; i++)
    {
        vec.push_back(i);
    }
    
    HDF5Span spann;
    spann.generate(vec.size());

    h5.writeSpan(spann);
    h5.writeDataSpan(vec.data(), "test", spann);
    h5.closeFile();
    
    h5.openFile("h5TestSpann.h5");

    auto spanns = h5.readSpan();

    REQUIRE(spanns.size() == 1);

    REQUIRE(spanns[0].globalCount == 10);
    REQUIRE(spanns[0].localCount == 10);
    REQUIRE(spanns[0].localOffset == 0);

    auto out = h5.readVectorSpann<int>("test", spanns[0]);

    REQUIRE(out.size() == 10);    
    for (size_t i = 0; i < 10; i++)
    {
        REQUIRE(vec[i] == out[i]);
    }

    h5.closeFile();

}

/*
TEST_CASE( "Test hdf5 checkpoint v2", "[H5Checkpoint]" ) {
    //
    double spacing = 0.25;

    HiDEMParticleData pData;
    FCCParticleGenerator<HiDEMTransportParticle> pGen({0}, {2});
    auto bed = std::make_shared<HeightMap <double>>(2,2,0,0,1,1);
    bed->setPointInd(0, 0, 0);
    bed->setPointInd(1, 0, 0);
    bed->setPointInd(0, 1, 0);
    bed->setPointInd(1, 1, 0);

    auto surf = std::make_shared<HeightMap <double>> (2,2,0,0,1,1);
    surf->setPointInd(0, 0, 1);
    surf->setPointInd(1, 0, 1);
    surf->setPointInd(0, 1, 1);
    surf->setPointInd(1, 1, 1);

    Domain d;
    d.lower = {0, 0, 0};
    d.upper = {2, 2, 2};
    d.lowerHalo = {-1, -1, -1};
    d.upperHalo = {3, 3, 3};
    auto geometry = std::make_shared<InputDEM>();
    geometry->base = bed;
    geometry->surface = surf;

    auto particles = pGen.generateFCC(spacing, d, {geometry});
    pData.appendTransportParticles(particles);

    LinkGenerator lGen;

    auto links = lGen.generateLinksDistance(spacing / 2.0, pData);

    // randomize CT values
    std::mt19937 gen(1212);
    std::uniform_real_distribution<> dis(1.0, 2.0);

    for(auto &l: links)
    {
        l.CT[0][0] = dis(gen);
        l.CT[1][0] = dis(gen);

        l.CT[0][1] = dis(gen);
        l.CT[1][1] = dis(gen);

        l.CT[0][2] = dis(gen);
        l.CT[1][2] = dis(gen);

        l.CT[0][3] = dis(gen);
        l.CT[1][3] = dis(gen);

        l.CT[0][4] = dis(gen);
        l.CT[1][4] = dis(gen);

        l.CT[0][5] = dis(gen);
        l.CT[1][5] = dis(gen);
    }

    GlobalLocalMapper pMapper;
    pData.remap(pMapper);

    HiDEMLinkData lData;


        std::shuffle(links.begin(), links.end(), gen);

        links.resize(links.size() - 154);
        std::sort(links.begin(), links.end(), [&](HiDEMTransportLink a, HiDEMTransportLink b) {
            if (a.particleLinkNum > b.particleLinkNum)
                return true;
            else if (a.particleLinkNum == b.particleLinkNum) {
                return a.globalKeyParticleA > b.globalKeyParticleA;
            } else return false;
        });

    lData.appendTransportLinks(links);

    lData.state[1] = 0;
    lData.state[4] = 0;
    lData.state[6] = 0;
    lData.state[9] = 0;
    lData.state[123] = 0;


    H5Checkpoint io;
    io.newCheckpooint("testCheckpoint.h5");

    io.writeDomains(d);

    io.writeParticleData(pData);
    io.writeLinkData(lData);


    std::vector<HiDEMTransportParticle> readParticles = io.readParticleData();

    REQUIRE(particles.size() == readParticles.size());
    for(auto &pOrig: particles)
    {
        //
        auto pos = std::find_if(readParticles.begin(), readParticles.end(), [&](HiDEMTransportParticle a){
            return a.globalKey == pOrig.globalKey;
        });
        REQUIRE(pos != readParticles.end());
        REQUIRE(pos->cord == pOrig.cord);
        REQUIRE(pos->cordPrev == pOrig.cordPrev);
        REQUIRE(pos->theta == pOrig.theta);
        REQUIRE(pos->thetaPrev == pOrig.thetaPrev);
    }

    // todo generate new pData
    std::vector<HiDEMTransportLink> readLinks = io.readLinkData(pData);

    REQUIRE(links.size() == readLinks.size());
    for(auto &lOrig: links)
    {
        //
        auto pos = std::find_if(readLinks.begin(), readLinks.end(), [&](HiDEMTransportLink a){
            return a.globalKey == lOrig.globalKey;
        });
        REQUIRE(pos != readLinks.end());
        REQUIRE(pos->globalKey == lOrig.globalKey);
        REQUIRE(pos->globalKeyParticleA == lOrig.globalKeyParticleA);
        REQUIRE(pos->globalKeyParticleB == lOrig.globalKeyParticleB);

        REQUIRE(pos->CT[0][0] == lOrig.CT[0][0]);
        REQUIRE(pos->CT[1][0] == lOrig.CT[1][0]);
        REQUIRE(pos->CT[0][1] == lOrig.CT[0][1]);
        REQUIRE(pos->CT[1][1] == lOrig.CT[1][1]);
        REQUIRE(pos->CT[0][2] == lOrig.CT[0][2]);
        REQUIRE(pos->CT[1][2] == lOrig.CT[1][2]);
        REQUIRE(pos->CT[0][3] == lOrig.CT[0][3]);
        REQUIRE(pos->CT[1][3] == lOrig.CT[1][3]);
        REQUIRE(pos->CT[0][4] == lOrig.CT[0][4]);
        REQUIRE(pos->CT[1][4] == lOrig.CT[1][4]);
        REQUIRE(pos->CT[0][5] == lOrig.CT[0][5]);
        REQUIRE(pos->CT[1][5] == lOrig.CT[1][5]);
    }

    auto domains = io.readDomains();

    REQUIRE(domains[0].upper.x == d.upper.x);
    REQUIRE(domains[0].upper.y == d.upper.y);
    REQUIRE(domains[0].upper.z == d.upper.z);
    REQUIRE(domains[0].lower == d.lower);
    REQUIRE(domains[0].upperHalo == d.upperHalo);
    REQUIRE(domains[0].lowerHalo == d.lowerHalo);
    REQUIRE(domains[0].owner == d.owner);
}
 */

/*
TEST_CASE( "Test writing and reading back data with HDF5", "[H5Checkpoint]" ) {

    //
    double spacing = 0.25;

    HiDEMParticleData pData;
    FCCParticleGenerator<HiDEMTransportParticle> pGen({0}, {2});
    auto bed = std::make_shared<HeightMap <double>>(2,2,0,0,1,1);
    bed->setPointInd(0, 0, 0);
    bed->setPointInd(1, 0, 0);
    bed->setPointInd(0, 1, 0);
    bed->setPointInd(1, 1, 0);

    auto surf = std::make_shared<HeightMap <double>> (2,2,0,0,1,1);
    surf->setPointInd(0, 0, 1);
    surf->setPointInd(1, 0, 1);
    surf->setPointInd(0, 1, 1);
    surf->setPointInd(1, 1, 1);

    Domain d;
    d.lower = {0, 0, 0};
    d.upper = {2, 2, 2};
    auto geometry = std::make_shared<InputDEM>();
    geometry->base = bed;
    geometry->surface = surf;

    auto particles = pGen.generateFCC(spacing, d, {geometry}, 0);
    pData.appendTransportParticles(particles);

    LinkGenerator lGen;

    auto links = lGen.generateLinksDistance(spacing / 2.0, pData);

    // randomize CT values
    std::mt19937 gen(1212);
    std::uniform_real_distribution<> dis(1.0, 2.0);

    for(auto &l: links)
    {
        l.CT[0][0] = dis(gen);
        l.CT[1][0] = dis(gen);

        l.CT[0][1] = dis(gen);
        l.CT[1][1] = dis(gen);

        l.CT[0][2] = dis(gen);
        l.CT[1][2] = dis(gen);

        l.CT[0][3] = dis(gen);
        l.CT[1][3] = dis(gen);

        l.CT[0][4] = dis(gen);
        l.CT[1][4] = dis(gen);

        l.CT[0][5] = dis(gen);
        l.CT[1][5] = dis(gen);
    }

    GlobalLocalMapper pMapper;
    pData.remap(pMapper);

    HiDEMLinkData lData;

    lData.appendTransportLinks(links);

    H5Checkpoint io;

    io.writeParticleData("test_particles.h5", pData);
    io.writeLinkData("test_links.h5", lData);

    std::vector<HiDEMTransportParticle> readParticles = io.readParticleData("test_particles.h5");
    std::vector<HiDEMTransportLink> readLinks = io.readLinkData("test_links.h5");

    REQUIRE(particles.size() == readParticles.size());
    for(auto &pOrig: particles)
    {
        //
        auto pos = std::find_if(readParticles.begin(), readParticles.end(), [&](HiDEMTransportParticle a){
            return a.globalKey == pOrig.globalKey;
        });
        REQUIRE(pos != readParticles.end());
        REQUIRE(pos->cord == pOrig.cord);
        REQUIRE(pos->cordPrev == pOrig.cordPrev);
        REQUIRE(pos->theta == pOrig.theta);
        REQUIRE(pos->thetaPrev == pOrig.thetaPrev);
    }

    REQUIRE(links.size() == readLinks.size());
    for(auto &lOrig: links)
    {
        //
        auto pos = std::find_if(readLinks.begin(), readLinks.end(), [&](HiDEMTransportLink a){
            return a.globalKey == lOrig.globalKey;
        });
        REQUIRE(pos != readLinks.end());
        REQUIRE(pos->globalKey == lOrig.globalKey);
        REQUIRE(pos->globalKeyParticleA == lOrig.globalKeyParticleA);
        REQUIRE(pos->globalKeyParticleB == lOrig.globalKeyParticleB);

        REQUIRE(pos->CT[0][0] == lOrig.CT[0][0]);
        REQUIRE(pos->CT[1][0] == lOrig.CT[1][0]);
        REQUIRE(pos->CT[0][1] == lOrig.CT[0][1]);
        REQUIRE(pos->CT[1][1] == lOrig.CT[1][1]);
        REQUIRE(pos->CT[0][2] == lOrig.CT[0][2]);
        REQUIRE(pos->CT[1][2] == lOrig.CT[1][2]);
        REQUIRE(pos->CT[0][3] == lOrig.CT[0][3]);
        REQUIRE(pos->CT[1][3] == lOrig.CT[1][3]);
        REQUIRE(pos->CT[0][4] == lOrig.CT[0][4]);
        REQUIRE(pos->CT[1][4] == lOrig.CT[1][4]);
        REQUIRE(pos->CT[0][5] == lOrig.CT[0][5]);
        REQUIRE(pos->CT[1][5] == lOrig.CT[1][5]);
    }
}
 */
