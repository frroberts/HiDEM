//
// Created by Fredrik Robertsen on 08/10/2020.
//
#define CATCH_CONFIG_ENABLE_BENCHMARKING

//#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

int main( int argc, char* argv[] ) {
    int result = Catch::Session().run( argc, argv );
    return result;
}


