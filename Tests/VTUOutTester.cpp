//
// Created by Fredrik Robertsen on 1/28/24.
//

#include <catch2/catch.hpp>
#include <ParticleData.h>
#include "OutputWriters/VtkParticles.h"


TEST_CASE( "Output particles to VTU", "[VTUOutput]" ) {

    ManVec<double> x;
    ManVec<double> y;
    ManVec<double> z;
    ManVec<float> dummy;

    x.push_back(0);
    y.push_back(1);
    z.push_back(2);
    dummy.push_back(3);

    VtkParticles partIo;

    partIo.createFile("particles.vtu");

    partIo.writePartMulti(x,y,z, {"dummy"}, dummy);

    partIo.closeFile();
}

TEST_CASE( "Output indexed particles to VTU", "[VTUOutput]" ) {
    ManVec<double> x;
    ManVec<double> y;
    ManVec<double> z;
    ManVec<float> dummy;
    ManVec<pInd_t> indexes;

    x.push_back(0);
    x.push_back(4);
    y.push_back(1);
    y.push_back(5);
    z.push_back(2);
    z.push_back(6);
    dummy.push_back(3);
    dummy.push_back(7);

    indexes.push_back(1);

    VtkParticles partIo;

    partIo.createFile("indexed.vtu");

    partIo.writePartMultiInd(x,y,z, indexes, {"dummy"}, dummy);

    partIo.closeFile();
}

TEST_CASE( "Output pred particles to VTU", "[VTUOutput]" ) {
    ManVec<double> x;
    ManVec<double> y;
    ManVec<double> z;
    ManVec<float> dummy;

    x.push_back(0);
    x.push_back(4);
    x.push_back(0);

    y.push_back(1);
    y.push_back(5);
    y.push_back(8);

    z.push_back(2);
    z.push_back(6);
    z.push_back(9);

    dummy.push_back(3);
    dummy.push_back(7);
    dummy.push_back(10);


    VtkParticles partIo;

    partIo.createFile("pred.vtu");

    partIo.writePartMultiPred(x,y,z, [&](int i){
        return x[i] == 0;
        }, {"dummy"}, dummy);

    partIo.closeFile();
}