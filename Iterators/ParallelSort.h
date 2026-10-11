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
    void *temp = NULL;
    size_t tempBytes = 0;

    void allocateNeededTemp(size_t neededBytes) {
#ifdef GPU
        if(neededBytes > tempBytes)
        {
            if(temp != NULL) {
                devFree(temp);
            }
            tempBytes = neededBytes;
            devMalloc(&temp, tempBytes);
        }
#endif
    }

public:
    ParallelSort() {

    }

    ~ParallelSort() {
#ifdef GPU
        if(temp != NULL) {
            devFree(temp);
        }
#endif
    }


    template<class ValT, class KeyT>
    void sortIndirect(ManVec<ValT> &values, ManVec<KeyT> &keys) {
#if defined(GPU)

        ManVec<ValT> outValues;
        ManVec<KeyT> outKeys;
        outValues.resize(values.size());
        outKeys.resize(keys.size());

        size_t neededBytes;
        int count = values.size();
        cubX::DeviceRadixSort::SortPairs(NULL, neededBytes, keys.ptr, outKeys.ptr, values.ptr, outValues.ptr, count);

        allocateNeededTemp(neededBytes);

        cubX::DeviceRadixSort::SortPairs(temp, tempBytes, keys.ptr, outKeys.ptr, values.ptr, outValues.ptr, count);
        Iterator::syncAll();
        std::swap(values, outValues);
#else
        std::sort(EXECPOLICY values.begin(), values.end(), [&](KeyT a, KeyT b) { return keys[a] < keys[b]; });
#endif
    }

    template<class ValT>
    void sortDirect(ManVec<ValT> &values) {
        //
#if defined(GPU)
        ManVec<ValT> outValues;
        outValues.resize(values.size());

        size_t neededBytes;
        int count = values.size();
        cubX::DeviceRadixSort::SortKeys(NULL, neededBytes, values.ptr, outValues.ptr, count);

        allocateNeededTemp(neededBytes);


        cubX::DeviceRadixSort::SortKeys(temp, tempBytes, values.ptr, outValues.ptr, count);
        Iterator::syncAll();
        std::swap(values, outValues);
#else
        std::sort(EXECPOLICY values.begin(), values.end(), std::less<ValT>());
#endif
    }

    template<class ValT>
    void sortDirectDesc(ManVec<ValT> &values) {
        //
#if defined(GPU)
        ManVec<ValT> outValues;
        outValues.resize(values.size());

        size_t neededBytes;
        int count = values.size();
        cubX::DeviceRadixSort::SortKeysDescending(NULL, neededBytes, values.ptr, outValues.ptr, count);

        allocateNeededTemp(neededBytes);


        cubX::DeviceRadixSort::SortKeysDescending(temp, tempBytes, values.ptr, outValues.ptr, count);
        Iterator::syncAll();
        std::swap(values, outValues);
#else
        std::sort(EXECPOLICY values.begin(), values.end(), std::greater<ValT>());
#endif
    }


    template<class KeyT1>
    DEVCALLABLE
    bool keyCompare(size_t indA, size_t indB, KeyT1 &keys1){
        return keys1[indA] < keys1[indB];
    }

    template<class KeyT1, class... KeyT>
    DEVCALLABLE
    bool keyCompare(size_t indA, size_t indB, KeyT1 &keys1, KeyT& ...keys){
        //
        if (keys1[indA] < keys1[indB])
            return true;
        else if (keys1[indA] == keys1[indB]) {
            return keyCompare(indA, indB, keys...);
        } else return false;
    }

#if defined(GPU)

    template<class ValT, class KeyT>
    void segmentSort(ManVec<ValT> &valuesIn, ManVec<ValT> &valuesOut, KeyT &keysIn, KeyT & keysOut,
                                      ManVec<int> &offsets, ManVec<int> &sizes, int *numRuns ) {
/*
        for (int i = 0; i < *numRuns; ++i) {
            std::cout << sizes[i] << " " << offsets[i] << " " << offsets[i+1] << std::endl;
        }

        for (int i = 0; i < keysIn.size(); ++i) {
            std::cout << "i " << valuesIn[i] << " k " << keysIn[i] << std::endl;
        }
*/
        size_t neededBytes;
        cubX::DeviceSegmentedRadixSort::SortPairs(NULL, neededBytes, keysIn.ptr, keysOut.ptr, valuesIn.ptr,
                                                            valuesOut.ptr,
                                                            valuesIn.size(), (*numRuns), offsets.ptr,
                                                            offsets.ptr + 1);
        allocateNeededTemp(neededBytes);
        cubX::DeviceSegmentedRadixSort::SortPairs(temp, neededBytes, keysIn.ptr, keysOut.ptr, valuesIn.ptr,
                                                            valuesOut.ptr,
                                                            valuesIn.size(), (*numRuns), offsets.ptr,
                                                            offsets.ptr + 1);
        Iterator::syncAll();
/*
        for (int i = 0; i < keysIn.size(); ++i) {
            std::cout << "o " <<  valuesOut[i] <<  " k " << keysOut[i] << std::endl;
        }
        */
    }

    /*
    template<class ValT>
    void runLenEncode(ManVec<ValT> &valsIn, ManVec<int> &offsets, ManVec<int> &sizes, int *numRuns) {
        size_t neededBytes;
        ManVec<ValT> uniques;
        uniques.resize(valsIn.size());
        cubX::DeviceRunLengthEncode::Encode(NULL, neededBytes, valsIn.ptr, uniques.ptr, sizes.ptr,
                                            numRuns, valsIn.size());
// Allocate temporary storage
        allocateNeededTemp(neededBytes);

// Run encoding
        cubX::DeviceRunLengthEncode::Encode(temp, neededBytes, valsIn.ptr, uniques.ptr, sizes.ptr,
                                            numRuns, valsIn.size());
        Iterator::syncAll();

        std::exclusive_scan(EXECPOLICY sizes.begin(), sizes.end(), offsets.begin(), 0);
    }
     */



    template<class ValT, class KeyT>
    void sortIndirectSegmentedGPUImpl(ManVec<ValT> &valuesIn, ManVec<ValT> &valuesOut, KeyT & keysIn, KeyT &keysOut,
                                      ManVec<int> &offsets, ManVec<int> &sizes, int *numRuns) {
        segmentSort(valuesIn, valuesOut, keysIn, keysOut, offsets, sizes, numRuns);
        Iterator::syncAll();

        std::swap(valuesOut, valuesIn);

    }


    template<class ValT, class KeyT, class... KeysT>
    void sortIndirectNLevelGPURecurse(ManVec<ValT> &orderIn, ManVec<ValT> &orderOut,
                                      ManVec<int> &offsets, ManVec<int> &sizes, int *numRuns, KeyT & keyIn, KeysT &... keys) {
        // todo, clean up you slob
        reorderDevVec(keyIn, orderIn);
        Iterator::syncAll();

        ManVec<int> unique(orderIn.size());
        KeyT keyOut(orderIn.size());

        sortIndirectSegmentedGPUImpl(orderIn, orderOut, keyIn, keyOut,offsets, sizes, numRuns);

        Iterator::syncAll();

        size_t neededBytes;

        // Determine temporary device storage requirements
        cubX::DeviceRunLengthEncode::Encode(NULL, neededBytes, keyOut.ptr, unique.ptr, sizes.ptr,
                                           numRuns, orderIn.size());
// Allocate temporary storage
        allocateNeededTemp(neededBytes);
// Run encoding
        cubX::DeviceRunLengthEncode::Encode(temp, neededBytes, keyOut.ptr, unique.ptr, sizes.ptr,
                                           numRuns, orderIn.size());
        Iterator::syncAll();
// Declare, allocate, and initialize device-accessible pointers for input and output

// Determine temporary device storage requirements
        cubX::DeviceScan::ExclusiveSum(NULL, neededBytes, sizes.ptr, offsets.ptr, (*numRuns)+1);
// Allocate temporary storage
        allocateNeededTemp(neededBytes);
// Run exclusive prefix sum
        cubX::DeviceScan::ExclusiveSum(temp, neededBytes, sizes.ptr, offsets.ptr, (*numRuns)+1);
        Iterator::syncAll();

        sortIndirectNLevelGPURecurse(orderIn, orderOut, offsets, sizes, numRuns, keys...);

    }

    template<class ValT, class KeyT, class... KeysT>
    void sortIndirectNLevelGPURecurse(ManVec<ValT> &orderIn, ManVec<ValT> &orderOut,
                                      ManVec<int> &offsets, ManVec<int> &sizes, int *numRuns, KeyT &keyIn) {

        KeyT keyOut(orderIn.size());

        reorderDevVec(keyIn, orderIn);
        Iterator::syncAll();

        sortIndirectSegmentedGPUImpl(orderIn, orderOut,  keyIn, keyOut,offsets, sizes, numRuns);

    }

    template<class ValT,  class... KeysT>
    void sortIndirectNLevelGPUEntry(ManVec<ValT> &valuesIn, KeysT &... keys) {
        // std::cout << "GPU entry " << std::endl;
        ManVec<int> offsets(valuesIn.size()+1);
        ManVec<int> sizes(valuesIn.size());
        ManVec<int> orderIn(valuesIn.size());
        ManVec<int> orderOut(valuesIn.size());

        for (int i = 0; i < valuesIn.size(); ++i) {
            orderIn[i] = i;
        }

        int *numRuns;
        devMallocManaged((void**)&numRuns, sizeof(int) * 1);
        *numRuns = 1;
        sizes[0] = valuesIn.size();
        offsets[*numRuns] = valuesIn.size();
        offsets[0] = 0;
        sortIndirectNLevelGPURecurse(orderIn, orderOut, offsets, sizes, numRuns, keys...);

        reorderDevVec(valuesIn, orderIn);

        devFree(numRuns);
    }
#endif

    template<class ValT, class... KeyT>
    void sortIndirectNLevel(ManVec<ValT> &valuesIn, KeyT &... key) {
#if defined(GPU)
        sortIndirectNLevelGPUEntry(valuesIn, key...);
#else
        // should really create an index copy
        std::sort(EXECPOLICY valuesIn.begin(), valuesIn.end(), [&](size_t a, size_t b) {
            return keyCompare(a, b, key...);
        });
#endif
        Iterator::syncAll();
    }

    // this works in the real code as its called with indexes
    template<class ValT, class... KeyT>
    void sortIndirectNLevelCPU(ManVec<ValT> &valuesIn, KeyT &... key) {
        //
        std::sort(EXECPOLICY valuesIn.begin(), valuesIn.end(), [&](size_t a, size_t b) {
            return keyCompare(a, b, key...);
        });
    }

    template<class ValT, class SegT>
    void segmentedDescending(ManVec<ValT> &input, ManVec<ValT> &output, ManVec<SegT> &blockStarts, ManVec<SegT> &blockEnds){
#ifdef GPU
        //
        size_t neededBytes;
        output.resize(input.size());
        Iterator::syncAll();

        cubX::DeviceSegmentedRadixSort::SortKeysDescending(nullptr, neededBytes, input.ptr, output.ptr,
                                                input.size(), blockStarts.size(), blockStarts.ptr, blockEnds.ptr);

        allocateNeededTemp(neededBytes);
// Run sorting operation
        cubX::DeviceSegmentedRadixSort::SortKeysDescending(temp, tempBytes, input.ptr, output.ptr,
                                                input.size(), blockStarts.size(), blockStarts.ptr, blockEnds.ptr);
        Iterator::syncAll();
#else
        // todo parallelize outer loop instead ? might be more efficient
        output.resize(input.size());
        std::memcpy(output.ptr, input.ptr, sizeof(ValT)*input.size());
        //
        // segmented sort
        for (int i = 0; i < blockEnds.size(); ++i) {
            //
            std::sort(EXECPOLICY &output[blockStarts[i]], &output[blockEnds[i]], [](int a, int b) {
                return a > b;
            });
        }
#endif
    }

};

#endif //PHELL_PARALLELSORT_H
