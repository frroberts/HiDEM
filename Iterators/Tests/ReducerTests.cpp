//
// Created by Fredrik Robertsen on 18/04/2022.
//

#include "catch2/catch.hpp"


#include "../ParallelReduce.h"


TEST_CASE( "Basic reduce", "[Parallel reduce]" ) {
    //
    ParallelReduce<double> red;


    double sum = red.reduce<double>(10, [] DEVCALLABLE (double a, double b){
        return a+b;
        }, [] DEVCALLABLE  (int index){
        return index;
    });
    REQUIRE(sum == 9+8+7+6+5+4+3+2+1);

    double max = red.reduce<double>(10, [] DEVCALLABLE  (double a, double b){
        if(a > b)
            return a;
        else return b;
        }, [] DEVCALLABLE (int index){
        return index;
    });
    REQUIRE(max == 9);

    double sum1k = red.reduce<double>(1024*2, [] DEVCALLABLE (double a, double b){
        return a+b;
        }, [] DEVCALLABLE (int index){
        return 1;
    });
    REQUIRE(sum1k == 1024*2);

    double sum32 = red.reduce<double>(32*45, [] DEVCALLABLE (double a, double b){
        return a+b;
        }, [] DEVCALLABLE (int index){
        return 1;
    });
    REQUIRE(sum32 == 32*45);

    double sum1 = red.reduce<double>(31251, [] DEVCALLABLE (double a, double b){
        return a+b;
        }, [] DEVCALLABLE (int index){
        return 1;
    });
    REQUIRE(sum1 == 31251);

 }

 TEST_CASE( "ManVec reduce", "[Parallel reduce]" ) {
    ManVec<double> data;
    ParallelReduce<double> red;

    for (size_t i = 0; i < 1331; i++)
    {
        data.push_back(1);
    }

    auto dataPtr = data.data();

    double sum1 = red.reduce<double>(data.size(), [] DEVCALLABLE (double a, double b){
        return a+b;
        }, [=] DEVCALLABLE (int index){
        return dataPtr[index];
    });
    REQUIRE(sum1 == 1331);

     
 }

TEST_CASE( "Reducer sum", "[Parallel reduce]" ) {
    ManVec<double> data;
    ParallelReduce<double> red;

    for (size_t i = 0; i < 1331; i++)
    {
        data.push_back(1);
    }

    auto dataPtr = data.data();

    double sum1 = red.sum(data);
    REQUIRE(sum1 == 1331);

     
}

TEST_CASE( "Reducer min max", "[Parallel reduce]" ) {
    ManVec<double> data;
    ParallelReduce<double> red;

    data.push_back(1);
    data.push_back(2);
    data.push_back(3);
    data.push_back(5);
    data.push_back(-1);
    data.push_back(-4);
    data.push_back(-9);

    double min;
    double max;
    red.minMax(min, max, data);
    REQUIRE(min == -9);
    REQUIRE(max == 5);
}

TEST_CASE( "Reducer min max indirect", "[Parallel reduce]" ) {
    ManVec<double> data;
    ManVec<int> indexes;
    ParallelReduce<double> red;

    data.push_back(1);
    data.push_back(2);
    data.push_back(3);
    data.push_back(5);
    data.push_back(-1);
    data.push_back(-4);
    data.push_back(-9);

    indexes.push_back(0);
    indexes.push_back(1);
    indexes.push_back(2);
    indexes.push_back(4);
    indexes.push_back(5);

    double min;
    double max;
    red.minMaxInderect(min, max, data, indexes);
    REQUIRE(min == -4);
    REQUIRE(max == 3);
}

TEST_CASE( "Reducer min max indirect large", "[Parallel reduce]" ) {
    ManVec<double> data;
    ManVec<int> indexes;
    ParallelReduce<double> red;

    int count = 100000;

    for (int i = 0; i < count; ++i) {
        data.push_back(1);
    }

    data[12345] = 3;
    data[12346] = 9;
    data[67123] = -4;

    for (int i = 0; i < count; ++i) {
        if(i == 12346)
            continue;
        indexes.push_back(i);
    }

    double min;
    double max;
    red.minMaxInderect(min, max, data, indexes);
    REQUIRE(min == -4);
    REQUIRE(max == 3);
}