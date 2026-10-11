//
// Created by Fredrik Robertsen on 7/26/23.
//

#ifndef PHELL_DROPREORDER_H
#define PHELL_DROPREORDER_H

#include <ManVec.h>

#include <ExecutionPolicy.h>
#include <Iterator.h>

#ifdef GPU
#include "DevPortabilityLayer.h"
#if (ENABLE_CUDA==1)
#include <cub/cub.cuh>
namespace cubX = cub;
#elif (ENABLE_HIP==1)
// todo this should really not be needed ...
#include "/opt/rocm/include/hipcub/backend/rocprim/device/device_partition.hpp"
#include <hipcub/hipcub.hpp>
namespace cubX = hipcub;
#endif

#else
#endif

#include "ParallelSort.h"

template <typename T>
void getDropReorder(ManVec<T> &droppedLocalIndexes, ManVec<T> &newOrder, size_t count) {
    xvtxRangePush("gen drop reorder");

#ifdef GPU
    ManVec<char, ScratchAllocator<char>> flags;
    flags.resize(count, false);

    auto flagPTr = flags.data();
    ManVec<T, ScratchAllocator<T>> newOrderIn;
    newOrder.resize(count, false);
    newOrderIn.resize(count, false);
    T *newOrderPtr = newOrder.data();
    T *newOrderInPtr = newOrderIn.data();
    T *droppedLocalIndexesPtr = droppedLocalIndexes.data();

    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (T i){
        newOrderPtr[i] = i;
        newOrderInPtr[i] = i;
        flagPTr[i] = 1;
        }, count);

    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(T i) {
        flagPTr[droppedLocalIndexesPtr[i]] = 0;
    }, droppedLocalIndexes.size());

    // Determine temporary device storage requirements
    void *d_temp_storage = nullptr;
    std::size_t temp_storage_bytes = 0;
    int *d_num_selected_out = nullptr;
    cubX::DevicePartition::Flagged(
            d_temp_storage, temp_storage_bytes,
            newOrderIn.data(), flags.data(), newOrder.data(), d_num_selected_out, count);

    // Allocate temporary storage
    getErrorDevApi( devMalloc((void**)&d_temp_storage, temp_storage_bytes) );
    getErrorDevApi( devMalloc((void**)&d_num_selected_out, sizeof (int)) );
    // Run selection
    cubX::DevicePartition::Flagged(
            d_temp_storage, temp_storage_bytes,
            newOrderIn.data(), flags.data(), newOrder.data(), d_num_selected_out, count);
    Iterator::syncAll();

    getErrorDevApi( devFree(d_temp_storage) );
    getErrorDevApi( devFree(d_num_selected_out) );
#else

    ParallelSort sorter;

    sorter.sortDirectDesc(droppedLocalIndexes);

    //std::sort(droppedLocalIndexes.begin(), droppedLocalIndexes.end(), std::greater<T>());
    Iterator::syncAll();


    newOrder.resize(count);
    T *newOrderPtr = newOrder.data();
    T *droppedLocalIndexesPtr = droppedLocalIndexes.data();

    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (T i){
        newOrderPtr[i] = i;
        }, count);
    Iterator::syncAll();

    int last = count;
    for (T indx : droppedLocalIndexes) {
        last--;
        //
        std::swap(newOrder[indx] ,newOrder[last]);
    }
#endif
        xvtxRangePop();

}



#endif //PHELL_DROPREORDER_H
