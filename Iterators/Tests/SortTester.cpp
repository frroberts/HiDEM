//
// Created by Fredrik Robertsen on 18/04/2022.
//

#include "catch2/catch.hpp"

#include <iostream>
#include "../ParallelSort.h"



TEST_CASE( "1 level direct sort", "[Parallel sort]" ) {
    ParallelSort sorter;

    ManVec<int> valuesStd;
    ManVec<int> values;

    values.resize(8);

    values[7] = 0;
    values[6] = 1;
    values[5] = 2;
    values[4] = 3;
    values[3] = 4;
    values[2] = 5;
    values[1] = 6;
    values[0] = 7;

    valuesStd = values;

    std::sort( valuesStd.begin(), valuesStd.end(), [&](int i, int j) {
                    return i < j;
                    });

    sorter.sortDirect(values);

    for (int i = 0; i < 8; ++i) {
        REQUIRE(valuesStd[i]  == values[i]);
    }
}


TEST_CASE( "1 level N level indrect sort", "[Parallel sort]" ) {
    ParallelSort sorter;

    ManVec<int> valuesStd;
    ManVec<int> values;
    ManVec<int> keysL1;

    values.resize(8);
    keysL1.resize(8);


    values[7] = 0;
    values[6] = 1;
    values[5] = 2;
    values[4] = 3;
    values[3] = 4;
    values[2] = 5;
    values[1] = 6;
    values[0] = 7;

    valuesStd = values;

    keysL1[7] = 0;
    keysL1[6] = 1;
    keysL1[5] = 2;
    keysL1[4] = 3;
    keysL1[3] = 4;
    keysL1[2] = 5;
    keysL1[1] = 6;
    keysL1[0] = 7;

    std::sort( valuesStd.begin(), valuesStd.end(), [&](int i, int j) {
                    return keysL1[i] > keysL1[j];
    });

    sorter.sortIndirectNLevel(values, keysL1);

    for (int i = 0; i < 8; ++i) {
        //std::cout << valuesStd[i] << " " << values[i] << std::endl;
        REQUIRE(valuesStd[i]  == values[i]);
    }
}


TEST_CASE( "2 level N level indrect sort", "[Parallel sort]" ) {
    ParallelSort sorter;

    ManVec<int> valuesStd;
    ManVec<int> values;
    ManVec<int> keysL1;
    ManVec<int> keysL2;

    values.resize(8);
    keysL1.resize(8);
    keysL2.resize(8);
    keysL2.resize(8);


    values[7] = 0;
    values[6] = 1;
    values[5] = 2;
    values[4] = 3;
    values[3] = 4;
    values[2] = 5;
    values[1] = 6;
    values[0] = 7;

    valuesStd = values;

    keysL1[7] = 2;
    keysL1[6] = 2;
    keysL1[5] = 2;
    keysL1[4] = 2;
    keysL1[3] = 3;
    keysL1[2] = 3;
    keysL1[1] = 3;
    keysL1[0] = 3;

    keysL2[7] = 0;
    keysL2[6] = 2;
    keysL2[5] = 4;
    keysL2[4] = 5;
    keysL2[3] = 0;
    keysL2[2] = 1;
    keysL2[1] = 0;
    keysL2[0] = 0;

    std::sort(valuesStd.begin(), valuesStd.end(), [&](int i, int j) {
        if (keysL1[i] == keysL1[j]) {
            return keysL2[i] > keysL2[j];
        } else return keysL1[i] < keysL1[j];
    });

    sorter.sortIndirectNLevel(values, keysL1, keysL2);

    for (int i = 0; i < 8; ++i) {
        std::cout << valuesStd[i] << " " << values[i] << std::endl;
        // REQUIRE(valuesStd[i]  == values[i]);
    }
}

/*
TEST_CASE( "3 level indrect sort", "[Parallel sort]" ) {
    ParallelSort sorter;

    ManVec<int> valuesStd;
    ManVec<int> values;
    ManVec<int> keysL1;
    ManVec<int> keysL2;
    ManVec<int> keysL3;

    values.resize(8);
    keysL1.resize(8);
    keysL2.resize(8);
    keysL2.resize(8);


    values[7] = 0;
    values[6] = 1;
    values[5] = 2;
    values[4] = 3;
    values[3] = 4;
    values[2] = 5;
    values[1] = 6;
    values[0] = 7;

    valuesStd = values;

    keysL1[7] = 2;
    keysL1[6] = 2;
    keysL1[5] = 2;
    keysL1[4] = 2;
    keysL1[3] = 3;
    keysL1[2] = 3;
    keysL1[1] = 4;
    keysL1[0] = 5;

    keysL2[7] = 2;
    keysL2[6] = 2;
    keysL2[5] = 4;
    keysL2[4] = 4;
    keysL2[3] = 1;
    keysL2[2] = 1;
    keysL2[1] = 0;
    keysL2[0] = 0;

    keysL3[7] = 1;
    keysL3[6] = 2;
    keysL3[5] = 1;
    keysL3[4] = 2;
    keysL3[3] = 1;
    keysL3[2] = 2;
    keysL3[1] = 0;
    keysL3[0] = 0;

    std::sort( valuesStd.begin(), valuesStd.end(), [&](int i, int j) {
        if (keysL1[i] == keysL1[j]) {
            if (keysL2[i] == keysL2[j]) {
                    return keysL3[i] < keysL3[j];
            } else return keysL2[i] > keysL2[j];
        } else return keysL1[i] > keysL1[j];
    });

    sorter.sortIndirectNLevel(values, keysL1, keysL2);

    for (int i = 0; i < 8; ++i) {
        std::cout << valuesStd[i] << " " << values[i] << std::endl;
        // REQUIRE(valuesStd[i]  == values[i]);
    }
}
*/