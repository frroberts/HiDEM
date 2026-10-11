//
// Created by Fredrik Robertsen on 11/30/24.
//

#ifndef PHELL_BULKRANDOM_H
#define PHELL_BULKRANDOM_H

#include "ManVec.h"
#include <random>
#include "../PLiB/LoggerV2/RangeCollection.h"

#if ENABLE_CUDA
#include <curand.h>
#elif ENABLE_HIP
#include <hiprand/hiprand.hpp>
#endif

template <typename T>
class BulkRandom{
    //
public:

    int count;
    ManVec<T> values;
    int regenCount = 0;

#if ENABLE_CUDA
    curandGenerator_t gen;

#elif ENABLE_HIP
        hiprandGenerator_t gen;
#else
#endif

    BulkRandom() {
#if ENABLE_CUDA
        curandCreateGenerator(&gen, CURAND_RNG_PSEUDO_DEFAULT);
        curandSetPseudoRandomGeneratorSeed(gen,regenCount);
#elif ENABLE_HIP
        hiprandCreateGenerator(&gen, HIPRAND_RNG_PSEUDO_DEFAULT);
        hiprandSetPseudoRandomGeneratorSeed(gen,regenCount);
#else
#endif
    }

    ~BulkRandom() {
     #if ENABLE_CUDA
        curandDestroyGenerator(gen);
#elif ENABLE_HIP
        hiprandDestroyGenerator(gen);
#else
#endif
    }

    void setSize(int newCount){
        //
        count = newCount;
        values.resize(count);
    }

    void reGenerate();
    ManVec<T> &getVec(){
        return values;
    }


};

template <typename T>
void BulkRandom<T>::reGenerate(){
    RangeV2 r("random regen", 3);
    regenCount++;

#if ENABLE_CUDA
    curandGenerateUniform(gen, values.data(), count);

    cudaDeviceSynchronize();
#elif ENABLE_HIP
    hiprandGenerateUniform(gen, values.data(), count);

    hipDeviceSynchronize();
#else

#pragma omp parallel
    {
        // never seed with 0
        std::mt19937 gen(2+regenCount);
        std::uniform_real_distribution<double> dist(0.0, 1.0);

#pragma omp for
        for (int i = 0; i < count; ++i) {
            values[i] = dist(gen);
        }
    }
#endif
}

#endif //PHELL_BULKRANDOM_H
