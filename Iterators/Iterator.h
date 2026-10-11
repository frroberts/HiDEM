//
// Created by Fredrik Robertsen on 19/06/2021.
//

#ifndef PHELL_ITERATOR_H
#define PHELL_ITERATOR_H


#ifdef GPU
#include "DevPortabilityLayer.h"
#endif

#include <tuple>
#include <map>
#include <algorithm>

#include <cstdio>

#include "ExecutionPolicy.h"
#include "ManVec.h"
namespace {

#ifdef GPU
    namespace DevKernels2 {
        template<class Tind,class F, class... Args>
        __global__ void iterateKern(F fun, Tind max, [[maybe_unused]] Args &... args) {
            //
            int idx = blockIdx.x * blockDim.x + threadIdx.x;

            if (idx >= max) return;

            fun(idx, args...);
        }

        template<class Tind,class F, class... Args>
        __global__ void iterateBlockedKern(F fun, Tind max, [[maybe_unused]] Args &... args) {
            //
            int idx = blockIdx.x * blockDim.x + threadIdx.x;

            if (idx >= max) return;

            fun(blockIdx.x, threadIdx.x, args...);
        }

        template<class Tind,class F, class... Args>
        __global__ void iterateIndirectKern(F fun, Tind max, Tind *indexes, [[maybe_unused]] Args &... args) {
            //
            int idx = blockIdx.x * blockDim.x + threadIdx.x;

            if (idx >= max) return;
            fun(indexes[idx], args...);
        }

        template<class Tind,class F, class... Args>
        __global__ void iterate3dKern(F fun, Tind xMax, Tind yMax, Tind zMax, [[maybe_unused]] Args &... args) {
            int idx = blockIdx.x * blockDim.x + threadIdx.x;
            int idy = blockIdx.y * blockDim.y + threadIdx.y;
            int idz = blockIdx.z * blockDim.z + threadIdx.z;
            if (idx >= xMax) return;
            if (idy >= yMax) return;
            if (idz >= zMax) return;

            fun(idx, idy, idz, args...);
        }

        template<class Tind,class T>
        __global__ void copyItoDMulti(Tind max, Tind **indexes, T **from, T **to) {
            int i = blockIdx.x * blockDim.x + threadIdx.x;
            int ptrInd = threadIdx.y;
            if (i >= max) return;

            int indirectInd = indexes[ptrInd][i];

            to[ptrInd][i] = from[ptrInd][indirectInd];
        }

        template<class Tind,class T>
        __global__ void copyDtoIMulti(Tind max, Tind **indexes, T **from, T **to) {
            int i = blockIdx.x * blockDim.x + threadIdx.x;
            int ptrInd = threadIdx.y;
            if (i >= max) return;

            int indirectInd = indexes[ptrInd][i];

            from[ptrInd][indirectInd] = to[ptrInd][i];
        }

        template<class F, class... Args>
            __global__ void iterateNonExitKern(F fun, [[maybe_unused]] Args &... args) {
            //
            fun(args...);
        }

        template<class Tind, typename Tdat>
        DEVCALLABLE
        static void gatherScatterRecurseDev(Tind toInd, Tind fromInd, Tdat* toBuffer,  Tdat* fromBuffer) {
            toBuffer[toInd] = fromBuffer[fromInd];
        }

        template<class Tind, typename Tdat, typename... TdatPack>
        DEVCALLABLE
        static void gatherScatterRecurseDev(Tind toInd, Tind fromInd, Tdat *toBuffer, Tdat *fromBuffer, [[maybe_unused]] TdatPack *... inOutBuffers) {
            toBuffer[toInd] = fromBuffer[fromInd];
            gatherScatterRecurseDev(toInd, fromInd, inOutBuffers...);
        }


        template<class Tind, typename Tdat>
        DEVCALLABLE
        static void gatherScatterRecursPackeDev(Tind toInd, Tind fromInd, Tdat* toBuffer,  Tdat* fromBuffer) {
            toBuffer[toInd] = fromBuffer[fromInd];
        }

        template<class Tind, typename Tdat, typename... TdatPack>
        DEVCALLABLE
        static void gatherScatterRecursePackDev(Tind toInd, Tind fromInd, Tdat *toBuffer, Tdat *fromBuffer, [[maybe_unused]] TdatPack *... fromBuffers) {
            toBuffer[toInd] = fromBuffer[fromInd];
            gatherScatterRecurseDev(toInd, fromInd, toBuffer, fromBuffers...);
        }

        template<class Tind, typename... Tdat>
        __global__ void gatherKern(int max, Tind *indexes, [[maybe_unused]] Tdat* ...inOutBuffers)
        {
            //
            Tind idx = blockIdx.x * blockDim.x + threadIdx.x;
            if (idx >= max) return;
            Tind index = indexes[idx];

            gatherScatterRecurseDev(idx, index, inOutBuffers...);
        }

        template<class Tind, typename... Tdat>
        __global__ void scatterKern(int max, Tind *indexes, [[maybe_unused]] Tdat *...inOutBuffers) {
            //
            Tind idx = blockIdx.x * blockDim.x + threadIdx.x;
            if (idx >= max) return;
            Tind index = indexes[idx];

            gatherScatterRecurseDev(index, idx, inOutBuffers...);
        }



/*
        template<class Tind, typename Tdat>
        DEVCALLABLE
        inline void gatherRecursePackDev(int stride, Tind toInd, Tind fromInd, Tdat *toBuffer, Tdat *fromBuffer) {
            toBuffer[toInd] = fromBuffer[fromInd];
        }

        template<class Tind, typename Tdat, typename... TdatPack>
        DEVCALLABLE
        inline void gatherRecursePackDev(int stride, Tind toInd, Tind fromInd, Tdat *toBuffer, Tdat *fromBuffer, [[maybe_unused]] TdatPack *... fromBuffers) {
            toBuffer[toInd + (sizeof...(fromBuffers)*stride)] = fromBuffer[fromInd];
            gatherRecursePackDev(stride, toInd, fromInd, toBuffer, fromBuffers...);
        }

        template<class Tind, typename Tdat>
        DEVCALLABLE
        inline void scatterRecursePackDev(int stride, Tind fromInd, Tind toInd, Tdat *fromBuffer, Tdat *toBuffer) {
            toBuffer[toInd] = fromBuffer[fromInd];
        }

        template<class Tind, typename Tdat, typename... TdatPack>
        DEVCALLABLE
        inline void scatterRecursePackDev(int stride, Tind fromInd, Tind toInd, Tdat *fromBuffer, Tdat *toBuffer, [[maybe_unused]] TdatPack *... toBuffers) {
            toBuffer[toInd] = fromBuffer[(fromInd) + (sizeof...(toBuffers)*stride)];
            scatterRecursePackDev(stride, fromInd, toInd, fromBuffer, toBuffers...);

        }

        template<class Tind, typename Tdat, typename... TdatPack>
        __global__ void gatherPackedKern(int max, Tind *indexes, Tdat* outBuffer, [[maybe_unused]] TdatPack* ...inBuffers)
        {
            //
            Tind idx = blockIdx.x * blockDim.x + threadIdx.x;
            if (idx >= max) return;
            Tind index = indexes[idx];

            gatherRecursePackDev(max, idx, index, outBuffer, inBuffers...);
        }

        template<class Tind, typename Tdat, typename... TdatPack>
        __global__ void scatterPackedKern(int max, Tind *indexes, Tdat* inBuffer, [[maybe_unused]] TdatPack *...outBuffers) {
            //
            Tind idx = blockIdx.x * blockDim.x + threadIdx.x;
            if (idx >= max) return;
            Tind index = indexes[idx];

            scatterRecursePackDev(max, idx, index, inBuffer, outBuffers...);
        }
*/
    }
#endif


    class Iterator {
    public:

#ifdef GPU

        class IteratorGPU {
        public:

            // is there a reason why the 1d things take a tuple as the input ?
            static auto getGrid(unsigned int max) {
                dim3 block = {512};
                dim3 grid = {1 + (max / block.x)};

                return std::make_tuple(block, grid);
            }

            static auto getGrid(std::tuple<unsigned int> max, unsigned int block = 512) {
                dim3 threadBlock = block;
                dim3 grid = {1 + (std::get<0>(max) / threadBlock.x)};
                return std::make_tuple(block, grid);
            }

            static auto getGrid3(std::tuple<unsigned int, unsigned int, unsigned int> max) {
                dim3 block = {8, 8, 1};
                dim3 grid = {1 + (std::get<0>(max) / block.x), 1 + (std::get<1>(max) / block.y), 1 + (std::get<2>(max) / block.z)};

                return std::make_tuple(block, grid);
            }

            static auto getGrid3(std::tuple<unsigned int, unsigned int, unsigned int> max, std::tuple<unsigned int, unsigned int, unsigned int> block_) {
                dim3 block = {std::get<0>(block_), std::get<1>(block_), std::get<2>(block_)};
                dim3 grid = {1 + (std::get<0>(max) / block.x), 1 + (std::get<1>(max) / block.y), 1 + (std::get<2>(max) / block.z)};

                return std::make_tuple(block, grid);
            }

            template<class Tind,class F, class... Args>
            static void iterate(F fun, Tind max, [[maybe_unused]] Args &... args) {
                auto[block, grid] = getGrid(max, 512);
                getErrorDev(((DevKernels2::iterateKern<<<grid, block>>>(fun, max, args...))));
            }

            template<class Tind,class F, class... Args>
            static void iterateBlocked(F fun, Tind blockSize, Tind max, [[maybe_unused]] Args &... args) {
                auto[block, grid] = getGrid({max}, blockSize);
                getErrorDev(((DevKernels2::iterateBlockedKern<<<grid, block>>>(fun, max, args...))));
            }


            template<class Tind,class F, class... Args>
            static void iterateIndirect(F fun, ManVec<Tind> &indexes, [[maybe_unused]] Args &... args) {
                auto[block, grid] = getGrid({indexes.size()}, 512);
                Tind max = indexes.size();
                getErrorDev(((DevKernels2::iterateIndirectKern<<<grid, block>>>(fun, max, indexes.data(), args...))));
            }

            template<class Tind,class F, class... Args>
            static void iterateIndirect(ExecutionPolicy pol, F fun, ManVec<Tind> &indexes, [[maybe_unused]] Args &... args) {
                auto[block, grid] = getGrid({indexes.size()}, pol.blockSize);
                Tind max = indexes.size();
                getErrorDev(((DevKernels2::iterateIndirectKern<<<grid, block>>>(fun, max, indexes.data(), args...))));
            }


            template<class Tind,class F, class... Args>
            static void iterate3D(F fun, Tind xMax, Tind yMax, Tind zMax, [[maybe_unused]] Args &... args) {
                auto[block, grid] = getGrid3({xMax, yMax, zMax});

                getErrorDev(((DevKernels2::iterate3dKern<<<grid, block>>>(fun, xMax, yMax, zMax, args...))));

            }

            template<class Tind,typename... Tdat>
            static void gather(int count, Tind *indexes, [[maybe_unused]] Tdat* ...inOutBuffers) {
                auto[block, grid] = getGrid({count}, 128);
                getErrorDev(((DevKernels2::gatherKern<<<grid, block>>>(count, indexes, inOutBuffers...))));
            }

            template<class Tind,typename... Tdat>
            static void scatter(int count, Tind *indexes, [[maybe_unused]] Tdat* ...inOutBuffers) {
                auto[block, grid] = getGrid({count}, 128);
                getErrorDev(((DevKernels2::scatterKern<<<grid, block>>>(count, indexes, inOutBuffers...))));
            }
        };

#endif


        class IteratorSTL {

        public:
            template<class Tind, class F, class... Args>
            static void iterate([[maybe_unused]] bool parallel, F fun, Tind max, [[maybe_unused]] Args &... args) {
                for (int x = 0; x < max; x++) {
                    fun(x, args...);
                }
            }

            template<class Tind, class F, class... Args>
            static void iterateIndirect([[maybe_unused]] bool parallel, F fun, ManVec<Tind> &indexes, [[maybe_unused]] Args &... args) {
                std::for_each(indexes.begin(), indexes.end(), [&](Tind ind) {
                    fun(ind, args...);
                });

            }

            template<class Tind, class F, class... Args>
            static void iterate3D([[maybe_unused]] bool parallel, F fun, Tind xMax, Tind yMax, Tind zMax, [[maybe_unused]] Args &... args) {
                for (int x = 0; x < xMax; x++) {
                    for (int y = 0; y < yMax; y++) {
                        for (int z = 0; z < zMax; z++) {
                            fun(x, y, z, args...);
                        }
                    }
                }
            }

            template<typename Tdat, typename Tind>
            static Tdat reduceSumIndirect([[maybe_unused]] bool parallel, Tdat *buffer, Tind *indexes, int count) {
                Tdat sum = 0;
                for (int i = 0; i < count; ++i) {
                    sum += buffer[indexes[i]];
                }
                return sum;
            }

            template<class Tind, class F, class... Args>
            static void iterateBlocked([[maybe_unused]] bool parallel, F fun, Tind blockSize, Tind max, [[maybe_unused]] Args &... args) {
                int blocks = 1 + (max/blockSize);
                for (int b = 0; b < blocks; b++) {
                    for (int i = 0; i < blockSize; ++i) {
                        if((b*blockSize)+i < max)
                            fun(b, i, args...);
                    }
                }
            }
        };

        class IteratorHost {

        public:
            template<class Tind, class F, class... Args>
            static void iterate([[maybe_unused]] bool parallel, F fun, Tind max, [[maybe_unused]] Args &... args) {
#pragma omp parallel for if(parallel)
                for (int x = 0; x < max; x++) {
                    fun(x, args...);
                }
            }

            template<class Tind, class F, class... Args>
            static void iterateIndirect([[maybe_unused]] bool parallel, F fun, ManVec<Tind> &indexes, [[maybe_unused]] Args &... args) {
#pragma omp parallel for if(parallel)
                for (int x = 0; x < indexes.size(); x++) {
                    fun(indexes[x], args...);
                }
            }

            template<class Tind, class F, class... Args>
            static void iterate3D([[maybe_unused]] bool parallel, F fun, Tind xMax, Tind yMax, Tind zMax, [[maybe_unused]] Args &... args) {
#pragma omp parallel for if(parallel)
                for (int x = 0; x < xMax; x++) {
                    for (int y = 0; y < yMax; y++) {
#pragma omp simd
                        for (int z = 0; z < zMax; z++) {
                            fun(x, y, z, args...);
                        }
                    }
                }
            }

            template<typename Tdat, typename Tind>
            static Tdat reduceSumIndirect([[maybe_unused]] bool parallel, Tdat *buffer, Tind *indexes, int count) {
                Tdat sum = 0;
#pragma omp parallel for reduction(+: sum) if(parallel)
                for (int i = 0; i < count; ++i) {
                    sum += buffer[indexes[i]];
                }
                return sum;
            }

            template<class Tind, class F, class... Args>
            static void iterateBlocked([[maybe_unused]] bool parallel, F fun, Tind blockSize, Tind max, [[maybe_unused]] Args &... args) {
                int blocks = 1 + (max/blockSize);
#pragma omp parallel for if(parallel)
                for (int b = 0; b < blocks; b++) {
                    for (int i = 0; i < blockSize; ++i) {
                        if((b*blockSize)+i < max)
                            fun(b, i, args...);
                    }
                }
            }
        };

    public:
        template<class Tind, class F, class... Args>
        static void iterateSerial(F fun, Tind max, [[maybe_unused]] Args &... args) {
            IteratorHost::iterate(false, fun, max, args...);
        }

        template<class Tind, class Pol, class F, class... Args>
        static void iterate3D([[maybe_unused]] Pol pol, F fun, Tind maxX, Tind maxY, Tind maxZ, [[maybe_unused]] Args &... args) {
            //
            //std::cout << "non offload" << std::endl;
            switch (pol.type) {
                case ExecutionPolicy::ExecType::Serial:
                    IteratorHost::iterate3D(false, fun, maxX, maxY, maxZ, args...);
                    break;
                case ExecutionPolicy::ExecType::Parallel:
                    IteratorHost::iterate3D(true, fun, maxX, maxY, maxZ, args...);
                    break;
                case ExecutionPolicy::ExecType::Offload:
                    // you should never make it here
                    printf("You should not be here, Offload exec is compiled wrong");
                    abort();
                    break;

            }
        }

        template<class Tind, class F, class... Args>
        static void iterate3D(OffloadExec pol, F fun, Tind maxX, Tind maxY, Tind maxZ, [[maybe_unused]] Args &... args) {
#ifdef GPU
            IteratorGPU::iterate3D(fun, maxX, maxY, maxZ, args...);
#else
            IteratorHost::iterate3D(true, fun, maxX, maxY, maxZ, args...);
#endif
        }

        template<class Tind, class Pol, class F, class... Args>
        static void iterate([[maybe_unused]] Pol pol, F fun, Tind max, [[maybe_unused]] Args &... args) {
            //
            //std::cout << "non offload" << std::endl;
            switch (pol.type) {
                case ExecutionPolicy::ExecType::Serial:
                    IteratorHost::iterate(false, fun, max, args...);
                    break;
                case ExecutionPolicy::ExecType::Parallel:
                    IteratorHost::iterate(true, fun, max, args...);
                    break;
                case ExecutionPolicy::ExecType::Offload:
                    // you should never make it here
                    printf("You should not be here, Offload exec is compiled wrong");
                    abort();
                    break;
            }
        }

        template<class Tind, class Pol, class F, class... Args>
        static void iterateBlocked([[maybe_unused]] Pol pol, F fun, Tind blockSize, Tind max, [[maybe_unused]] Args &... args) {
            switch (pol.type) {
                case ExecutionPolicy::ExecType::Serial:
                    IteratorHost::iterateBlocked(false, fun, blockSize, max, args...);
                    break;
                case ExecutionPolicy::ExecType::Parallel:
                    IteratorHost::iterateBlocked(true, fun, blockSize, max, args...);
                    break;
                case ExecutionPolicy::ExecType::Offload:
                    // you should never make it here
                    printf("You should not be here, Offload exec is compiled wrong");
                    abort();
                    break;
            }
        }

        template<class Tind, class F, class... Args>
        static void iterate([[maybe_unused]] OffloadExec pol, F fun, Tind max, [[maybe_unused]] Args &... args) {
#ifdef GPU
            IteratorGPU::iterate(fun, max, args...);
#else
            IteratorHost::iterate(true, fun, max, args...);
#endif
        }


        // for serial and parallel
        template<class Tind, class Pol, class F, class... Args>
        static void iterateIndirect([[maybe_unused]] Pol pol, F fun, ManVec<Tind> &indexes, [[maybe_unused]] Args &... args) {
            //std::cout << "non offload" << std::endl;
            switch (pol.type) {
                case ExecutionPolicy::ExecType::Serial:
                    IteratorHost::iterateIndirect(false, fun, indexes, args...);
                    break;
                case ExecutionPolicy::ExecType::Parallel:
                    IteratorHost::iterateIndirect(true, fun, indexes, args...);
                    break;
                case ExecutionPolicy::ExecType::Offload:
                    // you should never make it here
                    printf("You should not be here, Offload exec is compiled wrong");
                    abort();
                    break;
            }
        }


        // overload for offload only
        template<class Tind, class F, class... Args>
        static void iterateIndirect([[maybe_unused]] OffloadExec pol, F fun, ManVec<Tind> &indexes, [[maybe_unused]] Args &... args) {
            //std::cout << "indirect offload" << std::endl;
#ifdef GPU
            IteratorGPU::iterateIndirect(fun, indexes, args...);
#else
            IteratorHost::iterateIndirect(true, fun, indexes, args...);
#endif

        }

        template<class Tind, class F, class... Args>
        static void iterateBlocked([[maybe_unused]] OffloadExec pol, F fun, Tind blockSize, Tind max, [[maybe_unused]] Args &... args) {
#ifdef GPU
            IteratorGPU::iterateBlocked(fun, blockSize, max, args...);
#else
            IteratorHost::iterateBlocked(true, fun, blockSize, max, args...);
#endif
        }

        template<typename Tdat, typename Tind>
        DEVCALLABLE
        static void gatherScatterRecurse(Tind toInd, Tind fromInd, Tdat* toBuffer,  Tdat* fromBuffer) {
            toBuffer[toInd] = fromBuffer[fromInd];
        }

        template<typename Tdat, typename Tind,  typename... TdatPack>
        DEVCALLABLE
        static void gatherScatterRecurse(Tind toInd, Tind fromInd, Tdat *toBuffer, Tdat *fromBuffer, TdatPack *... inOutBuffers) {
            toBuffer[toInd] = fromBuffer[fromInd];
            gatherScatterRecurse(toInd, fromInd, inOutBuffers...);
        }

        template<typename... Tdat, typename Tind>
        static void gatherMulti([[maybe_unused]] OffloadExec pol, int count, Tind *indexes, [[maybe_unused]] Tdat *...inOutBuffers) {
#ifdef GPU
            IteratorGPU::gather(count, indexes, inOutBuffers...);
#else
            Iterator::iterate(pol, [=] DEVCALLABLE (Tind i){
                Tind index = indexes[i];
                gatherScatterRecurse(i, index, inOutBuffers...);
                }, count);
#endif
        }

        template<typename... Tdat, typename Tind>
        static void scatterMulti([[maybe_unused]] OffloadExec pol, int count, Tind *indexes, [[maybe_unused]] Tdat *...inOutBuffers) {
#ifdef GPU
            IteratorGPU::scatter(count, indexes, inOutBuffers...);

#else
            Iterator::iterate(pol, [=] DEVCALLABLE (Tind i){
                Tind index = indexes[i];
                gatherScatterRecurse(index, i, inOutBuffers...);
                }, count);
#endif
        }

                template<typename... Tdat, typename Tind>
        static void gatherMulti([[maybe_unused]] ParallelExec pol, int count, Tind *indexes, [[maybe_unused]] Tdat *...inOutBuffers) {
#ifdef GPU
            IteratorGPU::gather(count, indexes, inOutBuffers...);
#else
            Iterator::iterate(pol, [=] DEVCALLABLE (Tind i){
                Tind index = indexes[i];
                gatherScatterRecurse(i, index, inOutBuffers...);
                }, count);
#endif
        }

        template<typename... Tdat, typename Tind>
        static void scatterMulti([[maybe_unused]] ParallelExec pol, int count, Tind *indexes, [[maybe_unused]] Tdat *...inOutBuffers) {
#ifdef GPU
            IteratorGPU::scatter(count, indexes, inOutBuffers...);

#else
            Iterator::iterate(pol, [=] DEVCALLABLE (Tind i){
                Tind index = indexes[i];
                gatherScatterRecurse(index, i, inOutBuffers...);
                }, count);
#endif
        }

        template<typename Tdat, typename Tind>
        static void gather([[maybe_unused]] OffloadExec pol, Tdat *targetBuffer, Tdat *fromBuffer, Tind *indexes, int count) {
            Iterator::iterate(pol, [=] DEVCALLABLE (Tind i){
                targetBuffer[i] = fromBuffer[indexes[i]];
                }, count);
        }


        template<typename Tdat, typename Tind>
        static void scatter([[maybe_unused]] OffloadExec pol, Tdat *targetBuffer, Tdat *fromBuffer, Tind *indexes, int count) {
            Iterator::iterate(pol, [=] DEVCALLABLE (Tind i){
                targetBuffer[indexes[i]] = fromBuffer[i];
                }, count);
        }

        template<typename Tdat>
        static void copy(Tdat *targetBuffer, Tdat *fromBuffer, int count) {
            // this is really a memcpy
            // if building for GPU check what the pointer is and do cudamemcpy if it is
            std::memcpy (targetBuffer , fromBuffer, count);
        }

        template<typename Tdat, typename Tind>
        static Tdat reduceSumIndirect(Tdat *buffer, Tind *indexes, int count) {
            return IteratorHost::reduceSumIndirect(true, buffer, indexes, count);
        }

        static void syncAll() {
#ifdef GPU
            auto err = devDeviceSynchronize();
            // todo: check the error code
            if (err != devSuccess)
                std::cout << err << std::endl;
#else
#endif
            //
        }
    };
}

#endif //PHELL_ITERATOR_H
