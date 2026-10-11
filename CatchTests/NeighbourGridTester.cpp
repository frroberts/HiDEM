//
// Created by Fredrik Robertsen on 09/10/2020.
//

#include <catch2/catch.hpp>
#include <NeighbourGrid.h>

#include "TestHelpers.h"

TEST_CASE( "Generate neighbour grid test", "[Neighbour grid]" ) {
    HiDEMParticleData data;

    generateBox(&data, 3, 1);

    NeighbourGrid grid;
    grid.generate(data, 1.0);


    REQUIRE(grid.xGridSize == 3);
    REQUIRE(grid.yGridSize == 3);
    REQUIRE(grid.zGridSize == 3);

}

TEST_CASE( "Generate neighbour grid test asymetric", "[Neighbour grid]" ) {
    HiDEMParticleData data;

    ManVec<HiDEMTransportParticle> particles;
    int key = 0;
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 4; ++z) {
                //
                HiDEMTransportParticle p;
                p.owner = 0;
                p.cord = {(double)x,(double)y,(double)z};
                p.globalKey = key;
                particles.push_back(p);
            }
        }
    }

    data.appendTransportParticles(particles);

    NeighbourGrid grid;
    grid.generate(data, 1.0);

    REQUIRE(grid.xGridSize == 2);
    REQUIRE(grid.yGridSize == 3);
    REQUIRE(grid.zGridSize == 4);

}


TEST_CASE( "Colour particle test", "[Neighbour grid]" ) {
    HiDEMParticleData data;

    generateBox(&data, 6, 1, true);

    NeighbourGrid grid;
    grid.generate(data, 2.0);
    grid.colourParticles(data);

    // check that we get 27 grid cells
    REQUIRE(grid.totalCells == 27);

    REQUIRE(grid.bucketStarts.size() == 27);
    REQUIRE(grid.bucketSizes.size() == 27);
    REQUIRE(grid.bucketStarts[0] == 0);
    REQUIRE(grid.bucketSizes[0] == 8);

    REQUIRE(grid.bucketStarts[1] == 8);

    //
}

TEST_CASE( "Generate iteration order list", "[Neighbour grid]" ) {
    HiDEMParticleData data;

//    generateBox(&data, 6, 1, true);
    ManVec<HiDEMTransportParticle> particles;
    HiDEMTransportParticle p;
    p.owner = 0;
    p.globalKey = 0;
    p.cord = {0,0,0};
    particles.push_back(p);
    p.globalKey = 1;
    p.cord = {1,0,0};
    particles.push_back(p);
    p.globalKey = 2;
    p.cord = {0.1,0,0};
    particles.push_back(p);
    p.globalKey = 3;
    p.cord = {1.1,0,0};
    particles.push_back(p);

    data.appendTransportParticles(particles);

    NeighbourGrid grid;
    grid.generate(data, 1.0);
    grid.colourParticles(data);
    grid.generateIterationList(data);

    // this will break if i fix it to not generate empty dimensions
    REQUIRE(grid.bucketSizes.size() == 2);

    REQUIRE(grid.bucketSizes[0] == 2);
    REQUIRE(grid.bucketSizes[1] == 2);
    REQUIRE(grid.bucketStarts[0] == 0);
    REQUIRE(grid.bucketStarts[1] == 2);

    REQUIRE(grid.iterationOrder.size() == data.xCord.size());


    // this may break on algorithm changes
    for (int i = grid.bucketStarts[0]; i < grid.bucketStarts[0] + grid.bucketSizes[0]; ++i) {
        //
        size_t pIndI = grid.iterationOrder[i];
        for (int j = grid.bucketStarts[1]; j < grid.bucketStarts[1] + grid.bucketSizes[1]; ++j) {
            //
            size_t pIndJ = grid.iterationOrder[j];
            REQUIRE(data.xCord[pIndI] < data.xCord[pIndJ]);
            REQUIRE(data.yCord[pIndI] == data.yCord[pIndJ]);
            REQUIRE(data.zCord[pIndI] == data.zCord[pIndJ]);
        }
    }
}

TEST_CASE( "Generate empty iteration order list", "[Neighbour grid]" ) {
    HiDEMParticleData data;

    generateBox(&data, 3, 1);

    NeighbourGrid grid;
    grid.generate(data, 0.5);
    grid.colourParticles(data);
    grid.generateIterationList(data);

    int neigbourCount = 0;

    data.iterateAllParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &data) {
        if (data.xCord[pInd] == 1 && data.yCord[pInd] == 1 && data.zCord[pInd] == 1) {
            //auto [xId, yId, zId] = grid.getGridId(data->xCord[pInd],data->xCord[pInd],data->xCord[pInd]);
            int colour = grid.getPraticleColor(pInd);//getGridColor(xId, yId, zId);

            grid.iteratePossibleNeighbours([&](size_t nInd) {
                if (pInd == nInd)
                    return;
                neigbourCount++;
            }, pInd, colour, true);
        }
    }, data);
    REQUIRE(neigbourCount == 0);
}




TEST_CASE( "Generate iteration order list offset cords", "[Neighbour grid]" ) {
    //

    HiDEMParticleData data;

    //generateBox(&data, 3, 1, true);
    int i = 0;
    ManVec<HiDEMTransportParticle> particles;
    for (double x = 0; x < 3; x+=0.5) {
        for (double y = 0; y < 3; y+=0.5) {
            for (double z = 0.25; z < 5; z+=0.5) {
                HiDEMTransportParticle p;
                p.owner = 0;
                p.cord = {(double)x,(double)y,(double)z};
                p.globalKey = i++;
                particles.push_back(p);
            }
        }
    }

    data.appendTransportParticles(particles);

    NeighbourGrid grid;
    grid.generate(data, 0.25);
    grid.colourParticles(data);
    grid.generateIterationList(data);


}
