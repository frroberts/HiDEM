//
// Created by Fredrik Robertsen on 04/11/2020.
//

#ifndef PHELL_UTIL_H
#define PHELL_UTIL_H

#include <vector>
#include "../../Iterators/ManVec.h"
#include <algorithm>
#include <fstream>
#include "../../Iterators/ExecutionPolicy.h"
#include "../../Iterators/Iterator.h"
#include "ParallelSort.h"

// todo: optimize, should be possible to do in place ?
template< class T >
void reorderVec(std::vector<T> &v, std::vector<size_t> const &order )  {
    std::vector<T> ret;
    for(const auto& i: order)
    {
        ret.push_back(v[i]);
    }
    ManVec<T>::swap(v, ret);
}

template< class T, class Tind>
void reorderDevVec(ManVec<T> &v, ManVec<Tind> &order, OffloadExec policy = OffloadExec() )  {
    if(v.size() != order.size()){
        std::cout << "incorrect reorder " << std::endl;

        raise(SIGINT);
        exit(1);
    }
    ManVec<T> ret(v.size());

    T* v_ptr = v.data();
    T* ret_ptr = ret.data();
    Tind * order_ptr = order.data();

    Iterator::iterate(policy, [=] DEVCALLABLE (Tind ind) {
        ret_ptr[ind] = v_ptr[order_ptr[ind]];
    }, v.size());
    Iterator::syncAll();

    ManVec<T>::swap(v, ret);
}

template<typename Ind, typename FilterFun, typename ...Args>
ManVec<pInd_t> filterIndexVec(ManVec<Ind> &inputIndexes, FilterFun filter, Args& ...args) {
    ManVec<Ind> indexes;

    // parallel offload whatever this check
    for (int j = 0; j < inputIndexes.size(); ++j) {
        lInd_t ind = inputIndexes[j];
        if (filter(ind, args...))
            indexes.push_back(ind);
    }
    return indexes;
}

/*
template< class T >
void reorderDevVecWTemp(ManVec<T> &v, ManVec<T> &temp, ManVec<ind_t> &order, OffloadExec policy = ParallelExec() )  {

    T* v_ptr = v.data();
    T* ret_ptr = temp.data();
    ind_t * order_ptr = order.data();
    Iterator::iterate(policy, [=] DEVCALLABLE (size_t ind)
    {
        ret_ptr[ind] = v_ptr[order_ptr[ind]];
        }, v.size());
    Iterator::syncAll();

    ManVec<T>::swap(v, temp);

}

template< class T, int ptrCount>
void reorderDevVecBulk(std::array<T*, ptrCount> v_ptr, ManVec<ind_t> &order, OffloadExec policy = ParallelExec() )  {

    ind_t * order_ptr = order.data();
    Iterator::iterate(policy, [=] DEVCALLABLE (size_t ind)
    {
        for (int i = 0; i < ptrCount-1; i++)
        {
            v_ptr[i][ind] = v_ptr[i+1][order_ptr[ind]];
        }
        }, order.size());
    Iterator::syncAll();

    //ManVec<T>::swap(v_ptr, tmp_ptr);
        for (int i = ptrCount; i > 1; i--)
        {
            std::swap(v_ptr[i],v_ptr[i-1]);
            //v_ptr[i][ind] = v_ptr[i+1][order_ptr[ind]];
        }
}
*/
int getLocalRank();


#endif //PHELL_UTIL_H
