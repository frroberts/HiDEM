//
// Created by Fredrik Robertsen on 22/03/2021.
//

#ifndef PHELL_PARALLELREDUCE_H
#define PHELL_PARALLELREDUCE_H

#include <limits>
#include <ManVec.h>

#include <cstdint>


#ifdef ENABLE_OMP
#include <omp.h>
#endif


#ifdef GPU
#include "DevPortabilityLayer.h"

#include <thrust/reduce.h>
#include <thrust/functional.h>
#include <thrust/execution_policy.h>

#ifdef ENABLE_CUDA
#include <cub/cub.cuh>
namespace cubX = cub;
#elif ENABLE_HIP
//#include <cub/cub.cuh>

#include <hipcub/hipcub.hpp>
namespace cubX = hipcub;
#endif
#endif


namespace {

#ifdef GPU
    namespace DevKernels2 {


        template<typename T, class ReduceOp, class Gen, class... Args>
        __global__
        void reduceKern(size_t size, void *scratchPtr, ReduceOp op, Gen gen, [[maybe_unused]] Args &... args) {
            //
            size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
            if (idx >= size)
                idx = size - 1;
            //
            __shared__ T arr[32];
            __syncthreads();


            T val = gen(idx, args...);


            for (int offset = 1; offset <= 16; offset *= 2) {

#ifdef ENABLE_CUDA
                T other = __shfl_down_sync(0xffffffff, val, offset);
#elif ENABLE_HIP
                T other = __shfl_down(val, offset);
#endif
                if (idx + offset < size) {
                    T out = op(val, other);
                    val = out;
                }
            }

            if (threadIdx.x % 32 == 0) {
                int pos = threadIdx.x / 32;
                arr[pos] = val;
            }

            __syncthreads();

            if (threadIdx.x < 32) {
                val = arr[threadIdx.x];

                for (int offset = 1; offset <= 16; offset *= 2) {

#ifdef ENABLE_CUDA
                T other = __shfl_down_sync(0xffffffff, val, offset);
#elif ENABLE_HIP
                T other = __shfl_down(val, offset);
#endif
                    if ((blockIdx.x * blockDim.x + ((threadIdx.x + offset) * 32)) < size) {
                        T out = op(val, other);
                        val = out;
                    }
                }

                if (threadIdx.x == 0) {
                    T *tPtr = (T *) scratchPtr;
                    tPtr[blockIdx.x] = val;
                }
            }

        }


    }
#endif
}

template<class T>
class ParallelReduce {
    uint8_t *temp = nullptr;
    size_t tempSpaceBytes = 0;


#ifdef GPU
    size_t getTempSpaceDev(size_t fullSize) {
        return (fullSize / 512) + 1;
    }

#endif

    int getThreadCount() {
#ifdef ENABLE_OMP
        return omp_get_num_threads();
#else
        return 1;
#endif
    }

    int getCurrentThreadNum() {
#ifdef ENABLE_OMP
        return omp_get_thread_num();
#else
        return 0;
#endif
    }

    int getMaxThread(){
#ifdef ENABLE_OMP
        return omp_get_max_threads();
#else
        return 1;
#endif

    }
public:
    ParallelReduce() {
        //
    }

    ~ParallelReduce() {
#ifdef GPU
        if (temp != nullptr) {
            devFree(temp);
        }

#endif
    }

    T sum(ManVec<T> &in) {
        T* ptr = in.data();

        return reduce<T>(in.size(), [] DEVCALLABLE (T a, T b){
            return a+b;
        },
        [=] DEVCALLABLE (int i){
            return ptr[i];
        });
    }

    template<class ValOp, class... Args>
    T maxDerived(size_t count, ValOp gen, [[maybe_unused]] Args &... args) {
        return reduce<T>(count, [] DEVCALLABLE (T a, T b){
            if (a > b)
                return a;
            return b;
        }, gen, args...);
    }

    T max(ManVec<T> &in) {
        T* ptr = in.data();
        return maxDerived(in.size(),
        [=] DEVCALLABLE (int i){
            return ptr[i];
        });
    }

    template<class Tind>
    T maxIndirect(ManVec<T> &in, ManVec<Tind> &indexes) {
        T* ptr = in.data();

        return maxDerived(indexes.size(),
        [=] DEVCALLABLE (int i){
            return ptr[indexes[i]];
        });
    }

    template<class ValOp, class... Args>
    T minDerived(size_t count, ValOp gen, [[maybe_unused]] Args &... args) {

        return reduce<T>(count, [] DEVCALLABLE (T a, T b){
            if (a < b)
                return a;
            return b;
        }, gen, args...);
    }

    T min(ManVec<T> &in) {
        T* ptr = in.data();
        return minDerived(in.size(),
        [=] DEVCALLABLE (int i){
            return ptr[i];
        });
    }

    template<class Tind>
    T minIndirect(ManVec<T> &in, ManVec<Tind> &indexes) {
        T* ptr = in.data();

        return minDerived(indexes.size(),
        [=] DEVCALLABLE (int i){
            return ptr[indexes[i]];
        });
    }


    template<class ValOp>
    void minMaxDerived(T &minVal, T &maxVal, size_t count, ValOp gen) {

        /*
        auto [min, max] = reduce<std::tuple<T, T>>(count, [] DEVCALLABLE (std::tuple<T, T> a, std::tuple<T, T> b){
            T aMin = std::get<0>(a);
            T bMin = std::get<0>(b);
            T aMax = std::get<1>(a);
            T bMax = std::get<1>(b);

            
            if (aMin<bMin)
                aMin = aMin;
            else
                aMin = bMin;

            if (aMax > bMax)
                aMax = aMax;
            else
                aMax = bMax;

            return std::make_tuple(aMin, aMax);        
        }, 
        [=] DEVCALLABLE (int i){
            T val = gen(i);
            return std::make_tuple(val, val);
        });
         */
        minVal = minDerived(count, gen);
        maxVal = maxDerived(count, gen);
    }

    void minMax(T &minVal, T &maxVal, ManVec<T> &data) {
    //nvtxRangePush("minmax");
/*
        maxVal = thrust::reduce(thrust::device,
                            data.ptr, data.ptr+data.size(),
                            std::numeric_limits<T>::lowest(),
                            thrust::maximum<T>());
        minVal = thrust::reduce(thrust::device,
                            data.ptr, data.ptr+data.size(),
                            std::numeric_limits<T>::max(),
                            thrust::minimum<T>());
*/

        T* ptr = data.data();
        minMaxDerived(minVal, maxVal, data.size(), [=] DEVCALLABLE (int i){
            return ptr[i];
        });

    //nvtxRangePop();

    }

    template<class Tind>
    void minMaxInderect(T &minVal, T &maxVal, ManVec<T> &data, ManVec<Tind> &indexes) {

    //nvtxRangePush("minmax indirect");
        T *ptr = data.data();
        Tind *indexPtr = indexes.data();
/*
        maxVal = thrust::transform_reduce(thrust::device,
                                indexes.ptr, indexes.ptr + indexes.size(),
                                [=] DEVCALLABLE (Tind ind) {
                                    return ptr[ind];
                                },
                                std::numeric_limits<T>::lowest(),
                                thrust::maximum<T>());
        minVal = thrust::transform_reduce(thrust::device,
                                indexes.ptr, indexes.ptr + indexes.size(),
                                [=] DEVCALLABLE (Tind ind) {
                                    return ptr[ind];
                                },
                                std::numeric_limits<T>::max(),
                                thrust::minimum<T>());
*/

        minMaxDerived(minVal, maxVal, indexes.size(), [=] DEVCALLABLE (int i){
            return ptr[indexPtr[i]];
        });

    //nvtxRangePop();
    }


    template<class RetT, class ReduceOp, class Gen, class... Args>
    RetT reduceGenHost(size_t size, ReduceOp op, Gen gen, [[maybe_unused]] Args &... args) {

        /*
        RetT ret = gen(0);
        for (int i = 1; i < size; ++i) {
            ret = op(ret, gen(i));
        }
         */

        int perBlockCount = 1 + (size / getThreadCount());
        size_t neededSpace = getThreadCount();
        if (neededSpace * sizeof(RetT) > tempSpaceBytes) {
            if (temp != nullptr) {
                delete temp;
            }
            temp = (uint8_t *) new RetT[neededSpace];
            tempSpaceBytes = neededSpace * sizeof(RetT);
        }
        RetT *tempTPtr = (RetT *) temp;

#pragma omp parallel
        {
            //
            int tid = getCurrentThreadNum();

            if (0 + (tid * perBlockCount) < size) {
                RetT loc = gen(0 + (tid * perBlockCount), args...);

                for (int i = 1 + (tid * perBlockCount); i < size && i < ((tid + 1) * perBlockCount); ++i) {
                    loc = op(loc, gen(i, args...));
                }

                tempTPtr[tid] = loc;
            }
        };

        RetT ret = tempTPtr[0];
        for (int i = 1; i < getMaxThread(); ++i) {
            if ((i * perBlockCount) < size)
                ret = op(ret, tempTPtr[i]);
        }

        return ret;
    }

#ifdef GPU
    template<class RetT, class ReduceOp, class Gen, class... Args>
    T reduceGenDev(size_t size, ReduceOp op, Gen gen, [[maybe_unused]] Args &... args) {
    //nvtxRangePush("reduce step");

        size_t neededSpace = getTempSpaceDev(size);

        if (neededSpace * sizeof(T) > tempSpaceBytes) {
            if (temp != nullptr) {
                devFree(temp);
            }
            tempSpaceBytes = neededSpace * sizeof(T);
            devMallocManaged((void**)&temp, tempSpaceBytes * sizeof(T) );
        }


        dim3 block = {1024};
        dim3 grid = {((unsigned int)size / block.x)};

        if (size % block.x != 0)
            grid.x++;

        getErrorDev(((DevKernels2::reduceKern<RetT><<<grid, block>>>(size, temp, op, gen, args...))));

        devDeviceSynchronize();

        RetT * tempTPtr = (RetT*) temp;

        T ret = tempTPtr[0];
        for (int i = 1; i < grid.x; ++i) {
            ret = op(ret, tempTPtr[i]);
        }
    //nvtxRangePop();

        return ret;
    }
#endif

    template<class RetT, class ReduceOp, class Gen, class... Args>
    RetT reduce(size_t size, ReduceOp op, Gen gen, [[maybe_unused]] Args &... args) {
#ifdef GPU
        return reduceGenDev<RetT>(size, op, gen, args...);
#else
        return reduceGenHost<RetT>(size, op, gen, args...);
#endif
    }




};


#endif //PHELL_PARALLELREDUCE_H
