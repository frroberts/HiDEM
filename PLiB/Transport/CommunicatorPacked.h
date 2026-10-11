//
// Created by Fredrik Robertsen on 28/12/2020.
//

#ifndef PHELL_COMMUNICATORPACKED_H
#define PHELL_COMMUNICATORPACKED_H

#include <mpi.h>

#include <ManVec.h>
#include "Communicator.h"


/*
namespace CommunicatorPackedKernels{
    template<class Tind, typename Tdat>
    DEVCALLABLE
    static void gatherRecursePackDev(int stride, Tind toInd, Tind fromInd, Tdat *toBuffer, Tdat *fromBuffer) {
        toBuffer[(toInd * stride)] = fromBuffer[fromInd];
    }

    template<class Tind, typename Tdat, typename... TdatPack>
    DEVCALLABLE
    static void gatherRecursePackDev(int stride, Tind toInd, Tind fromInd, Tdat *toBuffer, Tdat *fromBuffer, [[maybe_unused]] TdatPack *... fromBuffers) {
        toBuffer[(toInd * stride) + (sizeof...(fromBuffers))] = fromBuffer[fromInd];
        gatherRecursePackDev(stride, toInd, fromInd, toBuffer, fromBuffers...);
    }

    template<class Tind, typename Tdat>
    DEVCALLABLE
    static void scatterRecursePackDev(int stride, Tind fromInd, Tind toInd, Tdat *fromBuffer, Tdat *toBuffer) {
        toBuffer[toInd] = fromBuffer[(fromInd * stride)];
    }

    template<class Tind, typename Tdat, typename... TdatPack>
    DEVCALLABLE
    static void scatterRecursePackDev(int stride, Tind fromInd, Tind toInd, Tdat *fromBuffer, Tdat *toBuffer, [[maybe_unused]] TdatPack *... toBuffers) {
        toBuffer[toInd] = fromBuffer[(fromInd * stride) + (sizeof...(toBuffers))];
        scatterRecursePackDev(stride, fromInd, toInd, fromBuffer, toBuffers...);

    }

    template<class Tind, typename Tdat, typename... TdatPack>
    __global__ void gatherPackedKern(int max, Tind *indexes, Tdat *outBuffer, [[maybe_unused]] TdatPack *...inBuffers) {
        //
        Tind idx = blockIdx.x * blockDim.x + threadIdx.x;
        if (idx >= max) return;
        Tind index = indexes[idx];

        gatherRecursePackDev(sizeof...(inBuffers), idx, index, outBuffer, inBuffers...);
    }

    template<class Tind, typename Tdat, typename... TdatPack>
    __global__ void scatterPackedKern(int max, Tind *indexes, Tdat *inBuffer, [[maybe_unused]] TdatPack *...outBuffers) {
        //
        Tind idx = blockIdx.x * blockDim.x + threadIdx.x;
        if (idx >= max) return;
        Tind index = indexes[idx];

        scatterRecursePackDev(sizeof...(outBuffers), idx, index, inBuffer, outBuffers...);
    }


    template<class T, class... Tdat, std::size_t... ints>
    DEVCALLABLE
    auto getPtrs(std::tuple<Tdat...> &dataBuffers, std::integer_sequence<std::size_t, ints...>) {
        //

        std::array<T*, sizeof...(Tdat)> ptrs;

        ((ptrs[ints] = std::get<ints>(dataBuffers)->data()), ...);

        return ptrs;
    }

    template<class T, class... Tdat, std::size_t... ints>
    DEVCALLABLE
    T* getPtr(std::tuple<Tdat...> &dataBuffers, int buffInd) {
        //

        switch (buffInd) {
            case 0:
                return std::get<0>(dataBuffers)->data();
            case 1:
                return std::get<1>(dataBuffers)->data();
            case 2:
                return std::get<2>(dataBuffers)->data();
            case 3:
                return std::get<3>(dataBuffers)->data();
            case 4:
                return std::get<4>(dataBuffers)->data();
            case 5:
                return std::get<5>(dataBuffers)->data();
        }
        return nullptr;
    }


    template<class Tind, typename Tdat, typename... TdatPack>
    __global__ void gatherPackedKern2D(int max, Tind *indexes, Tdat *buffPtr, std::tuple<TdatPack...> dataBuffers) {
        //
//        __shared__ Tdat coalaceBuffer[32 * sizeof...(TdatPack)];
//        __shared__ Tind indCache[32];

        int ind = blockIdx.x * blockDim.x + threadIdx.x;

        if (ind >= max) return;

        pInd_t fromInd = indexes[ind];

        int buffInd = threadIdx.y;

        auto ptr = CommunicatorPackedKernels::getPtr<Tdat>(dataBuffers, buffInd);

        buffPtr[(ind * sizeof...(TdatPack)) + buffInd] = ptr[fromInd];


        // coalaceBuffer[(sizeof...(TdatPack) * buffInd) + threadIdx.x] = ;
*

        auto val = ptr[fromInd];
        // not correct but correct pattern at least
        // needs a buffer that is mod 32*6 sized
        buffPtr[(blockIdx.x * blockDim.x * sizeof...(TdatPack)) + (threadIdx.y*blockDim.y)+ threadIdx.x] = val;
        //coalaceBuffer[(threadIdx.x * sizeof...(TdatPack)) + buffInd];
*
    }

    template<class Tind, typename Tdat, typename... TdatPack>
    __global__ void scatterPackedKern2D(int max, Tind *indexes, Tdat *buffPtr, std::tuple<TdatPack...> dataBuffers) {
        //
//        __shared__ Tdat coalaceBuffer[32 * sizeof...(TdatPack)];

        int ind = blockIdx.x * blockDim.x + threadIdx.x;

        if (ind >= max) return;

        pInd_t fromInd = indexes[ind];
        int buffInd = threadIdx.y;

        auto ptr = CommunicatorPackedKernels::getPtr<Tdat>(dataBuffers, buffInd);

        ptr[fromInd] = buffPtr[(ind * sizeof...(TdatPack)) + buffInd];


*
        coalaceBuffer[(threadIdx.x * sizeof...(TdatPack)) + buffInd] = buffPtr[(ind * sizeof...(TdatPack)) + buffInd];

        __syncthreads();

        // not correct but correct pattern at least
        buffPtr[(blockIdx.x * blockDim.x * sizeof...(TdatPack)) + (threadIdx.y*blockDim.y)+threadIdx.y] = coalaceBuffer[(threadIdx.x * sizeof...(TdatPack)) + buffInd];
*
    }
}

*/

template<class T, int numBuffers>
class CommunicatorPacked : public Communicator{

public:

#ifdef GPU
#ifdef ENABLE_GDR
    ManVec<T, ScratchAllocator<T>> outgoingBuffers;
    ManVec<T, ScratchAllocator<T>> incommingBuffers;
#else
    ManVec<T, ScratchAllocator<T>> outgoingBuffers_dev;
    ManVec<T, ScratchAllocator<T>> incommingBuffers_dev;

    ManVec<T, PinnedAllocator<T>> outgoingBuffers;
    ManVec<T, PinnedAllocator<T>> incommingBuffers;
#endif
#else
    ManVec<T, PinnedAllocator<T>> outgoingBuffers;
    ManVec<T, PinnedAllocator<T>> incommingBuffers;
#endif

    void setupBuffers()
    {
        outgoingSize = 0;
        incommingSize = 0;
        for (const auto &target: outgoingTargets) {
            outgoingSize += outgoingGlobalKeys[target.second].size();
        }
        for (const auto &target: incommingTargets) {
            incommingSize += incommingGlobalKeys[target.second].size();
        }

        Iterator::syncAll();

        outgoingBuffers.resize(outgoingSize * numBuffers);
        incommingBuffers.resize(incommingSize * numBuffers);

        #ifdef GPU
#ifdef ENABLE_GDR
     outgoingBuffers.resize(outgoingSize * numBuffers);
     incommingBuffers.resize(incommingSize * numBuffers);
#else
     outgoingBuffers_dev.resize(outgoingSize * numBuffers);
     incommingBuffers_dev.resize(incommingSize * numBuffers);

     outgoingBuffers.resize(outgoingSize * numBuffers);
     incommingBuffers.resize(incommingSize * numBuffers);
#endif
#else
     outgoingBuffers.resize(outgoingSize * numBuffers);
     incommingBuffers.resize(incommingSize * numBuffers);
#endif

        outRequests.resize(outgoingTargets.size());
        inRequests.resize(incommingTargets.size());
        Iterator::syncAll();

    }

    void packCommData(ManVec<T> &dataBuffer, int bufferInd) {

    }


    void unpackCommData(ManVec<T> &dataBuffer, int bufferInd) {

    }

    template<class ...Tdat, std::size_t... ints>
    DEVCALLABLE
    static void packBlock(int ind, std::tuple<Tdat...> dataBuffers, T* toPtr, pInd_t *indexes, std::integer_sequence<std::size_t, ints...>){
        pInd_t fromInd = indexes[ind];
        (( toPtr[(ind * std::tuple_size<std::tuple<Tdat...>>())+ ints] = std::get<ints>(dataBuffers)->data()[fromInd] ), ...);
    }




    template<typename... Tdat>
    void packAllCommData(std::tuple<Tdat...> dataBuffers) {
        if (outgoingIndexes.size() == 0)
            return;

    auto range = xvtxRangeStart("gather");



        auto gatherBuffer = outgoingBuffers.data();

#if defined(GPU) && !defined(ENABLE_GDR)
        gatherBuffer = outgoingBuffers_dev.data();
#endif
/*
        int max = outgoingIndexes.size();
        dim3 block = {32, 6};
        dim3 grid = {1 + (max / block.x)};

        getErrorDev((CommunicatorPackedKernels::gatherPackedKern2D<<<grid, block>>> (
            outgoingIndexes.size(), outgoingIndexes.data(), gatherBuffer, dataBuffers)));
        Iterator::syncAll();
*/

        Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int ind, CommunicatorPacked<T, numBuffers> &comm){
            packBlock(ind, dataBuffers, gatherBuffer, comm.outgoingIndexes.data(), std::make_index_sequence<sizeof...(Tdat)>());
        }, outgoingIndexes.size(), *this);


#if defined(GPU) && !defined(ENABLE_GDR)
        getErrorDevApi(devMemcpy(outgoingBuffers.data(), gatherBuffer,
            sizeof(T) * outgoingBuffers.size(), devMemcpyDefault));
#endif

        Iterator::syncAll();
        xvtxRangeEnd(range);

    }

    template<class ...Tdat, std::size_t... ints>
    DEVCALLABLE
    static void unPackBlock(int ind, std::tuple<Tdat...> dataBuffers, T* toPtr, pInd_t *indexes, std::integer_sequence<std::size_t, ints...>){
        //
        pInd_t fromInd = indexes[ind];
        ((std::get<ints>(dataBuffers)->data()[fromInd] = toPtr[(ind * std::tuple_size<std::tuple<Tdat...>>())+ ints] ), ...);

    }

    template<typename... Tdat>
    void unpackAllCommData(std::tuple<Tdat...> dataBuffers) {
        if (incommingIndexes.size() == 0)
            return;
    auto range = xvtxRangeStart("scatter");

        auto scatterBuffer = incommingBuffers.data();
#if defined(GPU) && !defined(ENABLE_GDR)
        scatterBuffer = incommingBuffers_dev.data();

        getErrorDevApi(devMemcpy(scatterBuffer, incommingBuffers.data(),
            sizeof(T)*incommingBuffers.size(), devMemcpyDefault));


#endif

/*
        int max = incommingIndexes.size();
        dim3 block = {32, 6};
        dim3 grid = {1 + (max / block.x)};

        getErrorDev((CommunicatorPackedKernels::scatterPackedKern2D<<<grid, block>>> (
            incommingIndexes.size(), incommingIndexes.data(), scatterBuffer, dataBuffers)));
        Iterator::syncAll();
*/

        Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int ind, CommunicatorPacked<T, numBuffers> &comm){
            unPackBlock(ind, dataBuffers, scatterBuffer, comm.incommingIndexes.data(), std::make_index_sequence<sizeof...(Tdat)>());
        }, incommingIndexes.size(), *this);

        xvtxRangeEnd(range);

    }

    void postRecvs() {
        MPI_Datatype type = getMPIType<T>();

        int reqIndIn = 0;
        for (const auto &target: incommingTargets) {
            //
            T *ptr = &(incommingBuffers.data()[incommingStarts[target.second] * numBuffers]);
            MPI_Irecv(ptr,
                      (incommingEnds[target.second] - incommingStarts[target.second]) * numBuffers,
                      type, target.first, tagStart, MPI_COMM_WORLD, &inRequests[reqIndIn]);
            reqIndIn++;
        }

    }

    void postSends() {
        MPI_Datatype type = getMPIType<T>();

        int reqIndOut = 0;
        for (const auto &target: outgoingTargets) {
            //
            const auto start{std::chrono::steady_clock::now()};
            T *ptr = &(outgoingBuffers.data()[outgoingStarts[target.second] * numBuffers]);

            MPI_Isend(ptr,
                      (outgoingEnds[target.second] - outgoingStarts[target.second]) * numBuffers,
                      type, target.first, tagStart, MPI_COMM_WORLD, &outRequests[reqIndOut]);
            reqIndOut++;

            const auto end{std::chrono::steady_clock::now()};
            const std::chrono::duration<double> elapsed_seconds{end - start};
        }
    }

    void syncRecvs() {
        MPI_Waitall(inRequests.size(), inRequests.data(), MPI_STATUSES_IGNORE);
    }

    void syncSends() {
        MPI_Waitall(outRequests.size(), outRequests.data(), MPI_STATUSES_IGNORE);
    }

    void invert()
    {
        //
        std::swap(outgoingTargets, incommingTargets);
        std::swap(outgoingIndexes, incommingIndexes);

        std::swap(outgoingGlobalKeys, incommingGlobalKeys);

        outgoingBuffers.resize(incommingSize);
        incommingBuffers.resize(outgoingSize);
        std::swap(outRequests, inRequests);
    }
};


#endif //PHELL_COMMUNICATORPACKED_H
