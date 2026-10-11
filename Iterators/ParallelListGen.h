//
// Created by Fredrik Robertsen on 3/23/25.
//

#ifndef PARALLELLISTGEN_H
#define PARALLELLISTGEN_H

#include <ManVec.h>
#include "Iterator.h"
#include "ExecutionPolicy.h"

template<typename T>
ManVec<T> listGenSeq(int max) {
    ManVec<T> list;
    list.resize(max);
    auto ptr = list.data();
    Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int ind) {
        ptr[ind] = ind;
    }, max);
    Iterator::syncAll();
    return list;
}

template<typename T, typename Fun>
ManVec<T> listGenPredicate(int max, Fun fun) {
    ManVec<T> list = listGenSeq<T>(max);

    auto point = stx::stable_partition(EXECPOLICY list.begin(), list.end(), fun);
    list.resize(std::distance(list.begin(), point));

    return list;
}


#endif //PARALLELLISTGEN_H
