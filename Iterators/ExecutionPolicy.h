//
// Created by Fredrik Robertsen on 18/06/2021.
//

#ifndef PHELL_EXECUTIONPOLICY_H
#define PHELL_EXECUTIONPOLICY_H

#include "DevPortabilityLayer.h"


#ifdef GPU
#include <thrust/partition.h>
#define EXECPOLICY thrust::device,
namespace stx = thrust;
#else
namespace stx = std;
#ifdef ENABLE_PSTL
#include <execution>
// not the greatest hack but apparently even serial policies require tbb
#define EXECPOLICY std::execution::par_unseq,
#else
#define EXECPOLICY
#endif
#endif


#ifdef ENABLE_PSTL
#include <execution>
// not the greatest hack but apparently even serial policies require tbb
#define STDEXECPOLICY std::execution::par_unseq,
#else
#define STDEXECPOLICY
#endif


class ExecutionPolicy{
public:
    enum ExecType{
    Serial,
    Parallel,
    Offload
};

    //
    ExecType type;
    bool parallel = false;
    bool offload = false;
    int blockSize = 512;
    // implement syncing
    void sync(){};
};

// just serial
class SerialExec: public ExecutionPolicy{
public:
    SerialExec(){
        type = ExecType::Serial;
    }
};

// parallel host
class ParallelExec: public ExecutionPolicy{
public:
    ParallelExec(){
        type = ExecType::Parallel;
    }
};

// parallel device, or if not built for GPUs parallel host
class OffloadExec: public ExecutionPolicy{
public:
    devStream_t *streamPtr;
    OffloadExec(){
        type = ExecType::Offload;
    }
};

#endif //PHELL_EXECUTIONPOLICY_H
