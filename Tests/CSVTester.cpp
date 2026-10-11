//
// Created by Fredrik Robertsen on 04/11/2020.
//

#include <algorithm>
#include <random>
#include <iostream>

#include <catch2/catch.hpp>

#include <Util/CSVWriter.h>

TEST_CASE( "Test build line", "[CSV tester]" ) {
    CSVWriter writer("temp");

    std::cout << writer.buildLine(std::string("adas"),1, 1.0, true) << std::endl;
}