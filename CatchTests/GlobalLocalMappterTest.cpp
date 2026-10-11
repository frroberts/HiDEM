//
// Created by Fredrik Robertsen on 23/02/23.
//
#include <catch2/catch.hpp>
#include "GlobalLocalMapper.h"


TEST_CASE( "Test individual Mapping", "[GlobalLocalMapper]" ) {
/*
    GlobalLocalMapperLinProbe<int, int> mapper;

    mapper.setSize(5);
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
    //''
    */
}


TEST_CASE( "Test bulk mapping", "[GlobalLocalMapper]" ) {

    auto *mapper = new GlobalLocalMapperLinProbe<gKey_t , int>();

    mapper->setSize(5);
    mapper->clear();

    ManVec<gKey_t> vec;
    vec.resize(5);
    vec[0] = 12;
    vec[1] = 1;
    vec[2] = 112;
    vec[3] = 52;
    vec[4] = 23;

    mapper->bulkMap(vec);
    mapper->finalize();

    REQUIRE(mapper->getLocalInd(112) == 2);
    REQUIRE(mapper->getLocalInd(12) == 0);
    REQUIRE(mapper->getLocalInd(52) == 3);
    REQUIRE(mapper->getLocalInd(23) == 4);
    REQUIRE(mapper->getLocalInd(2) == -1);
    REQUIRE(mapper->getLocalInd(1) == 1);
    //
}
