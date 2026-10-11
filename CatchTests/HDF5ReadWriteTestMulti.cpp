//
// Created by Fredrik Robertsen on 04/12/2021.
//

#include <catch2/catch.hpp>

#include "TestHelpers.h"

#include "Generators/FCCParticleGenerator.h"
#include "Generators/LinkGenerator.h"
#include <H5IO/H5Checkpoint.h>
#include <H5IO/H5Checkpoint.h>
#include <H5IO/HDF5IO.h>

TEST_CASE( "2 ranks read/write data spann", "[H5IO2Rank]" ) {

    //
        HDF5IO h5;
    h5.createFile("h5TestSpanMulti.h5");
    int self = getLocalRank();
    std::vector<int> vec;
    for (size_t i = 0; i < 2; i++)
    {
        vec.push_back(self+1);
    }

    HDF5Span span;
    span.generate(vec.size());

    h5.writeSpan(span);
    h5.writeDataSpan(vec.data(), "test", span);
    h5.closeFile();

    h5.openFile("h5TestSpanMulti.h5");

    auto spans = h5.readSpan();

    REQUIRE(spans.size() == 2);

    REQUIRE(spans[0].globalCount == 4);
    REQUIRE(spans[0].localCount == 2);
    REQUIRE(spans[0].localOffset == 0);
    REQUIRE(spans[1].localOffset == 2);

    auto out = h5.readVectorSpann<int>("test", spans[self]);

    REQUIRE(out.size() == 2);
    for (size_t i = 0; i < 2; i++)
    {
        REQUIRE(vec[i] == out[i]);
    }

    h5.closeFile();
}

TEST_CASE( "2 ranks write indexed data spann", "[H5IO2Rank]" ) {

    //
        HDF5IO h5;
    h5.createFile("h5TestSpanMulti.h5");
    int self = getLocalRank();
    std::vector<int> vec;
    for (size_t i = 0; i < 4; i++)
    {
        vec.push_back(self+1);
    }

    std::vector<ind_t > indexes;

    indexes.push_back(1);
    indexes.push_back(2);

    HDF5Span span;
    span.generate(indexes.size());

    h5.writeSpan(span);
    h5.writeDataIndexedSpan(vec.data(), vec.size(), indexes.data(), "test", span);
    h5.closeFile();

    h5.openFile("h5TestSpanMulti.h5");

    auto spans = h5.readSpan();

    REQUIRE(spans.size() == 2);

    REQUIRE(spans[0].globalCount == 4);
    REQUIRE(spans[0].localCount == 2);
    REQUIRE(spans[0].localOffset == 0);
    REQUIRE(spans[1].localCount == 2);
    REQUIRE(spans[1].localOffset == 2);

    auto out = h5.readVectorSpann<int>("test", spans[self]);

    REQUIRE(out.size() == 2);
        REQUIRE(out[0] == self+1);
        REQUIRE(out[1] == self+1);

    h5.closeFile();
}