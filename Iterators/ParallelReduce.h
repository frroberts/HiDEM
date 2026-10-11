//
// Created by Fredrik Robertsen on 22/03/2021.
//

#ifndef PHELL_PARALLELREDUCE_H
#define PHELL_PARALLELREDUCE_H

#include <limits>
#include <ManVec.h>

#include <cstdint>
#include <numeric>

#include <ExecutionPolicy.h>

#include "ThrustHelper.h"

template<class T>
class ParallelReduce {

public:
    static T sum(ManVec<T> &in) {
        return stx::reduce(EXECPOLICY in.begin(), in.end(), 0);
    }

    static T max(ManVec<T> &in) {
        return stx::reduce(EXECPOLICY in.begin(), in.end(), std::numeric_limits<T>::lowest(),
                           []DEVCALLABLE(T a, T b) {
                               return std::max(a, b);
                           });
    }

    template<class Tind>
    static T maxIndirect(ManVec<T> &in, ManVec<Tind> &indexes) {
        T *ptr = in.data();
        auto indexPtr = indexes.data();
        return transformReduce(indexes.begin(), indexes.end(), std::numeric_limits<T>::lowest(),
                               []DEVCALLABLE(T a, T b) {
                                   return std::max(a, b);
                               },
                               [=] DEVCALLABLE (int i) {
                                   return ptr[i];
                               });
    }

    static T min(ManVec<T> &in) {
        return stx::reduce(EXECPOLICY in.begin(), in.end(), std::numeric_limits<T>::max(),
                           []DEVCALLABLE(T a, T b) {
                               return std::min(a, b);
                           });
    }

    template<class Tind>
    static T minIndirect(ManVec<T> &in, ManVec<Tind> &indexes) {
        T *ptr = in.data();
        auto indexPtr = indexes.data();
        return transformReduce(indexes.begin(), indexes.end(), std::numeric_limits<T>::max(),
                               []DEVCALLABLE(T a, T b) {
                                   return std::min(a, b);
                               },
                               [=] DEVCALLABLE (int i) {
                                   return ptr[i];
                               });
    }

    static void minMax(T &minVal, T &maxVal, ManVec<T> &data) {
        minVal = min(data);
        maxVal = max(data);
    }

    template<class Tind>
    static void minMaxInderect(T &minVal, T &maxVal, ManVec<T> &data, ManVec<Tind> &indexes) {
        minVal = minIndirect(data, indexes);
        maxVal = maxIndirect(data, indexes);
    }

    template<class ForwardIt, class BinaryOp, class UnaryOp>
    static T transformReduce(ForwardIt first, ForwardIt last, T init,
                      BinaryOp reduce, UnaryOp transform) {
#ifdef GPU
        return thrust::transform_reduce(EXECPOLICY first, last, transform, init, reduce);
#else
        return std::transform_reduce(EXECPOLICY first, last, init, reduce,transform);
#endif
    }

};


#endif //PHELL_PARALLELREDUCE_H
