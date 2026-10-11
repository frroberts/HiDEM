//
// Created by Fredrik Robertsen on 22/08/2021.
//

#include <catch2/catch.hpp>

#include <ManVec.h>
#include <Balancer/Balancer.h>

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
    std::uniform_real_distribution<> dis(0, 1);
    for (int x = 0; x < 100; ++x) {
        for (int y = 0; y < 100; ++y) {
            for (int z = 0; z < 40; ++z) {
                for (int w = 0; w < 20; ++w) {

                xCord.push_back((double)x + dis(gen));
                yCord.push_back((double)y + dis(gen));
                zCord.push_back((double)z + dis(gen));
                weights.push_back(1);
}
            }
        }
    }
/*
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
*/

    for (int x = 50; x < 100; ++x) {
        for (int y = 50; y < 100; ++y) {
            for (int z = 0; z < 20; ++z) {
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

    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();

    auto domains = geomBalancer.balance(32, 0, bounding);
    std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << elapsed_seconds.count() << " seconds "<< std::endl;
    for (auto d: domains) {
        std::cout << d.load << std::endl;
    }
}

TEST_CASE( "Balancer performance", "[Balancer]" ) {
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
    std::uniform_real_distribution<> dis(0, 1);
    for (int x = 0; x < 100; ++x) {
        for (int y = 0; y < 100; ++y) {
            for (int z = 0; z < 40; ++z) {
                for (int w = 0; w < 20; ++w) {
                xCord.push_back((double)x + dis(gen));
                yCord.push_back((double)y + dis(gen));
                zCord.push_back((double)z + dis(gen));
                weights.push_back(1);
                }
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
            for (int z = 0; z < 20; ++z) {
                xCord.push_back((double)x + dis(gen));
                yCord.push_back((double)y + dis(gen));
                zCord.push_back((double)z + dis(gen));
                weights.push_back(1);
            }
        }
    }

    std::array<int, 10> counts = {2,4,8,16,32,64,128,256,512,1024};
    for(auto c : counts) {

    Domain bounding;
    bounding.upper = {xMax+1, yMax+1, zMax+1};
    bounding.lower = {xMin, yMin, zMin};

    Balancer geomBalancer;
    geomBalancer.zActive = false;
    geomBalancer.tolerance = 0.0001;
    geomBalancer.verbose = false;
    geomBalancer.setBuffers(xCord, yCord, zCord, weights);

    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();

    auto domains = geomBalancer.balance(c, 0, bounding);
    std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << elapsed_seconds.count() << " seconds for domains " << c << std::endl;
    }
}