//
// Created by Fredrik Robertsen on 25/12/2020.
//


#include <catch2/catch.hpp>

#include <Domain.h>
#include <World.h>

#include "../HiDEMLib/HiDEMTransportParticle.h"

TEST_CASE( "Test within", "[Domain]" ) {
    Domain d;
    d.upper = {1,1,1};
    d.lower = {0,0,0};

    d.upperHalo = {2,2,2};
    d.lowerHalo = {-1,-1,-1};

    REQUIRE(!d.within({1,1,1}));
    REQUIRE(d.within({0,0,0}));


    REQUIRE(!d.within({1.1,1.1,1.1}));
    REQUIRE(d.within({0.1,0.1,0.1}));


    REQUIRE(!d.withinHalo({2,2,2}));
    REQUIRE(d.withinHalo({-1,-1,-1}));


    REQUIRE(!d.withinHalo({2.1,2.1,2.1}));
    REQUIRE(d.withinHalo({-0.9,-0.9,-0.9}));

}

TEST_CASE( "Test Domain Math", "[Domain]" ) {
    Domain d;
    d.upper = {1,1,1};
    d.lower = {0,0,0};

    d.upperHalo = d.upper + Real3(1);
    d.lowerHalo = d.lower - Real3(1);

    REQUIRE(d.upperHalo.x == 2);
    REQUIRE(d.upperHalo.y == 2);
    REQUIRE(d.upperHalo.z == 2);

    REQUIRE(d.lowerHalo.x == -1);
    REQUIRE(d.lowerHalo.y == -1);
    REQUIRE(d.lowerHalo.z == -1);


}


TEST_CASE( "World test setup", "[Domain]" ) {
    //

    World world;
    Domain d1;
    d1.lower = {0,0,0};
    d1.upper = {1,1,1};
    d1.lowerHalo = {-1,-1,-1};
    d1.upperHalo = {2,2,2};
    world.domains.push_back(d1);
    Domain d2;
    d2.lower = {0,0,1};
    d2.upper = {1,1,2};
    d2.lowerHalo = {-1,-1,0};
    d2.upperHalo = {2,2,3};
    world.domains.push_back(d2);
    //
    HiDEMTransportParticle p1;
    p1.cord = {0.5};
    p1.owner = 0;
    HiDEMTransportParticle p2;
    p2.cord = {0.5,0.5,1.5};
    p2.owner = 1;

    REQUIRE(d2.within(p2.cord));
    REQUIRE(!d1.within(p2.cord));
    REQUIRE(d2.withinHalo(p2.cord));
    REQUIRE(d1.withinHalo(p2.cord));


}