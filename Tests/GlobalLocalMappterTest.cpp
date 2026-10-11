//
// Created by Fredrik Robertsen on 23/02/23.
//
#include <catch2/catch.hpp>
#include "GlobalLocalMapper.h"


TEST_CASE( "Test Mapping", "[GlobalLocalMapper]" ) {

    GlobalLocalMapper<int, int> mapper;

    mapper.clear();
    mapper.map(12, 0);
    mapper.map(1, 1);
    mapper.map(112, 2);
    mapper.map(52, 3);
    mapper.map(23, 4);

    mapper.finalize();

    REQUIRE(mapper.getLocalInd(12) == 0);
    REQUIRE(mapper.getLocalInd(112) == 2);
    REQUIRE(mapper.getLocalInd(52) == 3);
    REQUIRE(mapper.getLocalInd(23) == 4);
    REQUIRE(mapper.getLocalInd(2) == -1);
    REQUIRE(mapper.getLocalInd(1) == 1);
    //
}

/*
TEST_CASE( "Test Mapping own map", "[GlobalLocalMapper]" ) {

    GlobalLocalMapper<uint64_t , int> mapper;

    mapper.setSize(5);
    mapper.clear();

    mapper.map(12, 0);
    mapper.map(1, 1);
    mapper.map(112, 2);
    mapper.map(52, 3);
    mapper.map(23, 4);

    mapper.finalize();
        std::cout << std::endl;

    REQUIRE(mapper.getLocalInd(112) == 2);
    REQUIRE(mapper.getLocalInd(12) == 0);
    REQUIRE(mapper.getLocalInd(52) == 3);
    REQUIRE(mapper.getLocalInd(23) == 4);
    REQUIRE(mapper.getLocalInd(2) == -1);
    REQUIRE(mapper.getLocalInd(1) == 1);
    //
}
 */