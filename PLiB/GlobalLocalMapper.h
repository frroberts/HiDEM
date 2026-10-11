//
// Created by Fredrik Robertsen on 31/10/2020.
//

#ifndef PHELL_GLOBALLOCALMAPPER_H
#define PHELL_GLOBALLOCALMAPPER_H

#include <unordered_map>
#include <algorithm>
#include <chrono>

#include "Global.h"
#include "ManVec.h"
#include "Iterator.h"
#include <mpi.h>
#include "AtomicOps.h"

#include <chrono>

#ifdef GPU
#include <thrust/sort.h>
#include <thrust/execution_policy.h>
#define GLOBALLOCALMAPPEREXEC thrust::device,
namespace globalLocalMapperSTL = thrust;

#else
#include <execution>
// do ptstl here for host with pstl
// #define GLOBALLOCALMAPPEREXEC std::execution::par,
#define GLOBALLOCALMAPPEREXEC
namespace globalLocalMapperSTL = std;
#endif

template <class gType, class lType>
class GlobalLocalMapperLinProbe : public ManagedParent {

    // gcc also just does identity function for integer types
    DEVCALLABLE
    size_t hashFun(gType k) {
        return (k);
    }

public:

    gType sentinel = -1;
    ManVec<lType> values;
    ManVec<gType> keys;
    int numMapped = 0;
    float overSubscription = 2;
    int realSize;

    DEVCALLABLE
    lType getLocalInd(gType globalKey) {
        // needs to check if we find the sentinel as well and return -1 then
        int pos = hashFun(globalKey) % realSize;
        if (keys[pos] == globalKey) {
            return values[pos];
        } else {
            int searched = 0;
            while (keys[pos] != globalKey && searched != realSize) {
                pos = (pos + 1) % realSize;
                if (keys[pos] == globalKey) {
                    return values[pos];
                }
                searched++;
            }
        }
        return -1;
    }

    DEVCALLABLE
    void map(gType globalKey, lType localInd) {
        int pos = hashFun(globalKey) % realSize;
        int searched = 0;
        gType currentKey;
        do {
            {
                currentKey = keys[pos];
                if (currentKey == sentinel) {
                    keys[pos] = globalKey;
                }
            }
            if (currentKey == sentinel) {
                values[pos] = localInd;
                numMapped++;
                return;
            }
            pos = (pos + 1) % realSize;
            searched++;
        } while (searched != realSize);
    }

    DEVCALLABLE
    void mapImpl(gType globalKey, lType localInd) {
        int pos = hashFun(globalKey) % realSize;
        int searched = 0;
        gType currentKey;
        do {
            currentKey = Atomic::atomicCASVal(keys[pos],  sentinel, globalKey);

            if (currentKey == sentinel) {
                values[pos] = localInd;
                return;
            }
            pos = (pos + 1) % realSize;
            searched++;
        } while (searched != realSize);
    }



    void bulkCreate(ManVec<gKey_t> &gKeys) {
        //
        std::chrono::time_point<std::chrono::system_clock> start, end;
        start = std::chrono::system_clock::now();
        auto gKeysPtr = gKeys.data();
        Iterator::iterate(OffloadExec(), [=]DEVLAMBDA(int ind, GlobalLocalMapperLinProbe &mapper){
            mapper.mapImpl(gKeysPtr[ind], ind);
        }, gKeys.size(), *this);
        numMapped += gKeys.size();
        Iterator::syncAll();
        end = std::chrono::system_clock::now();
        auto diff = std::chrono::system_clock::now() - start;
        std::chrono::duration<double> elapsed = diff;
        std::cout << elapsed.count() << std::endl;
    }

    void finalize(){
        // option to optimize or even just build the table
    }

    void clear() {
        auto keysPtr = keys.data();
        Iterator::iterate(OffloadExec(), [=]DEVLAMBDA(int ind){
            keysPtr[ind] = -1;
        }, keys.size());
        Iterator::syncAll();
        numMapped = 0;
    }

    void setSize(size_t size){
        // option to set the size
        realSize = size*overSubscription;
        values.resize(realSize);
        keys.resize(realSize);
    }

    size_t numActive(){
        return numMapped;
    }
};


template <class gType, class lType>
class GlobalLocalMapperHash {
public:

    std::unordered_map<gType, lType> globalToLocal;

    lType getLocalInd(gType globalKey) {
        //
        auto pos = globalToLocal.find(globalKey);
        if (pos != globalToLocal.end()) {
            return (pos->second);
        }
        return -1;
    }

    void map(gType globalKey, lType localInd) {
        auto pos = globalToLocal.find(globalKey);
        if (pos != globalToLocal.end()) {
            int self;
            self = 0;
            MPI_Comm_rank(MPI_COMM_WORLD, &self);
            std::cout << self << " remapping existing original " << globalKey << " " << pos->second << " new " <<
                    localInd << std::endl;
            exit(1);
        }
        globalToLocal[globalKey] = localInd;
    }

    void bulkCreate(ManVec<gKey_t> &gKeys) {
        //
        for (ind_t i = 0; i < gKeys.size(); ++i) {
            map(gKeys[i], i);
        }
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

template <class gType, class lType>
class GlobalLocalMapperBinSearch : public ManagedParent{
public:

    ManVec<std::pair<gType, lType>> globalLocalLookup;

    int numMapped = 0;

    DEVCALLABLE
    lType getLocalInd(gType globalKey) {
        // binary search for global key, return index it was found at

        int left = 0;
        int right = globalLocalLookup.size() - 1;
        while (left <= right) {
            int m = (left + right) / 2;
            gType val = globalLocalLookup[m].first;
            if (val < globalKey) {
                left = m + 1;
            } else if (val > globalKey) {
                right = m - 1;
            } else {
                return globalLocalLookup[m].second;
            }
        }
        return -1;

        /*
        int loc;
        int left = 0;
        int right = globalLocalLookup.size() - 1;
        while (left <= right) {
            loc = (left + right) / 2;
            // sanity check loc here, although it cannot be wrong !!
            if ( loc < 0 || loc >= globalLocalLookup.size() ) {
                return -1;
            }
            if (globalLocalLookup[loc].first == globalKey) {
                return globalLocalLookup[loc].second;
            }
            if (globalLocalLookup[loc].first < globalKey)
                left = loc + 1;
            else
                right = loc - 1;
        }
        //std::cout << "fail" << std::endl;
        //exit(-1);
        return -1;

        */
    }

    void map(gType globalKey, lType localInd) {
        //
        globalLocalLookup[localInd].first = globalKey;
        globalLocalLookup[localInd].second = localInd;
        numMapped++;
    }

    void bulkGetLocal(ManVec<gType> &gKeys, ManVec<lType> &lKeys, int lKeyStart) {

        xvtxRangePush("bulkGetLocal");
        auto gKeyPtr  = gKeys.data();
        auto lKeyPtr  = lKeys.data();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i, GlobalLocalMapperBinSearch<gType, lType> &mapper) {
            auto targetGKey = gKeyPtr[i];
            ind_t localIndex = mapper.getLocalInd(targetGKey);
            assert(localIndex != -1);
            lKeyPtr[lKeyStart + i] = localIndex;
        }, gKeys.size(), *this);
        // manually check last error here to catch miss mappings ?
        Iterator::syncAll();
        xvtxRangePop();
    }

    void bulkCreate(ManVec<gKey_t> &gKeys) {
        //
        auto keyPtr = gKeys.data();
        auto mapPtr = globalLocalLookup.data();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i) {
            mapPtr[i].first = keyPtr[i];
            mapPtr[i].second = i;
        }, gKeys.size());

        Iterator::syncAll();
        numMapped += gKeys.size();
    }

    void finalize(){
        globalLocalMapperSTL::sort(GLOBALLOCALMAPPEREXEC globalLocalLookup.begin(), globalLocalLookup.end(),
            [] DEVCALLABLE (std::pair<gType, lType> a, std::pair<gType, lType> b) {
            return a.first < b.first;
        });
        Iterator::syncAll();
        // std::sort(globalLocalLookup.begin(), globalLocalLookup.end(), [] DEVCALLABLE (std::pair<gType, lType> a, std::pair<gType, lType> b){ return a.first < b.first;});
    }

    void clear() {
        auto mapPtr = globalLocalLookup.data();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i) {
            mapPtr[i].first = -1;
            mapPtr[i].second = -1;
        }, globalLocalLookup.size());

        Iterator::syncAll();
        numMapped = 0;
    }

    void setSize(size_t size){
        // option to set the size
        globalLocalLookup.resize(size);
        clear();
    }


    size_t numActive(){
        return numMapped;
    }
};

template <class gType, class lType>
//using GlobalLocalMapper = GlobalLocalMapperLinProbe<gType, lType>;
using GlobalLocalMapper = GlobalLocalMapperBinSearch<gType, lType>;
// using GlobalLocalMapper = GlobalLocalMapperHash<gType, lType>;

#endif //PHELL_GLOBALLOCALMAPPER_H
