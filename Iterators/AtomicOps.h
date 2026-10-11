//
// Created by Fredrik Robertsen on 5/15/24.
//

#ifndef PHELL_ATOMICOPS_H
#define PHELL_ATOMICOPS_H

#include "DevCall.h"

namespace Atomic {
    template<typename T>
    DEVCALLABLE
    static void atomicAddVal(T &target, const T val) {
#ifdef __CUDA_ARCH__
        atomicAdd(&target, val);
#elif __HIP_DEVICE_COMPILE__
        atomicAdd(&target, val);
#else
#pragma omp atomic
        target += val;
#endif
    }

    template<typename T>
    DEVCALLABLE
    static T atomicIncVal(T &target) {
#ifdef __CUDA_ARCH__
        return atomicAdd(&target, 1);
#elif __HIP_DEVICE_COMPILE__
        return atomicAdd(&target, 1);
#else
        T old;
#pragma omp atomic capture
        old = target++;
        return old;
#endif
    }

    template<typename T>
    DEVCALLABLE
    static T atomicCASVal64(T &target, T comp, T val) {
#ifdef __CUDA_ARCH__
            return atomicCAS((unsigned long long int *)&target, (unsigned long long int )comp, (unsigned long long int )val);
#elif __HIP_DEVICE_COMPILE__
            return atomicCAS((unsigned long long int *)&target, (unsigned long long int )comp, (unsigned long long int )val);
#else
        // todo replace with atomic compare capture
        T currentVal;
#pragma omp critical
        {
            currentVal = target;
            if (currentVal == comp) {
                target = val;
            }
        }
        return currentVal;
#endif
    }

    template<typename T>
DEVCALLABLE
static T atomicCASVal32(T &target, T comp, T val) {
#ifdef __CUDA_ARCH__
        return atomicCAS((unsigned int *)&target, (unsigned int )comp, (unsigned int )val);
#elif __HIP_DEVICE_COMPILE__
        return atomicCAS((unsigned int *)&target, (unsigned int )comp, (unsigned int )val);
#else
        // todo replace with atomic compare capture
        T currentVal;
#pragma omp critical
        {
            currentVal = target;
            if (currentVal == comp) {
                target = val;
            }
        }
        return currentVal;
#endif
    }
}
#endif //PHELL_ATOMICOPS_H
