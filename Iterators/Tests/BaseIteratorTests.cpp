//
// Created by Fredrik Robertsen on 27/04/2022.
//

#include <algorithm>

#include <catch2/catch.hpp>
#include <ManVec.h>
#include <Iterator.h>

TEST_CASE( "Gather test", "[BaseIterTests]" ) {


    ManVec<double> arr;
    for (int i = 0; i < 100; ++i) {
        arr.push_back(i);
    }
    ManVec<double> buff;
    buff.resize(10);

    ManVec<int> indexes;
    indexes.push_back(0);
    indexes.push_back(10);
    indexes.push_back(20);
    indexes.push_back(30);
    indexes.push_back(40);
    indexes.push_back(50);
    indexes.push_back(60);
    indexes.push_back(70);
    indexes.push_back(80);
    indexes.push_back(90);

    Iterator::gatherMulti(OffloadExec(), indexes.size(), indexes.data(), buff.data(), arr.data());
    Iterator::syncAll();

    int j = 0;
    for (int i = 0; i < arr.size(); i+=10) {
        REQUIRE(buff[j] == i);
        j++;
    }
    REQUIRE(j == 10);
}

TEST_CASE( "Gather test multi", "[BaseIterTests]" ) {


    ManVec<double> arr1;
    ManVec<double> arr2;
    for (int i = 0; i < 100; ++i) {
        arr1.push_back(i);
        arr2.push_back(i+1);
    }
    ManVec<double> buff1;
    buff1.resize(10);
    ManVec<double> buff2;
    buff2.resize(10);

    ManVec<int> indexes;
    indexes.push_back(0);
    indexes.push_back(10);
    indexes.push_back(20);
    indexes.push_back(30);
    indexes.push_back(40);
    indexes.push_back(50);
    indexes.push_back(60);
    indexes.push_back(70);
    indexes.push_back(80);
    indexes.push_back(90);

    Iterator::gatherMulti(OffloadExec(), indexes.size(), indexes.data(),
                          buff1.data(), arr1.data(), buff2.data(), arr2.data());
    Iterator::syncAll();

    int j = 0;
    for (int i = 0; i < arr1.size(); i+=10) {
        REQUIRE(buff1[j] == i);
        REQUIRE(buff2[j] == i+1);
        j++;
    }
    REQUIRE(j == 10);
}

TEST_CASE( "Scatter test", "[BaseIterTests]" ) {


    ManVec<double> arr;
    for (int i = 0; i < 100; ++i) {
        arr.push_back(i);
    }
    ManVec<double> buff;
    buff.resize(10);

    ManVec<int> indexes;
    indexes.push_back(0);
    indexes.push_back(10);
    indexes.push_back(20);
    indexes.push_back(30);
    indexes.push_back(40);
    indexes.push_back(50);
    indexes.push_back(60);
    indexes.push_back(70);
    indexes.push_back(80);
    indexes.push_back(90);

    Iterator::gatherMulti(OffloadExec(), indexes.size(), indexes.data(), buff.data(), arr.data());
    Iterator::syncAll();

    int j = 0;
    for (int i = 0; i < arr.size(); i+=10) {
        REQUIRE(buff[j] == i);
        j++;
    }
    REQUIRE(j == 10);

    for (int i = 0; i < indexes.size(); ++i) {
        buff[i]+=1;
    }

    Iterator::scatterMulti(OffloadExec(), indexes.size(), indexes.data(), arr.data(), buff.data());
    Iterator::syncAll();

    for (int i = 0; i < arr.size(); i+=10) {
        REQUIRE(arr[i] == i+1);
    }
}

TEST_CASE( "Scatter test multi", "[BaseIterTests]" ) {


    ManVec<double> arr1;
    ManVec<double> arr2;
    for (int i = 0; i < 100; ++i) {
        arr1.push_back(i);
        arr2.push_back(i+1);
    }
    ManVec<double> buff1;
    buff1.resize(10);
    ManVec<double> buff2;
    buff2.resize(10);

    ManVec<int> indexes;
    indexes.push_back(0);
    indexes.push_back(10);
    indexes.push_back(20);
    indexes.push_back(30);
    indexes.push_back(40);
    indexes.push_back(50);
    indexes.push_back(60);
    indexes.push_back(70);
    indexes.push_back(80);
    indexes.push_back(90);

    Iterator::gatherMulti(OffloadExec(), indexes.size(), indexes.data(), buff1.data(), arr1.data(), buff2.data(), arr2.data());
    Iterator::syncAll();

    int j = 0;
    for (int i = 0; i < arr1.size(); i+=10) {
        REQUIRE(buff1[j] == i);
        j++;
    }

    for (int i = 0; i < indexes.size(); ++i) {
        buff1[i]+=1;
        buff2[i]+=1;
    }

    Iterator::scatterMulti(OffloadExec(), indexes.size(), indexes.data(), arr1.data(), buff1.data(), arr2.data(), buff2.data());
    Iterator::syncAll();

    for (int i = 0; i < arr1.size(); i+=10) {
        REQUIRE(arr1[i] == i+1);
        REQUIRE(arr2[i] == i+2);
    }
}