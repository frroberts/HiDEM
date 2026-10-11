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
}
#endif //PHELL_ATOMICOPS_H
