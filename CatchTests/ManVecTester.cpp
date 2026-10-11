//
// Created by Fredrik Robertsen on 7/26/23.
//
#include <catch2/catch.hpp>

#include <ManVec.h>
#include <Util/DropReorder.h>

TEST_CASE( "Test getting drop reorder", "[ManVec]" ) {
    //

    int cap = 1000;
    ManVec<int> vec(cap);

    for (int i = 0; i < cap; ++i) {
        vec[i] = 0;
    }

    ManVec<int> ind;

    std::default_random_engine e1(1);
    std::uniform_int_distribution<int> uniform_dist(0, cap-1);

    for (int i = 0; i < cap*0.25; ++i) {
        ind.push_back(uniform_dist(e1));
    }

    for (int i = 0; i < ind.size(); ++i) {
        vec[ind[i]] = 1;
    }

    ManVec<int> reoder;
    getDropReorder(ind, reoder, vec.size());

    reorderDevVec(vec,reoder);
    vec.resize(cap-ind.size());

    for (int i = 0; i < vec.size(); ++i) {
        REQUIRE(vec[i] != 1);
    }

}