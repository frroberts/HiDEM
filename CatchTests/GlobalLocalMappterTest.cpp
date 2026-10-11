//
// Created by Fredrik Robertsen on 23/02/23.
//
#include <catch2/catch.hpp>
#include "OffloadHashMap.h"


TEST_CASE( "Test individual Mapping", "[GlobalLocalMapper]" ) {
/*
    OffloadHashMap<int, int> mapper;

    mapper.setSize(5);
    mapper.clear();

    mapper.mapImpl(12, 0);
    mapper.mapImpl(1, 1);
    mapper.mapImpl(112, 2);
    mapper.mapImpl(52, 3);
    mapper.mapImpl(23, 4);

    mapper.finalize();

    REQUIRE(mapper.getValue(12) == 0);
    REQUIRE(mapper.getValue(112) == 2);
    REQUIRE(mapper.getValue(52) == 3);
    REQUIRE(mapper.getValue(23) == 4);
    REQUIRE(mapper.getValue(2) == -1);
    REQUIRE(mapper.getValue(1) == 1);
    //''
    */
}

TEST_CASE( "Test key query", "[GlobalLocalMapper]" ) {

    OffloadHashMap<gKey_t, int> mapper;

    mapper.setSize(5);
    mapper.clear();

    mapper.mapImpl(12, 0);
    mapper.mapImpl(1, 1);
    mapper.mapImpl(112, 2);
    mapper.mapImpl(52, 3);
    mapper.mapImpl(23, 4);

    mapper.finalize();

    REQUIRE(mapper.getValue(12) == 0);
    REQUIRE(mapper.getValue(112) == 2);
    REQUIRE(mapper.getValue(52) == 3);
    REQUIRE(mapper.getValue(23) == 4);
    REQUIRE(mapper.getValue(2) == mapper.notFound());
    REQUIRE(mapper.getValue(1) == 1);
    REQUIRE(mapper.getValue(-1) == mapper.notFound());
    REQUIRE(mapper.getValue(-11) == mapper.notFound());

}



TEST_CASE( "Test bulk mapping", "[GlobalLocalMapper]" ) {

    auto *mapper = new OffloadHashMap<gKey_t , int>();

    mapper->setSize(5);
    mapper->clear();

    ManVec<gKey_t> vec;
    vec.resize(5);
    vec[0] = 12;
    vec[1] = 1;
    vec[2] = 112;
    vec[3] = 52;
    vec[4] = 23;

    mapper->bulkCreate(vec);
    mapper->finalize();

    REQUIRE(mapper->getValue(112) == 2);
    REQUIRE(mapper->getValue(12) == 0);
    REQUIRE(mapper->getValue(52) == 3);
    REQUIRE(mapper->getValue(23) == 4);
    REQUIRE(mapper->getValue(2) == mapper->notFound());
    REQUIRE(mapper->getValue(1) == 1);
    //
}


TEST_CASE( "Test bulk mapping large", "[GlobalLocalMapper]" ) {

    auto *mapper = new OffloadHashMap<gKey_t , int>();
    std::unordered_map<gKey_t, int> ref;

    int size = 10000;

    mapper->setSize(size);
    mapper->clear();

    ManVec<gKey_t> vec;

    std::mt19937 gen(123);
    //std::uniform_int_distribution<> distrib(0, size-1);

    for (int i = 0; i < size; i++) {
        vec.push_back(i*7);
    }
    std::shuffle(vec.begin(), vec.end(), gen);

    for (int i = 0; i < size; i++) {
        ref[vec[i]] = i;
    }

    mapper->bulkCreate(vec);
    mapper->finalize();

    int matchCount = 0;
    for (int i = 0; i < size; i++) {
        if (ref[vec[i]] == mapper->getValue(vec[i])) {
            matchCount++;
        };
    }
    REQUIRE(matchCount == size);


}
