//
// Created by Fredrik Robertsen on 19/10/2020.
//

#include <catch2/catch.hpp>

#include "TestHelpers.h"

#include <Geometry/Map2d.h>

TEST_CASE( "Interp test index calculation", "[Map2d]" ) {
    {
        Map2d<double> g(2,2,0,0,1,1);
        auto[xLow,yLow,xHigh,yHigh] = g.getIndexes(0,1);
        REQUIRE(xLow == 0);
        REQUIRE(yLow == 1);
        REQUIRE(xHigh == 1);
        REQUIRE(yHigh == 1);
    }
    {
        Map2d<double> g(4,4,0,0,1,1);
        auto[xLow,yLow,xHigh,yHigh] = g.getIndexes(0,1);
        REQUIRE(xLow == 0);
        REQUIRE(yLow == 3);
        REQUIRE(xHigh == 1);
        REQUIRE(yHigh == 3);
    }
    {
        Map2d<double> g(4,4,0,0,1,1);
        auto[xLow,yLow,xHigh,yHigh] = g.getIndexes(0.25,0.74);
        REQUIRE(xLow == 0);
        REQUIRE(yLow == 2);
        REQUIRE(xHigh == 1);
        REQUIRE(yHigh == 3);
    }
}

TEST_CASE( "Interp test 1", "[Map2d]" ) {
    //
    {
        Map2d<double> g(2, 2, 14, 20, 15, 21);
        g.setPointInd(0, 0, 91);
        g.setPointInd(1, 0, 210);
        g.setPointInd(0, 1, 162);
        g.setPointInd(1, 1, 95);
        double res = g.interp(14.5, 20.2);
        double delta = res - 146.1;
        REQUIRE(abs(delta) < 0.001);
    }
    {
        Map2d<double> g(3, 3, 0, 0, 2, 2);
        g.setPointInd(0, 0, 0);
        g.setPointInd(1, 0, 0);
        g.setPointInd(0, 1, 5);
        g.setPointInd(1, 1, 5);
        g.setPointInd(0, 2, 0);
        g.setPointInd(1, 2, 0);
        double res = g.interp(1, 1);
        double delta = res - 5;
        REQUIRE(abs(delta) < 0.001);
    }
    {
        Map2d<double> g(2, 2, 0, 0, 1, 1);
        g.setPointInd(0, 0, 0);
        g.setPointInd(1, 0, 0);
        g.setPointInd(0, 1, 1);
        g.setPointInd(1, 1, 1);
        double res = g.interp(0.5, 2) ;
        double delta = res - 1;
        REQUIRE(abs(delta) < 0.001);
    }
    {
        Map2d<double> g(2, 2, 0, 0, 1, 1);
        g.setPointInd(0, 0, 0);
        g.setPointInd(1, 0, 0);
        g.setPointInd(0, 1, 1);
        g.setPointInd(1, 1, 1);
        double res = g.interp(2, 0.5) ;
        double delta = res - 0.5;
        REQUIRE(abs(delta) < 0.001);
    }
}

TEST_CASE( "Create test negative size", "[Map2d]" ) {
    Map2d<double> g(3, 3, -2, -2, 2, 2);

    auto[xLow,yLow,xHigh,yHigh] = g.getIndexes(0.25,-1.74);
    REQUIRE(xLow == 1);
    REQUIRE(yLow == 0);
    REQUIRE(xHigh == 2);
    REQUIRE(yHigh == 1);

}

TEST_CASE( "Set point cord", "[Map2d]" ) {
    Map2d<double> g(2, 2, -2, -2, 2, 2);

    auto[xLow,yLow,xHigh,yHigh] = g.getIndexes(0.25,-1.74);
    g.setPointCord(-2, -2, 0);
    g.setPointCord(-2, 2, 0);
    g.setPointCord(2, -2, 1);
    g.setPointCord(2, 2, 1);

    double res = g.interp(0, 0) ;
    double delta = res - 0.5;
    REQUIRE(abs(delta) < 0.001);
}

TEST_CASE( "Set point cord non symetric", "[Map2d]" ) {
    Map2d<double> g(4, 8, -2, -2, 6, 2);

    auto[xLow,yLow,xHigh,yHigh] = g.getIndexes(0.25,-1.74);
    for (int i = -2; i < 6; i+=2) {
        for (float j = -2; j < 2; j+=0.5f) {
            g.setPointCord(i,j, 1);
        }
    }
    double res = g.interp(0, 0) ;
    double delta = res - 1.0;
    REQUIRE(abs(delta) < 0.001);
}