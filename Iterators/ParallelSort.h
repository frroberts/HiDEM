//
// Created by Fredrik Robertsen on 31/07/2021.
//

#ifndef PHELL_PARALLELSORT_H
#define PHELL_PARALLELSORT_H

#include "Iterator.h"
#include "ManVec.h"
#include "ParallelScan.h"

#ifdef GPU
#include "DevPortabilityLayer.h"
#if (ENABLE_CUDA==1)
#include <cub/cub.cuh>
namespace cubX = cub;
#elif (ENABLE_HIP==1)
#include <hipcub/hipcub.hpp>
namespace cubX = hipcub;
#endif

#else
#endif


#include <algorithm>
#include <utility>


class ParallelSort {
public:
    ParallelSort() {

    }

    ~ParallelSort() {

    }

    template<class ValT, class KeyT>
    static void sortIndirect(ManVec<ValT> &values, ManVec<KeyT> &keys) {

        auto keyPtr = keys.data();
        stx::sort(EXECPOLICY values.begin(), values.end(), [=] DEVCALLABLE (KeyT a, KeyT b) { return keyPtr[a] < keyPtr[b]; });
    }

    template<class ValT>
    static void sortDirect(ManVec<ValT> &values) {

        stx::sort(EXECPOLICY values.begin(), values.end(), std::less<ValT>());
    }

    template<class ValT>
    static void sortDirectDesc(ManVec<ValT> &values) {

        stx::sort(EXECPOLICY values.begin(), values.end(), std::greater<ValT>());
    }


    template<class KeyT1>
    DEVCALLABLE
    static bool keyCompare(size_t indA, size_t indB, KeyT1 &keys1){
        return keys1[indA] < keys1[indB];
    }

    template<class KeyT1, class... KeyT>
    DEVCALLABLE
    static bool keyCompare(size_t indA, size_t indB, KeyT1 &keys1, KeyT& ...keys){
        //
        if (keys1[indA] < keys1[indB])
            return true;
        else if (keys1[indA] == keys1[indB]) {
            return keyCompare(indA, indB, keys...);
        } else return false;
    }

    template<class ValT, std::size_t... ints, class... KeyPtrT>
    DEVCALLABLE
    static bool keyCompareTup(ValT indA, ValT indB, std::tuple<KeyPtrT...> ptrs, std::integer_sequence<std::size_t, ints...>) {
        return (keyCompare(indA, indB, std::get<ints>(ptrs)...));
    }

    template<class ValT, class... KeyPtrT>
    static void sortIndirectNLevel(ManVec<ValT> &valuesIn, std::tuple<KeyPtrT...> ptrs) {
        auto dataTup = vecPtrToDataPtr(ptrs);
        stx::sort(EXECPOLICY valuesIn.begin(), valuesIn.end(), [dataTup] DEVCALLABLE  (ValT a, ValT b) {
            return keyCompareTup(a, b, dataTup, std::make_index_sequence<sizeof...(KeyPtrT)>());
        });
    }




};

#endif //PHELL_PARALLELSORT_H
