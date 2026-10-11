//
// Created by Fredrik Robertsen on 7/26/23.
//

#ifndef PHELL_OWNEDLISTGEN_H
#define PHELL_OWNEDLISTGEN_H

#include <ManVec.h>
#include <Iterator.h>
#include "Util.h"
#include "../Global.h"

#ifdef GPU
#include <thrust/partition.h>
#include <thrust/execution_policy.h>
#define OWNEDLISTGENEXEC thrust::device,
namespace ownedListGenSTL = thrust;

#else
// do ptstl here for host with pstl
#define OWNEDLISTGENEXEC
namespace ownedListGenSTL = std;
#endif

template <typename T>
void generateOwnedNonOwned(ManVec<int> &owners, ManVec<T> &ownedIndexes, ManVec<T> &nonOwnedIndexes, size_t count) {
    ManVec<pInd_t> allIndexes(count);
    auto allIndexesPtr = allIndexes.data();
    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(ind_t i) {
        allIndexesPtr[i] = i;
    }, count);
    Iterator::syncAll();

    int self = getLocalRank();
    auto ownerPtr = owners.data();
    ownedListGenSTL::stable_partition(OWNEDLISTGENEXEC allIndexes.begin(), allIndexes.end(), [=] DEVCALLABLE (pInd_t pInd) {
        return ownerPtr[pInd] == self;
    });
    auto partPoint = ownedListGenSTL::partition_point(OWNEDLISTGENEXEC allIndexes.begin(), allIndexes.end(), [=] DEVCALLABLE (pInd_t pInd) {
        return ownerPtr[pInd] == self;
    });

    size_t numOwned = std::distance(allIndexes.begin(), partPoint);
    size_t numNonOwned = count - numOwned;

    ownedIndexes.resize(numOwned);
    nonOwnedIndexes.resize(numNonOwned);
    Iterator::syncAll();

    auto ownedIndexesPtr = ownedIndexes.data();
    auto nonOwnedIndexesPtr = nonOwnedIndexes.data();
    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(ind_t i) {
        ownedIndexesPtr[i] = allIndexesPtr[i];
    }, numOwned);

    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(ind_t i) {
        nonOwnedIndexesPtr[i] = allIndexesPtr[numOwned + i];
    }, numNonOwned);
    Iterator::syncAll();
}
#endif //PHELL_OWNEDLISTGEN_H
