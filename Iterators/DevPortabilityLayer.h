//
// Created by Fredrik Robertsen on 24/08/2022.
//

#ifndef PHELL_DEVPORTABILITYLAYER_H
#define PHELL_DEVPORTABILITYLAYER_H

#include <cstdio>
#include <iostream>
#include <cstdint>

#define CUDA_ENABLED ( ENABLE_CUDA == 1)
#define HIP_ENABLED ( ENABLE_HIP == 1)

#if CUDA_ENABLED
#include <cuda_runtime_api.h>
#include <cuda_profiler_api.h>
#include <nvtx3/nvToolsExt.h>
#elif HIP_ENABLED
// #include <hip_runtime_api.h>
#include "hip/hip_runtime.h"
// #include <roctracer_ext.h>
#else
#endif

// launch bounds for AMD systems
#if (defined(ENABLE_HIP) && ENABLE_HIP == 1)
#define LBounds(MAX_THREADS_PER_BLOCK, MIN_WARPS_PER_EXECUTION_UNIT) __launch_bounds__(MAX_THREADS_PER_BLOCK, MIN_WARPS_PER_EXECUTION_UNIT)
#else
#define LBounds(MAX_THREADS_PER_BLOCK, MIN_WARPS_PER_EXECUTION_UNIT)
#endif

// partial implementation of a portability layer between HIP and cuda as i dont want to hipify the entire thing


#if CUDA_ENABLED
typedef cudaError_t devError_t;
typedef cudaMemcpyKind devMemcpyKind;
typedef cudaStream_t devStream_t;
#define devMemcpyDefault cudaMemcpyDefault
#define devSuccess cudaSuccess
#define devMemoryAdvise cudaMemoryAdvise

#define xvtxRangeId_t nvtxRangeId_t

#elif HIP_ENABLED
typedef hipError_t devError_t;
typedef hipMemcpyKind devMemcpyKind;
typedef hipStream_t devStream_t;
#define devMemcpyDefault hipMemcpyDefault
#define devSuccess hipSuccess
#define devMemoryAdvise hipMemoryAdvise

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
#if CUDA_ENABLED
    return cudaMallocManaged(ptr, size);
#elif HIP_ENABLED
// #ifdef ENABLE_HIP_MANAGED
//     return hipMallocManaged(ptr, size);
// #else
// #warning "COMPILING MANAGED AS NORMAL ALLOC"
    return hipMalloc(ptr, size);
// #endif
#else
    return 0;
#endif
}

inline devError_t devMalloc(void** ptr, size_t size){
#if CUDA_ENABLED
    return cudaMalloc(ptr, size);
#elif HIP_ENABLED
    return hipMalloc(ptr, size);
#else
    return 0;
#endif
}

inline devError_t devMallocHost(void** ptr, size_t size){
#if CUDA_ENABLED
    return cudaMallocHost(ptr, size);
#elif HIP_ENABLED
    return hipHostMalloc(ptr, size);
#else
    return 0;
#endif
}

inline devError_t devFree(void* ptr){
#if CUDA_ENABLED
    return cudaFree(ptr);
#elif HIP_ENABLED
    return hipFree(ptr);
#else
    return 0;
#endif
}

inline devError_t devFreeHost(void* ptr){
#if CUDA_ENABLED
    return cudaFreeHost(ptr);
#elif HIP_ENABLED
    return hipHostFree(ptr);
#else
    return 0;
#endif
}



inline devError_t devMemcpy(void* dst, const void* src, size_t count, devMemcpyKind kind ){
#if CUDA_ENABLED
    return cudaMemcpy(dst, src, count, kind);
#elif HIP_ENABLED
    return hipMemcpy(dst, src, count, kind);
#else
    return 0;
#endif
}

inline devError_t devMemcpyAsync(void* dst, const void* src, size_t count, devMemcpyKind kind, devStream_t stream = 0){
#if CUDA_ENABLED
    return cudaMemcpyAsync(dst, src, count, kind, stream);
#elif HIP_ENABLED
    return hipMemcpyAsync(dst, src, count, kind, stream);
#else
    return 0;
#endif
}

inline devError_t devSetDevice(int dev){
#if CUDA_ENABLED
    return cudaSetDevice(dev);
#elif HIP_ENABLED
    return hipSetDevice(dev);
#else
    return 0;
#endif
}

inline devError_t devGetDevice(int* dev){
#if CUDA_ENABLED
    return cudaGetDevice(dev);
#elif HIP_ENABLED
    return hipGetDevice(dev);
#else
        return 0;
#endif
}

inline devError_t devGetDeviceCount(int* count){
#if CUDA_ENABLED
    return cudaGetDeviceCount(count);
#elif HIP_ENABLED
    return hipGetDeviceCount(count);
#else
    return 0;
#endif
}

inline devError_t devDeviceSynchronize(){
#if CUDA_ENABLED
    return cudaDeviceSynchronize();
#elif HIP_ENABLED
    return hipDeviceSynchronize();
#else
    return 0;
#endif
}

inline devError_t devProfilerStart(){
#if CUDA_ENABLED
    return cudaProfilerStart();
#elif HIP_ENABLED
    //return roctracer_start();
    return hipError_t();
#else
    return 0;
#endif
}

inline devError_t devProfilerStop(){
#if CUDA_ENABLED
    return cudaProfilerStop();
#elif HIP_ENABLED
    //return roctracer_stop();
    return hipError_t();
#else
    return 0;
#endif
}

inline const char* devGetErrorString(devError_t error){
#if CUDA_ENABLED
    return cudaGetErrorString(error);
#elif HIP_ENABLED
    return hipGetErrorString(error);
#else
    return 0;
#endif
}

inline devError_t devGetLastError(){
#if CUDA_ENABLED
    return cudaGetLastError();
#elif HIP_ENABLED
    return hipGetLastError();
#else
    return 0;
#endif
}

inline devError_t devPeekAtLastError(){
#if CUDA_ENABLED
    return cudaPeekAtLastError();
#elif HIP_ENABLED
    return hipPeekAtLastError();
#else
    return 0;
#endif
}

inline devError_t devMemAdvice(const void * ptr, size_t count, devMemoryAdvise advice, int device){
   #if CUDA_ENABLED
    return cudaMemAdvise(ptr, count, advice, device);
#elif HIP_ENABLED
    return hipMemAdvise(ptr, count, advice, device);
#else
    return 0;
#endif
}

inline devError_t devDeviceGetPCIBusId(char* pciBusId, int  len, int  device) {
#if CUDA_ENABLED
    return cudaDeviceGetPCIBusId(pciBusId, len, device);
#elif HIP_ENABLED
    return hipDeviceGetPCIBusId(pciBusId, len, device);
#else
    return 0;
#endif
    //
}

inline xvtxRangeId_t xvtxRangeStart(std::string name){
#if CUDA_ENABLED
    return nvtxRangeStart(name.c_str());
#elif HIP_ENABLED
    return 0;
#else
    return 0;
#endif
}

inline void xvtxRangeEnd(xvtxRangeId_t range){
#if CUDA_ENABLED
    nvtxRangeEnd (range);
#elif HIP_ENABLED
#else
#endif
}



#endif //PHELL_DEVPORTABILITYLAYER_H
