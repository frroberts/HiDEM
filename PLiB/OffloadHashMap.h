//
// Created by Fredrik Robertsen on 31/10/2020.
//

#ifndef PHELL_OFFLOADHASHMAP_H
#define PHELL_OFFLOADHASHMAP_H

#include <chrono>

#include "Global.h"
#include "ManVec.h"
#include "Iterator.h"
#include "AtomicOps.h"

template<class kType, class vType, kType emptyKey = std::numeric_limits<kType>::max(),
    vType emptyVal = std::numeric_limits<vType>::max()>
class GlobalLocalMapperBinSearch : public ManagedParent{
public:

    ManVec<std::pair<kType, vType>> globalLocalLookup;

    int numMapped = 0;

    DEVCALLABLE
    vType getValue(kType globalKey) {
        // binary search for global key, return index it was found at

        int left = 0;
        int right = globalLocalLookup.size() - 1;
        while (left <= right) {
            int m = (left + right) / 2;
            kType val = globalLocalLookup[m].first;
            if (val < globalKey) {
                left = m + 1;
            } else if (val > globalKey) {
                right = m - 1;
            } else {
                return globalLocalLookup[m].second;
            }
        }
        return emptyVal;
    }

    void map(kType globalKey, vType localInd) {
        //
        globalLocalLookup[localInd].first = globalKey;
        globalLocalLookup[localInd].second = localInd;
        numMapped++;
    }

    void bulkGetValue(ManVec<kType> &gKeys, ManVec<vType> &lKeys, int lKeyStart) {

        xvtxRangePush("bulkGetLocal");
        auto gKeyPtr  = gKeys.data();
        auto lKeyPtr  = lKeys.data();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i, GlobalLocalMapperBinSearch<kType, vType> &mapper) {
            auto targetGKey = gKeyPtr[i];
            ind_t localIndex = mapper.getValue(targetGKey);
            // assert(localIndex != emptyVal);
            lKeyPtr[lKeyStart + i] = localIndex;
        }, gKeys.size(), *this);
        // manually check last error here to catch miss mappings ?
        Iterator::syncAll();
        xvtxRangePop();
    }

    void bulkCreate(ManVec<kType> &gKeys) {
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
        stx::sort( EXECPOLICY globalLocalLookup.begin(), globalLocalLookup.end(),
            [] DEVCALLABLE (std::pair<kType, vType> a, std::pair<kType, vType> b) {
            return a.first < b.first;
        });
        Iterator::syncAll();
    }

    void clear() {
        auto mapPtr = globalLocalLookup.data();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i) {
            mapPtr[i].first = emptyKey;
            mapPtr[i].second = emptyVal;
        }, globalLocalLookup.size());

        Iterator::syncAll();
        numMapped = 0;
    }

    void setSize(size_t size){
        // option to set the size
        globalLocalLookup.resize(size);
        clear();
    }

    DEVCALLABLE
    vType notFound() {
        return emptyVal;
    }

    size_t numActive(){
        return numMapped;
    }
};

template<class kType, class vType, kType emptyKey = std::numeric_limits<kType>::max(),
    vType emptyVal = std::numeric_limits<vType>::max()>
class OffloadHashMapReal : public ManagedParent {

    ManVec<std::pair<kType, vType>> keyVals;

    size_t numMapped = 0;
    float overSubscription = 2;
    size_t realSize = 0;

    DEVCALLABLE
    size_t hashFun32(kType k) {
        k ^= k >> 16;
        k *= 0x85ebca6b;
        k ^= k >> 13;
        k *= 0xc2b2ae35;
        k ^= k >> 16;

        return (k);
    }

    DEVCALLABLE
    size_t hashFun64(kType k)
    {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccd;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53;
        k ^= k >> 33;

        return k;
    }

public:


    DEVCALLABLE
    vType getValue(kType key) {
        if (key == emptyKey) {
            return emptyVal;
        }
        size_t pos = 0;
        if constexpr (sizeof(kType) == 4)
            pos = hashFun32(key) % realSize;
        else
            pos = hashFun64(key) % realSize;

        size_t searched = 0;
        while (searched != realSize) {
            if (keyVals[pos].first == emptyKey) {
                return emptyVal;
            }

            if (keyVals[pos].first == key) {
                // printf("%i \n", searched);
                // fix this to make it spin if val is empty val
                vType value = keyVals[pos].second;
                return value;
            }
            pos = (pos + 1) % realSize;
            searched++;
        }
        return emptyVal;
    }

    DEVCALLABLE
    void mapImpl(kType key, vType value) {
        size_t pos = 0;
        if constexpr (sizeof(kType) == 4)
            pos = hashFun32(key) % realSize;
        else
            pos = hashFun64(key) % realSize;
        size_t searched = 0;
        kType currentKey;
        do {
            // can double check if the current pos is empty ? to avoid the atomic op
            if constexpr (sizeof(kType) == 4)
                currentKey = Atomic::atomicCASVal32(keyVals[pos].first,  emptyKey, key);
            else
                currentKey = Atomic::atomicCASVal64(keyVals[pos].first,  emptyKey, key);

            if (currentKey == emptyKey || currentKey == key) {
                keyVals[pos].second = value;
                // printf("%i \n", searched);
                // atomic add to numMapped ?
                return;
            }
            pos = (pos + 1) % realSize;
            searched++;
        } while (searched != realSize);
    }

    void bulkCreate(ManVec<kType> &gKeys) {
        //
        auto gKeysPtr = gKeys.data();
        Iterator::iterate(OffloadExec(), [=]DEVLAMBDA(size_t ind, OffloadHashMapReal &mapper){
            mapper.mapImpl(gKeysPtr[ind], ind);
        }, gKeys.size(), *this);
        numMapped += gKeys.size();
        Iterator::syncAll();


    }


    void bulkGetValue(ManVec<kType> &gKeys, ManVec<vType> &lKeys, int valueStartPos) {

        xvtxRangePush("bulkGetLocal");
        auto gKeyPtr  = gKeys.data();
        auto lKeyPtr  = lKeys.data();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i, OffloadHashMapReal<kType, vType> &mapper) {
            auto targetGKey = gKeyPtr[i];
            vType value = mapper.getValue(targetGKey);
            // assert(value != emptyVal);
            lKeyPtr[valueStartPos + i] = value;
        }, gKeys.size(), *this);
        // manually check last error here to catch miss mappings ?
        Iterator::syncAll();
        xvtxRangePop();
    }


    void finalize(){
        // option to optimize or even just build the table
    }

    void clear() {
        auto keyValsPtr = keyVals.data();
        Iterator::iterate(OffloadExec(), [=]DEVLAMBDA(int ind){
            keyValsPtr[ind] = {emptyKey, emptyVal};
        }, keyVals.size());
        Iterator::syncAll();
        numMapped = 0;
    }

    void setSize(size_t size){
        // option to set the size
        realSize = size*overSubscription;
        keyVals.resize(realSize);
        clear();
    }

    // num active does not account for remapping stuff
    size_t numActive(){
        return numMapped;
    }

    DEVCALLABLE
    vType notFound() {
        return emptyVal;
    }
};


template <class gType, class lType>
using OffloadHashMap = OffloadHashMapReal<gType, lType>;


#endif //PHELL_OFFLOADHASHMAP_H
