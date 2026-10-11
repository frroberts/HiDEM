//
// Created by Fredrik Robertsen on 04/11/2020.
//

#ifndef PHELL_UTIL_H
#define PHELL_UTIL_H

#include <vector>
#include "../../Iterators/ManVec.h"
#include <algorithm>
#include <fstream>
#include <regex>

#include "../../Iterators/ExecutionPolicy.h"
#include "../../Iterators/Iterator.h"
#include "ParallelSort.h"
#include <TupleHelpers.h>

template< class T, class Tind>
void reorderDevVec(ManVec<T> &v, ManVec<Tind> &order, OffloadExec policy = OffloadExec() )  {
    xvtxRangePush("reorder dev vec");

    if(v.size() != order.size()){
        std::cout << "incorrect reorder " << std::endl;

        raise(SIGINT);
        exit(1);
    }
    ManVec<T, ScratchAllocator<T>> temp;
    temp.resize(v.size(), false);
/*
    ManVec<T> ret;
    ret.resize(v.size(), false);
*/
    T* v_ptr = v.data();
    T* ret_ptr = temp.data();
    Tind * order_ptr = order.data();

    Iterator::iterate(policy, [=] DEVCALLABLE (Tind ind) {
        ret_ptr[ind] = v_ptr[order_ptr[ind]];
    }, v.size());
    Iterator::syncAll();

    // std::swap(v, ret);
    Iterator::copy(v.data(), ret_ptr, v.size()*sizeof(T));
        xvtxRangePop();

}

template< class T, class Tind, class TScratch>
void reorderDevVecScratch(ManVec<T> &v, ManVec<Tind> &order, ManVec<TScratch, ScratchAllocator<TScratch>> &scratch, OffloadExec policy = OffloadExec() )  {
    xvtxRangePush("reorder dev vec scratch");

    // ability to reuse the same scratch buffer as temp storage
    if(v.size() != order.size()){
        std::cout << "incorrect reorder " << std::endl;

        raise(SIGINT);
        exit(1);
    }
    size_t sizeInScratch = v.size()*(static_cast<float>(sizeof(T))/static_cast<float>(sizeof(TScratch)));
    scratch.resize(sizeInScratch, false);

    T* v_ptr = v.data();
    T* ret_ptr = reinterpret_cast<T*>(scratch.data());
    Tind * order_ptr = order.data();

    Iterator::iterate(policy, [=] DEVCALLABLE (Tind ind) {
        ret_ptr[ind] = v_ptr[order_ptr[ind]];
    }, v.size());
    Iterator::syncAll();

    Iterator::copy(v.data(), ret_ptr, v.size()*sizeof(T));
    xvtxRangePop();

}



template< class TupleT, class Tind>
void reorderDevVecTup(TupleT &inputPteTup, ManVec<Tind> &order, int numVals, OffloadExec policy = OffloadExec() ) {

    auto vecPtrs = inputPteTup;

    callForTupleElemRef(vecPtrs, [&] <typename T>(ManVec<T> *&vecPtr) {
        vecPtr = new ManVec<T>();
        vecPtr->resize(numVals, false);
    });

    auto inputPtr = vecPtrToDataPtr(inputPteTup);
    auto outPtr = vecPtrToDataPtr(vecPtrs);

    auto reorderPtr = order.data();
    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int ind) {
        int reorderInd = reorderPtr[ind];
        loadStorePtrTuple(ind, outPtr, reorderInd, inputPtr);
    }, numVals);

    callForTuple2Elem(inputPteTup, vecPtrs, [] <typename T>(ManVec<T> *&aPtr, ManVec<T> *&bPtr) {
        std::swap(*aPtr, *bPtr);
    });

    callForTupleElemRef(vecPtrs, [&] <typename T>(ManVec<T> *&vecPtr) {
        delete vecPtr;
    });
}

/*
// not used
template<typename Elems, class Tind>
void partitionVecPack(Elems elems, ManVec<Tind> &indexes, int prevSize, OffloadExec policy = OffloadExec()) {
    ManVec<char> flags;
    flags.resize(prevSize);
    auto flagPTr = flags.data();

    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i) {
        flagPTr[i] = 1;
    }, prevSize);

    auto nonOwnedIndexesPtr = indexes.data();

    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(int i) {
        flagPTr[nonOwnedIndexesPtr[i]] = 0;
    }, indexes.size());

    // Determine temporary device storage requirements
    void *d_temp_storage = nullptr;
    std::size_t temp_storage_bytes = 0;
    int *d_num_selected_out = nullptr;
    cubX::DevicePartition::Flagged(
        d_temp_storage, temp_storage_bytes,
        std::get<0>(elems)->data(), flags.data(), std::get<0>(elems)->data(), d_num_selected_out, prevSize);

    // Allocate temporary storage
    getErrorDevApi(devMalloc((void**)&d_temp_storage, temp_storage_bytes));
    getErrorDevApi(devMalloc((void**)&d_num_selected_out, sizeof (int)));
    // Run selection


    callForTupleElem(elems, [&]<typename T>(T *vecPTr) {
        T ret;
        ret.resize(vecPTr->size(), false);

        cubX::DevicePartition::Flagged(
            d_temp_storage, temp_storage_bytes,
            vecPTr->data(), flags.data(), ret.data(), d_num_selected_out, vecPTr->size());
        Iterator::syncAll();
        std::swap(*vecPTr, ret);
        //reorderDevVec(*vecPTr, reorderOrder);
    });


    getErrorDevApi(devFree(d_temp_storage));
    getErrorDevApi(devFree(d_num_selected_out));
}
*/

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

template<typename T>
void splitString(const std::string &input, const std::regex &splitExpr, T fun) {
    auto begin = std::sregex_iterator(input.begin(), input.end(), splitExpr);
    auto end = std::sregex_iterator();

    for (std::sregex_iterator i = begin; i != end; ++i) {
        std::string str = i->str();
        fun(str);
    }
}

template<typename T>
void splitStringDelims(const std::string &input, std::string delims, T fun) {
    std::size_t prev = 0, pos;
    while ((pos = input.find_first_of(delims, prev)) != std::string::npos) {
        if (pos > prev)
            fun(input.substr(prev, pos - prev));
        prev = pos + 1;
    }
    if (prev < input.length())
        fun(input.substr(prev, std::string::npos));
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

template <typename T>
DEVCALLABLE
T clamp(T val, T lo, T hi) {
    if (val < lo)
        return lo;
    if (val > hi)
        return hi;
    return val;
}


#endif //PHELL_UTIL_H
