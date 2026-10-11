//
// Created by Fredrik Robertsen on 30/12/2020.
//

#include <catch2/catch.hpp>

#include <Transport/CommunicatorIndividual.h>
#include <Iterator.h>
#include "Generators/FCCParticleGenerator.h"

#include <Domain.h>
#include <World.h>

#include "Generators/LinkGenerator.h"
#include <Transport/Transporter.h>
#include <Util/Util.h>

#include "TestHelpers.h"

#include "../HiDEM/HiDEMTransportParticle.h"
#include "../HiDEM/HiDEMParticleData.h"
#include "../HiDEM/Distribution/Distribution.h"


TEST_CASE( "Generating split FCC lattice", "[Communicator2Rank]" ) {
    //
    double spacing = 2*100.0;

    //World world;
    HeightMap <double> bed(2,2,0,0,400,400);
    bed.setPointInd(0, 0, 0);
    bed.setPointInd(1, 0, 0);
    bed.setPointInd(0, 1, 0);
    bed.setPointInd(1, 1, 0);

    HeightMap <double> surf(2,2,0,0,400,400);
    surf.setPointInd(0, 0, 400);
    surf.setPointInd(1, 0, 400);
    surf.setPointInd(0, 1, 400);
    surf.setPointInd(1, 1, 400);

    Real3 lowerLimit = {bed.xMin, bed.yMin, bed.getMin()};
    Real3 upperLimit = {bed.xMax, bed.yMax, surf.getMax()};
    Domain d;
    d.upper = upperLimit;
    d.lower = lowerLimit;
    FCCParticleGenerator<HiDEMTransportParticle> pGen(lowerLimit, upperLimit);
    std::vector<HiDEMTransportParticle> particlesSingle = pGen.generateFCC(spacing, bed, surf, d);
    std::cout << "single domain generated " << particlesSingle.size() << " particles" << std::endl;

    HiDEMParticleData pDataSingle;
    pDataSingle.appendTransportParticles(particlesSingle);

    GlobalLocalMapper pMapperSingle;
    pDataSingle.remap(pMapperSingle);

    LinkGenerator lGen;
    auto linksSingle = lGen.generateLinksDistance((spacing / 2.0)*1.1, pDataSingle);

    HiDEMLinkData lDataSingle;
    lDataSingle.appendInitLinks(linksSingle);
    lDataSingle.connectParticleLinks(pMapperSingle);


    Distribution distr(false);
    distr.generateDomains(2, 1, lowerLimit, upperLimit, spacing*4);

    std::vector<HiDEMTransportParticle> particles = pGen.generateFCC(spacing, bed, surf, distr.world.domains[distr.world.self]);
    std::cout << distr.world.self << " rank particles " <<  particles.size() << std::endl;
    HiDEMParticleData pData;
    pData.appendTransportParticles(particles);

    std::vector<HiDEMTransportParticle> distributedParticles;
    distributedParticles.resize(particlesSingle.size());

    MPI_Gather(particles.data(), particles.size()*sizeof(HiDEMTransportParticle), MPI_BYTE,
               distributedParticles.data(), particles.size()*sizeof(HiDEMTransportParticle),
               MPI_BYTE,0, MPI_COMM_WORLD);

    if (distr.world.self == 0) {
        // on rank 0 check that distributedParticles is the same as particlesSingle
        for (int i = 0; i < particlesSingle.size(); ++i) {
            REQUIRE(particlesSingle[i].cord.x == distributedParticles[i].cord.x);
            REQUIRE(particlesSingle[i].cord.y == distributedParticles[i].cord.y);
            REQUIRE(particlesSingle[i].cord.z == distributedParticles[i].cord.z);

            REQUIRE(particlesSingle[i].globalKey == distributedParticles[i].globalKey);
        }
        std::sort(distributedParticles.begin(), distributedParticles.end(),
                  [](HiDEMTransportParticle a, HiDEMTransportParticle b){return a.globalKey <= b.globalKey;});
        for (int i = 1; i < distributedParticles.size(); ++i) {
            REQUIRE(distributedParticles[i-1].globalKey != distributedParticles[i].globalKey);
        }

    }
    MPI_Barrier(MPI_COMM_WORLD);
    // make sure single and multi setup gets the same
    // and that we have no duplicate global keys

    GlobalLocalMapper pMapper;

    int totalCount;
    int localCount = particles.size();
    MPI_Allreduce(&localCount, &totalCount, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    REQUIRE(totalCount == particlesSingle.size());

    ParticleTrasporter<HiDEMParticleData, HiDEMTransportParticle> pTrans;
    pTrans.getOutgoingHalo(distr.world, pData);
    pTrans.getIncomming();
    pTrans.appendIncomming(pData);

    pData.remap(pMapper);

    std::cout << "num local " << pData.numLocal << std::endl;

    auto links = lGen.generateLinksDistance((spacing / 2.0) * 1.1, pData);

    localCount = links.size();
    MPI_Allreduce(&localCount, &totalCount, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    REQUIRE(totalCount == linksSingle.size());

    std::vector<HiDEMTransportLink> distributedLinks;
    distributedLinks.resize(linksSingle.size());

    int sizes[2] = {0, 0};
    int localSize = links.size()*sizeof(HiDEMTransportLink);
    MPI_Allgather(&localSize, 4, MPI_BYTE,
               sizes, 4,MPI_BYTE, MPI_COMM_WORLD);
    int displ[2] = {0, static_cast<int>(links.size()*sizeof(HiDEMTransportLink))};

    MPI_Allgatherv(links.data(), links.size()*sizeof(HiDEMTransportLink), MPI_BYTE,
               distributedLinks.data(), sizes, displ,
               MPI_BYTE, MPI_COMM_WORLD);

    // somewhat dumb way to test it, same a ids b ids and same number, should be enough ?
    std::sort(distributedLinks.begin(), distributedLinks.end(),[](auto a, auto b){return (a.globalKeyParticleA < b.globalKeyParticleA) ;});
    std::sort(linksSingle.begin(), linksSingle.end(),[](auto a, auto b){return (a.globalKeyParticleA < b.globalKeyParticleA) ;});

    if (distr.world.self == 0) {
        for (int i = 0; i < linksSingle.size(); ++i) {
            REQUIRE(linksSingle[i].globalKeyParticleA == distributedLinks[i].globalKeyParticleA);
        }
    }
    std::sort(distributedLinks.begin(), distributedLinks.end(),[](auto a, auto b){return (a.globalKeyParticleB < b.globalKeyParticleB) ;});
    std::sort(linksSingle.begin(), linksSingle.end(),[](auto a, auto b){return (a.globalKeyParticleB < b.globalKeyParticleB) ;});

    if (distr.world.self == 0) {
        for (int i = 0; i < linksSingle.size(); ++i) {
            REQUIRE(linksSingle[i].globalKeyParticleB == distributedLinks[i].globalKeyParticleB);
        }
    }


    /*
    std::sort(distributedLinks.begin(), distributedLinks.end(),[](auto a, auto b){return (a.globalKey < b.globalKey) ;});
    std::sort(linksSingle.begin(), linksSingle.end(),[](auto a, auto b){return (a.globalKey < b.globalKey) ;});

    if (distr.world.self == 0) {
        for (int i = 0; i < distributedLinks.size(); ++i) {
            //
            if((distributedLinks[i].globalKey != linksSingle[i].globalKey) ||
            (distributedLinks[i].globalKeyParticleA != linksSingle[i].globalKeyParticleA) ||
            (distributedLinks[i].globalKeyParticleB != linksSingle[i].globalKeyParticleB)) {
            std::cout << distributedLinks[i].globalKey << " " << distributedLinks[i].globalKeyParticleA << " " << distributedLinks[i].globalKeyParticleB << " | ";
            std::cout << linksSingle[i].globalKey << " " << linksSingle[i].globalKeyParticleA << " " << linksSingle[i].globalKeyParticleB << " | ";
                std::cout << "<----";
            std::cout << std::endl;
            }

            REQUIRE(linksSingle[i].globalKey == distributedLinks[i].globalKey);
            REQUIRE(linksSingle[i].globalKeyParticleA == distributedLinks[i].globalKeyParticleA);
            REQUIRE(linksSingle[i].globalKeyParticleB == distributedLinks[i].globalKeyParticleB);
        }
    }
     */



    HiDEMLinkData lData;

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);

    // make sure we have the same amount of shared links ? what am i trying to test here really ?
    int numSharedLinks = 0;
    lData.iterateAllLinks(SerialExec(), [&](ind_t lInd, ind_t aPInd, ind_t bPInd, HiDEMLinkData &lData) {
        //
        if (pData.owners[aPInd] != distr.world.self)
            numSharedLinks++;
        if (pData.owners[bPInd] != distr.world.self)
            numSharedLinks++;
    }, lData);

    int numSharedLinksGlobal;
    MPI_Allreduce(&numSharedLinks, &numSharedLinksGlobal, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    LinkTransporter lTrans;
    lTrans.getOutgoingHalo(distr.world, pData, lData);
    lTrans.getIncomming();
    lTrans.appendIncomming(lData);
    lData.connectParticleLinks(pMapper);


    GlobalLocalMapper lMapper;
    lData.remap(lMapper);

    numSharedLinks = 0;
    lData.iterateAllLinks(SerialExec(), [&](ind_t lInd, ind_t aPInd, ind_t bPInd, HiDEMLinkData &lData) {
        //
        if (pData.owners[aPInd] != distr.world.self)
            numSharedLinks++;
        if (pData.owners[bPInd] != distr.world.self)
            numSharedLinks++;
    }, lData);

    REQUIRE(numSharedLinks != 0);

    REQUIRE(numSharedLinksGlobal == numSharedLinks);

    // verify that single and multiple is the same

    std::unordered_map<ind_t, int> linkCountSingle;
    pData.iterateOwnedParticles(SerialExec(), [&](ind_t pInd, HiDEMParticleData &pData){
        //
        int numLinks = 0;
        lData.iterateParticleLinks([&](ind_t lInd, ind_t pIndOhter, HiDEMLinkData &ldata){
            numLinks++;
            }, pInd, lDataSingle);
        linkCountSingle[pInd] = numLinks;
        }, pDataSingle);

    std::unordered_map<ind_t, int> linkCountMulti;
    pData.iterateOwnedParticles(SerialExec(), [&](ind_t pInd, HiDEMParticleData &pData){
        //
        int numLinks = 0;
        lData.iterateParticleLinks([&](ind_t lInd, ind_t pIndOhter, HiDEMLinkData &ldata){
            numLinks++;
            }, pInd, lData);
        linkCountMulti[pInd] = numLinks;
        }, pData);

    for(auto lc: linkCountMulti)
    {
        /*
        if(linkCountSingle[lc.first] > lc.second)
            std::cout << "a " << distr.world.self << " " << lc.first << " " << linkCountSingle[lc.first] << " " << lc.second << std::endl;
        if(linkCountSingle[lc.first] < lc.second)
            std::cout << "b " << distr.world.self << " " << lc.first << " " << linkCountSingle[lc.first] << " " << lc.second << std::endl;
        //REQUIRE(linkCountSingle[lc.first] == lc.second);
         */
    }


    auto lComm = lTrans.getCommunicator<bool>();
    auto pComm = pTrans.getCommunicator<double>();
    lComm.makeLocalIndexes(lMapper);
    pComm.makeLocalIndexes(pMapper);

    pComm.setNumBuffers(1);

    pData.iterateAllParticles(SerialExec(), [&](ind_t pInd, HiDEMParticleData &pData){
        pData.xCordPrev[pInd] = pData.xCord[pInd];
        }, pData);

    pData.iterateOwnedParticles(SerialExec(), [&](ind_t pInd, HiDEMParticleData &pData){
        //
        pData.xCord[pInd] += 0.01;
        }, pData);
    pComm.packCommData(pData.xCord, 0);

    pComm.postRecvs();
    pComm.postSends();
    pComm.syncSends();
    pComm.syncRecvs();

    pData.iterateAllParticles(SerialExec(), [&](ind_t pInd, HiDEMParticleData &pData){
        //
        if(pData.owners[pInd] != distr.world.self) {
            REQUIRE(pData.xCord[pInd] != pData.xCordPrev[pInd] + 0.01) ;
        }

        }, pData);

    pComm.unpackCommData(pData.xCord, 0);

    pData.iterateAllParticles(SerialExec(), [&](ind_t pInd, HiDEMParticleData &pData){
        //
        REQUIRE(pData.xCord[pInd] == pData.xCordPrev[pInd] + 0.01) ;
        }, pData);
    // todo test link commnication
}

/*
TEST_CASE( "Particle and link transport full scale", "[Communicator2Rank]" ) {
    //
    //
    HeightMap <double> bed(2,2,0,0,4,4);
    bed.setPointInd(0, 0, 0);
    bed.setPointInd(1, 0, 0);
    bed.setPointInd(0, 1, 0);
    bed.setPointInd(1, 1, 0);

    HeightMap <double> surf(2,2,0,0,4,4);
    surf.setPointInd(0, 0, 4);
    surf.setPointInd(1, 0, 4);
    surf.setPointInd(0, 1, 4);
    surf.setPointInd(1, 1, 4);

    Real3 lowerLimit = {bed.xMin, bed.yMin, bed.getMin()};
    Real3 upperLimit = {bed.xMax, bed.yMax, surf.getMax()};
    FCCParticleGenerator pGen(lowerLimit, upperLimit);

    StaticDistribution distr;
    distr.generateDomains(2, 1, lowerLimit, upperLimit, 1);

    double spacing = 0.5;
    std::vector<HiDEMTransportParticle> particles = pGen.generateFCC(spacing, bed, surf, distr.world.domains[distr.world.self]);

    HiDEMParticleData pData;
    pData.appendTransportParticles(particles);

    GlobalLocalMapper pMapper;
    pData.remap(pMapper);

    LinkGenerator lGen;
    auto links = lGen.generateLinksDistance(spacing / 2.0, pData);

    HiDEMLinkData lData;
    lData.appendTransportLinks(links);
    lData.connectParticleLinks(pMapper);
    GlobalLocalMapper lMapper;
    lData.remap(lMapper);


    int ownedPartCountPre = 0;
    int ownedPartCountPreGlobal = 0;
    int ownedLinkCountPre = 0;
    int ownedLinkCountPreGlobal = 0;
    pData.iterateOwnedParticles(SerialExec(), [&](ind_t pInd, HiDEMParticleData &pData){
        //
        pData.xCord[pInd] += 0.5;
        ownedPartCountPre++;
        }, pData);
    MPI_Allreduce(&ownedPartCountPre, &ownedPartCountPreGlobal, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    SerialIterator::iterateOwnedLinks([&](ind_t lInd, ind_t pIndA, ind_t pIndB, HiDEMLinkData &lData){
        ownedLinkCountPre++;
        }, lData);
    MPI_Allreduce(&ownedLinkCountPre, &ownedLinkCountPreGlobal, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    distr.distribute(pData, lData, pMapper, lMapper);

    int ownedPartCountPost = 0;
    int ownedPartCountPostGlobal = 0;
    int ownedLinkCountPost = 0;
    int ownedLinkCountPostGlobal = 0;

    Domain selfDomain = distr.world.domains[distr.world.self];
    pData.iterateOwnedParticles(SerialExec(), [&](ind_t pInd, HiDEMParticleData &pData){
        REQUIRE(selfDomain.within({pData.xCord[pInd], pData.yCord[pInd], pData.zCord[pInd]}));
        REQUIRE(pData.owner[pInd] == getLocalRank());
        ownedPartCountPost++;
    }, pData);

    MPI_Allreduce(&ownedPartCountPost, &ownedPartCountPostGlobal, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    REQUIRE(ownedPartCountPostGlobal == ownedPartCountPreGlobal);

    SerialIterator::iterateOwnedLinks([&](ind_t lInd, ind_t pIndA, ind_t pIndB, HiDEMLinkData &lData){
        ownedLinkCountPost++;
        }, lData);
    MPI_Allreduce(&ownedLinkCountPost, &ownedLinkCountPostGlobal, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    REQUIRE(ownedLinkCountPostGlobal == ownedLinkCountPreGlobal);


}
*/