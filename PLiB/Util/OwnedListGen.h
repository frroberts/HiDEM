//
// Created by Fredrik Robertsen on 7/26/23.
//

#ifndef PHELL_OWNEDLISTGEN_H
#define PHELL_OWNEDLISTGEN_H

#include <ManVec.h>
#include <Iterator.h>
#include "Util.h"
#include "../Global.h"

#include <ExecutionPolicy.h>

template <typename T>
void generateOwnedNonOwned(ManVec<int> &owners, ManVec<T> &ownedIndexes, ManVec<T> &nonOwnedIndexes, size_t count) {
    ManVec<pInd_t> allIndexes;
    allIndexes.resize(count);

    auto allIndexesPtr = allIndexes.data();
    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(ind_t i) {
        allIndexesPtr[i] = i;
    }, count);
    Iterator::syncAll();

    int self = getLocalRank();
    auto ownerPtr = owners.data();
    stx::stable_partition(EXECPOLICY allIndexes.begin(), allIndexes.end(), [=] DEVCALLABLE (pInd_t pInd) {
        return ownerPtr[pInd] == self;
    });
    auto partPoint = stx::partition_point(EXECPOLICY allIndexes.begin(), allIndexes.end(), [=] DEVCALLABLE (pInd_t pInd) {
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
