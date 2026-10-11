//
// Created by Fredrik Robertsen on 24/08/2022.
//

#ifndef PHELL_DEVPORTABILITYLAYER_H
#define PHELL_DEVPORTABILITYLAYER_H

#include <cstdio>
#include <iostream>
#include <cstdint>

// this is a bit meh, fix better at some point
#include "../PLiB/Global.h"


#if ENABLE_CUDA
#include <cuda_runtime_api.h>
#include <cuda_profiler_api.h>
#include <nvtx3/nvToolsExt.h>
#elif ENABLE_HIP
// #include <hip_runtime_api.h>
#include "hip/hip_runtime.h"
#include "rocm_smi/rocm_smi.h"
// #include <roctracer_ext.h>
#else
#endif

// launch bounds for AMD systems
#if ENABLE_HIP == 1
#define LBounds(MAX_THREADS_PER_BLOCK, MIN_WARPS_PER_EXECUTION_UNIT) __launch_bounds__(MAX_THREADS_PER_BLOCK, MIN_WARPS_PER_EXECUTION_UNIT)
#else
#define LBounds(MAX_THREADS_PER_BLOCK, MIN_WARPS_PER_EXECUTION_UNIT)
#endif

// partial implementation of a portability layer between HIP and cuda as i dont want to hipify the entire thing


#if ENABLE_CUDA
typedef cudaError_t devError_t;
typedef cudaMemcpyKind devMemcpyKind;
typedef cudaStream_t devStream_t;
#define devMemcpyDefault cudaMemcpyDefault
#define devSuccess cudaSuccess
#define devMemoryAdvise cudaMemoryAdvise
#define devStreamNonBlocking cudaStreamNonBlocking

#define xvtxRangeId_t nvtxRangeId_t

#elif ENABLE_HIP
typedef hipError_t devError_t;
typedef hipMemcpyKind devMemcpyKind;
typedef hipStream_t devStream_t;
#define devMemcpyDefault hipMemcpyDefault
#define devSuccess hipSuccess
#define devMemoryAdvise hipMemoryAdvise
#define devStreamNonBlocking hipStreamNonBlocking

#define xvtxRangeId_t uint64_t

#else
typedef int devError_t;
typedef int devMemcpyKind;
typedef int devStream_t;
#define devMemcpyDefault 0
typedef int devError;
typedef int devMemoryAdvise;
#define xvtxRangeId_t uint64_t

#endif


inline devError_t devMallocManaged(void** ptr, size_t size){
#if ENABLE_CUDA
    return cudaMallocManaged(ptr, size);
#elif ENABLE_HIP
// #ifdef ENABLE_HIP_MANAGED
//     return hipMallocManaged(ptr, size);
// #else
// #warning "COMPILING MANAGED AS NORMAL ALLOC"
    return hipMalloc(ptr, size);
// #endif
#else
    *ptr = malloc(size);
    return 0;
#endif
}

inline devError_t devMalloc(void** ptr, size_t size){
#if ENABLE_CUDA
    return cudaMalloc(ptr, size);
#elif ENABLE_HIP
    return hipMalloc(ptr, size);
#else
    return 0;
#endif
}

inline devError_t devMallocHost(void** ptr, size_t size){
#if ENABLE_CUDA
    return cudaMallocHost(ptr, size);
#elif ENABLE_HIP
    return hipHostMalloc(ptr, size);
#else
    return 0;
#endif
}

inline devError_t devFree(void* ptr){
#if ENABLE_CUDA
    return cudaFree(ptr);
#elif ENABLE_HIP
    return hipFree(ptr);
#else
    free(ptr);
    return 0;
#endif
}

inline devError_t devFreeHost(void* ptr){
#if ENABLE_CUDA
    return cudaFreeHost(ptr);
#elif ENABLE_HIP
    return hipHostFree(ptr);
#else
    return 0;
#endif
}



inline devError_t devMemcpy(void* dst, const void* src, size_t count, devMemcpyKind kind ){
#if ENABLE_CUDA
    return cudaMemcpy(dst, src, count, kind);
#elif ENABLE_HIP
    return hipMemcpy(dst, src, count, kind);
#else
    return 0;
#endif
}

inline devError_t devMemcpyAsync(void* dst, const void* src, size_t count, devMemcpyKind kind, devStream_t stream = 0){
#if ENABLE_CUDA
    return cudaMemcpyAsync(dst, src, count, kind, stream);
#elif ENABLE_HIP
    return hipMemcpyAsync(dst, src, count, kind, stream);
#else
    return 0;
#endif
}

inline devError_t devSetDevice(int dev){
#if ENABLE_CUDA
    return cudaSetDevice(dev);
#elif ENABLE_HIP
    return hipSetDevice(dev);
#else
    return 0;
#endif
}

inline devError_t devGetDevice(int* dev){
#if ENABLE_CUDA
    return cudaGetDevice(dev);
#elif ENABLE_HIP
    return hipGetDevice(dev);
#else
        return 0;
#endif
}

inline devError_t devGetDeviceCount(int* count){
#if ENABLE_CUDA
    return cudaGetDeviceCount(count);
#elif ENABLE_HIP
    return hipGetDeviceCount(count);
#else
    return 0;
#endif
}

inline devError_t devDeviceSynchronize(){
#if ENABLE_CUDA
    return cudaDeviceSynchronize();
#elif ENABLE_HIP
    return hipDeviceSynchronize();
#else
    return 0;
#endif
}

inline devError_t devProfilerStart(){
#if ENABLE_CUDA
    return cudaProfilerStart();
#elif ENABLE_HIP
    //return roctracer_start();
    return hipError_t();
#else
    return 0;
#endif
}

inline devError_t devProfilerStop(){
#if ENABLE_CUDA
    return cudaProfilerStop();
#elif ENABLE_HIP
    //return roctracer_stop();
    return hipError_t();
#else
    return 0;
#endif
}

inline const char* devGetErrorString(devError_t error){
#if ENABLE_CUDA
    return cudaGetErrorString(error);
#elif ENABLE_HIP
    return hipGetErrorString(error);
#else
    return 0;
#endif
}

inline devError_t devGetLastError(){
#if ENABLE_CUDA
    return cudaGetLastError();
#elif ENABLE_HIP
    return hipGetLastError();
#else
    return 0;
#endif
}

inline devError_t devPeekAtLastError(){
#if ENABLE_CUDA
    return cudaPeekAtLastError();
#elif ENABLE_HIP
    return hipPeekAtLastError();
#else
    return 0;
#endif
}

inline devError_t devMemAdvice(const void * ptr, size_t count, devMemoryAdvise advice, int device){
   #if ENABLE_CUDA
    return cudaMemAdvise(ptr, count, advice, device);
#elif ENABLE_HIP
    return hipMemAdvise(ptr, count, advice, device);
#else
    return 0;
#endif
}

inline devError_t devDeviceGetPCIBusId(char* pciBusId, int  len, int  device) {
#if ENABLE_CUDA
    return cudaDeviceGetPCIBusId(pciBusId, len, device);
#elif ENABLE_HIP
    return hipDeviceGetPCIBusId(pciBusId, len, device);
#else
    return 0;
#endif
    //
}

inline devError_t devStreamCreateWithFlags(devStream_t * pStream, unsigned int  flags){
#if ENABLE_CUDA
    return cudaStreamCreateWithFlags(pStream, flags);
#elif ENABLE_HIP
    return hipStreamCreateWithFlags(pStream, flags);
#else
    return 0;
#endif
}

inline devError_t devStreamDestroy(devStream_t pStream){
#if ENABLE_CUDA
    return cudaStreamDestroy(pStream);
#elif ENABLE_HIP
    return hipStreamDestroy(pStream);
#else
    return 0;
#endif
}

inline devError_t devStreamCreate(devStream_t * pStream ){
#if ENABLE_CUDA
    return cudaStreamCreate(pStream);
#elif ENABLE_HIP
    return hipStreamCreate(pStream);
#else
    return 0;
#endif
}

inline devError_t devStreamSynchronize(devStream_t pStream){
#if ENABLE_CUDA
    return cudaStreamSynchronize(pStream);
#elif ENABLE_HIP
    return hipStreamSynchronize(pStream);
#else
    return 0;
#endif
}

inline xvtxRangeId_t xvtxRangePush(std::string name){
#if ENABLE_CUDA
    return nvtxRangePush(name.c_str());
#elif ENABLE_HIP
    return 0;
#else
    return 0;
#endif
}

inline xvtxRangeId_t xvtxRangeStart(std::string name){
#if ENABLE_CUDA
    return nvtxRangeStart(name.c_str());
#elif ENABLE_HIP
    return 0;
#else
    return 0;
#endif
}

inline void xvtxRangeEnd(xvtxRangeId_t range){
#if ENABLE_CUDA
    nvtxRangeEnd (range);
#elif ENABLE_HIP
#else
#endif
}

inline void xvtxRangePop(){
#if ENABLE_CUDA
    nvtxRangePop();
#elif ENABLE_HIP
#else
#endif
}



#endif //PHELL_DEVPORTABILITYLAYER_H
