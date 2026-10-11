//
// Created by Fredrik Robertsen on 07/11/2020.
//

#include <algorithm>

#include <catch2/catch.hpp>
#include <ManVec.h>

TEST_CASE( "Test adding", "[ManVec]" ) {
    ManVec<int> v;

    size_t startCap = v.capacity();

    v.push_back(1);
    v.push_back(2);
    v.push_back(3);

    REQUIRE(v[0] == 1);
    REQUIRE(v[1] == 2);
    REQUIRE(v[2] == 3);
    REQUIRE(v.size() == 3);

    // should not have reallocated ...
    REQUIRE(startCap == v.capacity());

}


TEST_CASE( "Test adding w realloc", "[ManVec]" ) {
    ManVec<int> v;

    size_t startCap = v.capacity();

    for (int i = 0; i < 1025; ++i) {
        v.push_back(1);
    }

    REQUIRE(v.size() == 1025);

    // should have reallocated ...
    REQUIRE(startCap != v.capacity());

}

TEST_CASE( "Test begin(), end()", "[ManVec]" ) {
    //
    ManVec<int> v;

    v.push_back(0);
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);

    int curr = 0;
    for(const auto &i: v)
    {
        REQUIRE(i==curr);
        curr++;
    }
}

TEST_CASE( "Test std sort integration", "[ManVec]" ) {
    //
    ManVec<int> v;

    v.push_back(3);
    v.push_back(2);
    v.push_back(1);
    v.push_back(0);

    std::sort(v.begin(), v.end());

    int curr = 0;
    for(const auto &i: v)
    {
        REQUIRE(i==curr);
        curr++;
    }
}

TEST_CASE( "Test resize 1", "[ManVec]" ) {
    //
    ManVec<int> v;

    v.clear();
    v.resize(2000);

    REQUIRE(v.size() == 2000);
    REQUIRE(v.capacity() == (int)((2000+1)*1.2));

}