//
// Created by Fredrik Robertsen on 24/01/2021.
//

#include <catch2/catch.hpp>


#include <Transport/CommunicatorIndividual.h>
#include "Generators/FCCParticleGenerator.h"
#include <Iterator.h>

#include <Domain.h>
#include <World.h>

#include "Generators/LinkGenerator.h"
#include <Transport/Transporter.h>
#include <Util/Util.h>

#include "TestHelpers.h"

#include "../HiDEM/Distribution/Distribution.h"
#include "../HiDEM/HiDEMParticleData.h"
#include "../HiDEM/HiDEMTransportParticle.h"
#include "../HiDEM/Distribution/Distribution.h"


TEST_CASE( "Simulation test static distribution", "[StaticDistribution2Rank]" ) {
    double spacing = 0.5*100.0;

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

    LinkGenerator lGen;

    Distribution distr(false);
    distr.generateDomains(2, 1, lowerLimit, upperLimit, spacing*4);

    std::vector<HiDEMTransportParticle> particles = pGen.generateFCC(spacing, bed, surf, distr.world.domains[distr.world.self]);
    HiDEMParticleData pData;
    pData.appendTransportParticles(particles);

    GlobalLocalMapper pMapper;

    ParticleTrasporter<HiDEMParticleData, HiDEMTransportParticle> pTrans;
    pTrans.getOutgoingHalo(distr.world, pData);
    pTrans.getIncomming();
    pTrans.appendIncomming(pData);

    pData.remap(pMapper);

    auto links = lGen.generateLinksDistance((spacing / 2.0) * 1.1, pData);

    HiDEMLinkData lData;

    lData.appendInitLinks(links);
    lData.connectParticleLinks(pMapper);


    LinkTransporter lTrans;
    lTrans.getOutgoingHalo(distr.world, pData, lData);
    lTrans.getIncomming();
    lTrans.appendIncomming(lData);
    lData.connectParticleLinks(pMapper);

    GlobalLocalMapper lMapper;
    lData.remap(lMapper);

    for (int ts = 0; ts < 10; ++ts) {
        //NeighbourGrid &grid = *gridPtr;
        distr.distribute(pData, lData, pMapper, lMapper);
        distr.gatherHaloParticles(pData);

    }
}