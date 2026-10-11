//
// Created by Fredrik Robertsen on 12/16/23.
//

#ifndef PHELL_UNIQUE_H
#define PHELL_UNIQUE_H

#include <unordered_set>
#include <algorithm>
#include <random>
#include <ParallelReduce.h>
#include <ParallelSelect.h>

#ifdef GPU

#include "DevPortabilityLayer.h"

#if (ENABLE_CUDA==1)
#include <cub/cub.cuh>
namespace cubX = cub;
#elif (ENABLE_HIP==1)
//#include <cub/cub.cuh>

#include <hipcub/hipcub.hpp>
namespace cubX = hipcub;
#endif
#endif


#ifdef GPU

template<int accelxGrid, int indPerThread, int numInCache>
class SharedUniq {
    typedef cubX::BlockRadixSort <pInd_t, accelxGrid, indPerThread> BlockRadixSort;
    typedef cubX::BlockDiscontinuity <pInd_t, accelxGrid> BlockDiscontinuity;
    typedef cubX::BlockScan<int, accelxGrid> BlockScan;

    typename BlockRadixSort::TempStorage storageRadix;

    typename BlockDiscontinuity::TempStorage storageDiscont;

    typename BlockScan::TempStorage storageScan;
public:
    __device__ bool get(pInd_t *indexesShrd, int &numCurrentlyCached, ManVec<int> &activeInBlock) {

        pInd_t index[indPerThread];

        // fill index from shared
        for (int i = 0; i < indPerThread; i += 1) {
            index[i] = indexesShrd[threadIdx.x + (i * blockDim.x)];
        }
        // sort the current indexes
        BlockRadixSort(storageRadix).Sort(index);

        // find the places that change
        int head_flags[indPerThread];

#if (ENABLE_CUDA==1)
        BlockDiscontinuity(storageDiscont).FlagHeads(head_flags, index, cuda::std::not_equal_to());
#elif (ENABLE_HIP==1)
        BlockDiscontinuity(storageDiscont).FlagHeads(head_flags, index, cubX::Inequality());
#endif



        int selectedIndex[indPerThread];

        // figure out the output indexes for these
        BlockScan(storageScan).ExclusiveSum(head_flags, selectedIndex);

        for (int i = 0; i < indPerThread; i++) {
            // Find the end, save it to numCurrentlyCached
            if (head_flags[i] == 1) {
                if (index[i] == INT32_MAX) {
                    // this is the count of how many were selected in the block
                    numCurrentlyCached = selectedIndex[i];
                }
            }
        }
        __syncthreads();
        // if the numCurrentlyCached is more than cachable, exit, this block is uncaclable
        if (numCurrentlyCached >= numInCache) {
            if (threadIdx.x == 0) {
                activeInBlock[blockIdx.x] = -1;
                //    printf("exceeded cache size %i %i %i \n", numCurrentlyCached, threadIdx.x, blockIdx.x);
            }
            // this needs to move out of here to the calling function
            return false;
        }

        for (int i = 0; i < indPerThread; i++) {
            // for the flagged ones store them into the shared space
            if (head_flags[i] == 1) {
                if (index[i] != INT32_MAX) {
                    indexesShrd[selectedIndex[i]] = index[i];
                }
            }
        }
        __syncthreads();
        // fill rest with INT32_MAX
        for (int i = numCurrentlyCached; i < indPerThread * accelxGrid; i += blockDim.x) {
            indexesShrd[i] = INT32_MAX;
        }
        __syncthreads();

        return true;
    }
};

#endif


#endif //PHELL_UNIQUE_H
