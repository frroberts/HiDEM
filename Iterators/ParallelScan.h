//
// Created by Fredrik Robertsen on 31/07/2021.
//

#ifndef PHELL_PARALLELSCAN_H
#define PHELL_PARALLELSCAN_H

#include "ManVec.h"
#include <numeric>
#include <ExecutionPolicy.h>

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

public:
    ParallelScan() {
        //
    }
    ~ParallelScan(){

    }

    template<class T>
    static void exclusive(ManVec<T> &input, ManVec<T> &output)
    {
        stx::exclusive_scan(EXECPOLICY input.begin(), input.end(), output.begin(), 0);
    }
};

#endif //PHELL_PARALLELSCAN_H
