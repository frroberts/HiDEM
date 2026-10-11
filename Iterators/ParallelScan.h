//
// Created by Fredrik Robertsen on 31/07/2021.
//

#ifndef PHELL_PARALLELSCAN_H
#define PHELL_PARALLELSCAN_H

#include "ManVec.h"
#include <numeric>

#ifdef ENABLE_PSTL
#include <execution>
// not the greatest hack but apparently even serial policies require tbb
#define EXECPOLICY std::execution::par,
#else
#define EXECPOLICY
#endif

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

class ParallelScan
{
    void     *temp = NULL;
    size_t   tempBytes = 0;
public:
    ParallelScan() {
        //
    }
    ~ParallelScan(){
#ifdef GPU
        if(temp != NULL)
        {
            devFree(temp);
        }
#endif
    }

    template<class T>
    void exclusive(ManVec<T> &input, ManVec<T> &output)
    {
        //std::assert(input.size() == output.size());

#ifdef GPU
        // Determine temporary device storage requirements
        size_t   neededBytes = 0;
        cubX::DeviceScan::ExclusiveSum(NULL, neededBytes, input.ptr, output.ptr, input.size());
        if(neededBytes > tempBytes)
        {
            if(temp != NULL) {
                devFree(temp);
            }
            tempBytes = neededBytes;
            devMalloc(&temp, tempBytes);
        }
        // Allocate temporary storage for exclusive prefix scan
        // Run exclusive prefix min-scan
        cubX::DeviceScan::ExclusiveSum(temp, tempBytes, input.ptr, output.ptr, input.size());
        devDeviceSynchronize();
#else
        std::exclusive_scan(EXECPOLICY input.begin(), input.end(), output.begin(), 0);

        /*
        int x = 0;
#pragma omp parallel for reduction(inscan, + : x)
        for (int i = 0; i < input.size(); i++) {
            output[i] = x;
#pragma omp scan exclusive(x)
            x += input[i];
        }
         */
#endif
    }
};

#endif //PHELL_PARALLELSCAN_H
