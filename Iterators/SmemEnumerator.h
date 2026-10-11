//
// Created by Fredrik Robertsen on 4/16/26.
//

#ifndef PHELL_SMEMENUMERATOR_H
#define PHELL_SMEMENUMERATOR_H
#include <limits>

#include "AtomicOps.h"


template<class kType, class vType, int size, kType emptyKey = std::numeric_limits<kType>::max(),
    vType emptyVal = std::numeric_limits<vType>::max()>
class SmemEnumerator {
public:
    kType keys[size];
    vType values[size];

    DEVCALLABLE
    size_t hashFun32(kType k) {
        k ^= k >> 16;
        k *= 0x85ebca6b;
        k ^= k >> 13;
        k *= 0xc2b2ae35;
        k ^= k >> 16;

        return (k);
    }

    public:
    int numMapped;


    __device__
    void clear() {
        // for loop needs to be threablock based, and sync
        // assuming 1d block
        for (int i = threadIdx.x; i < size; i += blockDim.x) {
            keys[i] = emptyKey;
            values[i] = emptyVal;
        }
        __syncthreads();
    }

    __device__
    vType getValue(kType key) {
        if (key == emptyKey) {
            return emptyVal;
        }
        int pos = 0;
        pos = hashFun32(key) % size;

        int searched = 0;
        while (searched != size) {
            if (keys[pos] == emptyKey) {
                return emptyVal;
            }

            if (keys[pos] == key) {
                // printf("%i \n", searched);
                // fix this to make it spin if val is empty val
                vType value = values[pos];
                return value;
            }
            pos = (pos + 1) % size;
            searched++;
        }
        return emptyVal;
    }

    __device__
    void mapValue(kType key) {
        uint32_t pos = 0;
        pos = hashFun32(key) % size;
        int searched = 0;
        do {
            // can double check if the current pos is empty ? to avoid the atomic op
            auto prevKey = Atomic::atomicCASVal32(keys[pos], emptyKey, key);

            if (prevKey == key)
                return;
            if (prevKey == emptyKey) {
                values[pos] = Atomic::atomicIncVal(numMapped);
                return;
            }
            pos = (pos + 1) % size;
            searched++;
        } while (searched != size);
    }

    __device__
    void storeCompressedKeys(kType *output) {
        for (int i = threadIdx.x; i < size; i += blockDim.x) {
            int key = keys[i];
            int val = values[i];
            if (key != emptyKey && val != emptyVal) {
                output[val] = key;
            }
        }
    }

};

#endif //PHELL_SMEMENUMERATOR_H
