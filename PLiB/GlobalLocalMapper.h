//
// Created by Fredrik Robertsen on 31/10/2020.
//

#ifndef PHELL_GLOBALLOCALMAPPER_H
#define PHELL_GLOBALLOCALMAPPER_H

#include <unordered_map>
#include <algorithm>

#include "Global.h"
#include "ManVec.h"
#include "Iterator.h"
#include <mpi.h>

#ifndef OWNHASHMAP

template <class gType, class lType>
class GlobalLocalMapper {
public:

    std::unordered_map<gType, lType> globalToLocal;

    lType getLocalInd(gType globalKey) {
        //
        auto pos = globalToLocal.find(globalKey);
        if (pos != globalToLocal.end()) {
            return (pos->second);
        }
        //exit(1);
        return -1;
    }

    void map(gType globalKey, lType localInd) {
        auto pos = globalToLocal.find(globalKey);
        if (pos != globalToLocal.end()) {
                int self;
    self = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &self);
            std::cout << self << " remapping existing original " << globalKey << " " << pos->second << " new " << localInd << std::endl;
            exit(1);
        }
        globalToLocal[globalKey] = localInd;
    }

    void finalize(){
        // option to optimize or even just build the table
    }

    void clear() {
        globalToLocal = std::unordered_map<gType, lType>();
    }

    void setSize(size_t size){
        // option to set the size
        globalToLocal.reserve(size);
    }

    size_t numActive(){
        return globalToLocal.size();
    }
};

#else
/*
// builds on the idea that all indexes get mapped, so the local key is the location of the global one
template <class gType, class lType>
class GlobalLocalMapperBinSearch {
public:

    ManVec<std::pair<gType, lType>> globalLocalLookup;

    DEVCALLABLE
    lType getLocalInd(gType globalKey) {
        // binary search for global key, return index it was found at

        int loc;
        int left = 0;
        int right = globalLocalLookup.size() - 1;
        while (left <= right) {
            loc = (left + right) / 2;
            if (globalLocalLookup[loc].first == globalKey) {
                return loc;

            } else if (globalLocalLookup[loc].first < globalKey)
                left = loc + 1;
            else
                right = loc - 1;
        }
        return -1;
    }
    
    DEVCALLABLE
    void map(gType globalKey, lType localInd) {
        //
        globalLocalLookup[localInd].first = globalKey;
        globalLocalLookup[localInd].second = localInd;
    }

    void finalize(){
        // std::sort(globalLocalLookup.begin(), globalLocalLookup.end(), [](auto a, auto b){ return a.first < b.first;});
    }

    void clear() {
        for (int i = 0; i < globalLocalLookup.size(); ++i) {
            globalLocalLookup[i].first = -1;
            globalLocalLookup[i].second = -1;
        }
    }

    void setSize(size_t size){
        // option to set the size
        globalLocalLookup.resize(size);
    }
};
 */

// builds on the idea that all indexes get mapped, so the local key is the location of the global one
template<class gType, class lType>
class GlobalLocalMapper : public ManagedParent{
public:

    DEVCALLABLE

    size_t hashFun(gType k) {

        k ^= k >> 33;
        k *= uint64_t(0xff51afd7ed558ccd);
        k ^= k >> 33;
        k *= uint64_t(0xc4ceb9fe1a85ec53);
        k ^= k >> 33;

        return k% (cap-1);;
    }

    // 32 bit Murmur3 hash
    /*
 uint32_t hashFun(uint32_t k)
{
    k ^= k >> 16;
    k *= 0x85ebca6b;
    k ^= k >> 13;
    k *= 0xc2b2ae35;
    k ^= k >> 16;
    return k % (cap-1);
}
     */

    int cap;
    int active = 0;

    ManVec<std::pair<gType, lType>> globalLocalLookup;

    DEVCALLABLE
    lType getLocalInd(gType globalKey) {
        gType place = hashFun(globalKey);
        while (globalLocalLookup[place].first != globalKey){
            if(globalLocalLookup[place].first == -1) {
                return -1;
            } else {
                place++;
                place = place % (cap-1);
            }
        };
        return globalLocalLookup[place].second;
    }

    void map(gType globalKey, lType localInd) {
        gType place = hashFun(globalKey);
        // std::cout << place << std::endl;

        do{
            if(globalLocalLookup[place].first == -1) {
                globalLocalLookup[place].first = globalKey;
                globalLocalLookup[place].second = localInd;
                break;
            } else {
                place++;
                place = place % (cap-1);
            }
        } while (globalLocalLookup[place].first != globalKey);

        active++;
    }

    void finalize(){
        // std::cout << "collisions " << collisions << " " << cap <<  " " << firstPlace << std::endl;
    }

    void clear() {
        for (int i = 0; i < cap; ++i) {
            globalLocalLookup[i].first = -1;
            globalLocalLookup[i].second = -1;
        }
        active = 0;
    }

    void setSize(size_t size){
        // set cap to 2x size to ensure load factor is not too high
        cap = size*4;
        globalLocalLookup.resize(cap);
    }

    size_t numActive(){
        return active;
    }
};

#endif

#endif //PHELL_GLOBALLOCALMAPPER_H
