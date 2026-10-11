//
// Created by Fredrik Robertsen on 22/08/2021.
//

#include <catch2/catch.hpp>

#include <ManVec.h>
#include <Balancer/Balancer.h>

TEST_CASE( "Balancer tester", "[Balancer]" ) {
    ManVec<double> yCord;
    ManVec<double> xCord;
    ManVec<double> zCord;
    ManVec<double> weights;

    double xMax = 100;
    double yMax = 100;
    double zMax = 40;
    double xMin = 0;
    double yMin = 0;
    double zMin = 0;

    std::mt19937 gen(1);
    std::uniform_real_distribution<> dis(0, 0);
    for (int x = 0; x < 100; ++x) {
        for (int y = 0; y < 100; ++y) {
            for (int z = 0; z < 40; ++z) {
                xCord.push_back((double)x + dis(gen));
                yCord.push_back((double)y + dis(gen));
                zCord.push_back((double)z + dis(gen));
                weights.push_back(1);
            }
        }
    }
    for (int x = 0; x < 30; ++x) {
        for (int y = 0; y < 30; ++y) {
            for (int z = 0; z < 40; ++z) {
                xCord.push_back((double)x + dis(gen));
                yCord.push_back((double)y + dis(gen));
                zCord.push_back((double)z + dis(gen));
                weights.push_back(1);
            }
        }
    }
    for (int x = 0; x < 10; ++x) {
        for (int y = 0; y < 10; ++y) {
            for (int z = 0; z < 40; ++z) {
                xCord.push_back((double)x + dis(gen));
                yCord.push_back((double)y + dis(gen));
                zCord.push_back((double)z + dis(gen));
                weights.push_back(4);
            }
        }
    }

    for (int x = 50; x < 100; ++x) {
        for (int y = 50; y < 100; ++y) {
            for (int z = 0; z < 40; ++z) {
                xCord.push_back((double)x + dis(gen));
                yCord.push_back((double)y + dis(gen));
                zCord.push_back((double)z + dis(gen));
                weights.push_back(1);
            }
        }
    }

    Domain bounding;
    bounding.upper = {xMax+1, yMax+1, zMax+1};
    bounding.lower = {xMin, yMin, zMin};

    Balancer geomBalancer;
    geomBalancer.zActive = false;
    geomBalancer.tolerance = 0.0001;
    geomBalancer.verbose = false;
    geomBalancer.setBuffers(xCord, yCord, zCord, weights);
    auto domains = geomBalancer.balance(37, 0, bounding);

    for(auto d: domains)
    {
        // std::cout << d.load << std::endl;
    }
}