//
// Created by Fredrik Robertsen on 28/12/2020.
//

#ifndef PHELL_COMMUNICATORPACKED_H
#define PHELL_COMMUNICATORPACKED_H

#include <mpi.h>

#include <ManVec.h>
#include <ScratchMem.h>
#include <ParallelSort.h>

#include "../ParticleData.h"
#include "../Util/Util.h"
#include "../NeighbourGrid.h"
#include "Transporter.h"

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
}
 */

template<class T>
class CommunicatorPacked : public Communicator<T> {



public:

    std::vector<ManVec<ind_t>> outgoingGlobalKeys;
    std::vector<ManVec<ind_t>> incommingGlobalKeys;

    std::unordered_map<int, int> outgoingTargets;
    std::unordered_map<int, int> incommingTargets;

    ManVec<pInd_t> outgoingIndexes;
    ManVec<pInd_t> incommingIndexes;

    std::vector<int> incommingStarts;
    std::vector<int> incommingEnds;
    std::vector<int> outgoingStarts;
    std::vector<int> outgoingEnds;

    int outgoingSize = 0;
    int incommingSize = 0;

    std::vector<MPI_Request> outRequests;
    std::vector<MPI_Request> inRequests;

#ifdef GPU
#ifdef ENABLE_GDR
    ScratchMem<T> outgoingBuffers;
    ScratchMem<T> incommingBuffers;
#else
    ScratchMem<T> outgoingBuffers_dev;
    ScratchMem<T> incommingBuffers_dev;

    PinnedMem<T> outgoingBuffers;
    PinnedMem<T> incommingBuffers;
#endif
#else
    PinnedMem<T> outgoingBuffers;
    PinnedMem<T> incommingBuffers;
#endif

    /*
    // if GPU change to GPU memory
    ScratchMem<T> outgoingBuffers;
    ScratchMem<T> incommingBuffers;
*/
    int numBuff;

    int tagStart = 0;

    CommunicatorPacked(){
        //devStreamCreateWithFlags(&transportStream, devStreamNonBlocking);
        //devStreamCreateWithFlags(&computeStream, devStreamNonBlocking);
    }

    template <class gType, class lType>
    void makeLocalIndexes(GlobalLocalMapper<gType, lType> &mapper)
    {
        //
        MPI_Barrier(MPI_COMM_WORLD);

        outgoingSize = 0;
        incommingSize = 0;
        for (const auto &target: outgoingTargets) {
            outgoingSize += outgoingGlobalKeys[target.second].size();
        }
        for (const auto &target: incommingTargets) {
            incommingSize += incommingGlobalKeys[target.second].size();
        }
        outgoingIndexes.resize(outgoingSize, false);
        incommingIndexes.resize(incommingSize, false);
        incommingStarts.resize(incommingTargets.size());
        incommingEnds.resize(incommingTargets.size());
        outgoingStarts.resize(outgoingTargets.size());
        outgoingEnds.resize(outgoingTargets.size());

        // need to fix start and ends separately
        int i = 0;
        for (const auto &target: outgoingTargets) {
            outgoingStarts[target.second] = i;
            i+= outgoingGlobalKeys[target.second].size();
            outgoingEnds[target.second] = i;
        }
        i = 0;
        for (const auto &target: incommingTargets) {
            incommingStarts[target.second] = i;
            i+= incommingGlobalKeys[target.second].size();
            incommingEnds[target.second] = i;
        }

        i = 0;
        for (const auto &target: outgoingTargets) {


            auto ptrInd = outgoingIndexes.ptr;
            auto ptrKey = outgoingGlobalKeys[target.second].ptr;
            auto start = outgoingStarts[target.second];
            Iterator::iterate(ParallelExec(),[=] (int i, GlobalLocalMapper<gType, lType> &mapper){
                auto targetGKey = ptrKey[i];
                ind_t localIndex = mapper.getLocalInd(targetGKey);
                if(localIndex == -1) {
                    std::cout << "error local index not found outgoing " << targetGKey << std::endl;
                    exit(1);
                }
                ptrInd[start+i] = localIndex;
            }, outgoingGlobalKeys[target.second].size(), mapper);

        }
        i = 0;
        for (const auto &target: incommingTargets) {
            auto ptrInd = incommingIndexes.ptr;
            auto ptrKey = incommingGlobalKeys[target.second].ptr;
            auto start = incommingStarts[target.second];


            Iterator::iterate(ParallelExec(),[=] (int i, GlobalLocalMapper<gType, lType> &mapper){
                auto targetGKey = ptrKey[i];
                ind_t localIndex = mapper.getLocalInd(targetGKey);
                if(localIndex == -1) {
                    std::cout << "error local index not found incomming " << targetGKey << std::endl;
                    exit(1);
                }
                ptrInd[start+i] = localIndex;
            }, incommingGlobalKeys[target.second].size(), mapper);


        }
        Iterator::syncAll();

        MPI_Barrier(MPI_COMM_WORLD);

    }

    void setNumBuffers(int numBuffers)
    {
        outgoingSize = 0;
        incommingSize = 0;
        for (const auto &target: outgoingTargets) {
            outgoingSize += outgoingGlobalKeys[target.second].size();
        }
        for (const auto &target: incommingTargets) {
            incommingSize += incommingGlobalKeys[target.second].size();
        }

        numBuff = numBuffers;
        Iterator::syncAll();

        outgoingBuffers.resize(outgoingSize * numBuff);
        incommingBuffers.resize(incommingSize * numBuff);

        #ifdef GPU
#ifdef ENABLE_GDR
     outgoingBuffers.resize(outgoingSize * numBuff);
     incommingBuffers.resize(incommingSize * numBuff);
#else
     outgoingBuffers_dev.resize(outgoingSize * numBuff);
     incommingBuffers_dev.resize(incommingSize * numBuff);

     outgoingBuffers.resize(outgoingSize * numBuff);
     incommingBuffers.resize(incommingSize * numBuff);
#endif
#else
     outgoingBuffers.resize(outgoingSize * numBuff);
     incommingBuffers.resize(incommingSize * numBuff);
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
        //
        pInd_t fromInd = indexes[ind];
        (( toPtr[(ind * std::tuple_size<std::tuple<Tdat...>>())+ ints] = std::get<ints>(dataBuffers)->data()[fromInd] ), ...);

    }

    template<typename... Tdat>
    void packAllCommData(std::tuple<Tdat...> dataBuffers) {

#if defined(GPU) && !defined(ENABLE_GDR)
        Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int ind, CommunicatorPacked<T> &comm){
            packBlock(ind, dataBuffers, comm.outgoingBuffers_dev.data(), comm.outgoingIndexes.data(), std::make_index_sequence<sizeof...(Tdat)>());
        }, outgoingIndexes.size(), *this);
        getErrorDev(devMemcpy(outgoingBuffers.data(), outgoingBuffers_dev.data(),
                              sizeof(T) * outgoingBuffers.size(), devMemcpyDefault));
#else
        Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int ind, CommunicatorPacked<T> &comm){
            packBlock(ind, dataBuffers, comm.outgoingBuffers.data(), comm.outgoingIndexes.data(), std::make_index_sequence<sizeof...(Tdat)>());
        }, outgoingIndexes.size(), *this);
#endif
        Iterator::syncAll();
        /*
#ifdef GPU
        auto [block, grid] = Iterator::IteratorGPU::getGrid(outgoingIndexes.size(), 128);
        getErrorDev(((CommunicatorPackedKernels::gatherPackedKern<<<grid, block,0,stream>>>(
                            outgoingIndexes.size(), outgoingIndexes.data(), outgoingBuffers_dev.data(),
                            std::get<0>(dataBuffers)->data(),
                            std::get<1>(dataBuffers)->data(),
                            std::get<2>(dataBuffers)->data(),
                            std::get<3>(dataBuffers)->data(),
                            std::get<4>(dataBuffers)->data(),
                            std::get<5>(dataBuffers)->data()
                    ))));
#else
#endif
*/
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

#if defined(GPU) && !defined(ENABLE_GDR)
        getErrorDev(devMemcpy(incommingBuffers_dev.data(), incommingBuffers.data(),
                                    sizeof(T)*incommingBuffers.size(), devMemcpyDefault));

        Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int ind, CommunicatorPacked<T> &comm){
            unPackBlock(ind, dataBuffers, comm.incommingBuffers_dev.data(), comm.incommingIndexes.data(), std::make_index_sequence<sizeof...(Tdat)>());
        }, incommingIndexes.size(), *this);
#else
        Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int ind, CommunicatorPacked<T> &comm){
            unPackBlock(ind, dataBuffers, comm.incommingBuffers.data(), comm.incommingIndexes.data(), std::make_index_sequence<sizeof...(Tdat)>());
        }, incommingIndexes.size(), *this);
#endif

        Iterator::syncAll();
        /*
#ifdef GPU
        auto [block, grid] = Iterator::IteratorGPU::getGrid(incommingIndexes.size(), 128);
        getErrorDev(((CommunicatorPackedKernels::scatterPackedKern<<<grid, block,0,stream>>>(
                            incommingIndexes.size(), incommingIndexes.data(), incommingBuffers_dev.data(),
                            std::get<5>(dataBuffers)->data(),
                            std::get<4>(dataBuffers)->data(),
                            std::get<3>(dataBuffers)->data(),
                            std::get<2>(dataBuffers)->data(),
                            std::get<1>(dataBuffers)->data(),
                            std::get<0>(dataBuffers)->data()
                    ))));
#else
#endif
         */
    }

    void postRecvs() {
        int scale = 1;
        MPI_Datatype type;

        if (std::is_same<T, double>::value) {
            type = MPI_DOUBLE;
        } else if (std::is_same<T, bool>::value) {
            type = MPI_BYTE;
        } else if (std::is_same<T, float>::value) {
            type = MPI_FLOAT;
        } else {
            std::cout << "non implemented communicator " << std::endl;
            exit(0);
        }

        int reqIndIn = 0;
        for (const auto &target: incommingTargets) {
            //
            T *ptr = &(incommingBuffers.data()[incommingStarts[target.second] * numBuff]);
            MPI_Irecv(ptr,
                      (incommingEnds[target.second] - incommingStarts[target.second]) * scale * numBuff,
                      type, target.first, tagStart, MPI_COMM_WORLD, &inRequests[reqIndIn]);
            reqIndIn++;
        }

    }

    void postSends() {
        int scale = 1;
        MPI_Datatype type;

        if (std::is_same<T, double>::value) {
            type = MPI_DOUBLE;
        } else if (std::is_same<T, bool>::value) {
            type = MPI_BYTE;
        } else if (std::is_same<T, float>::value) {
            type = MPI_FLOAT;
        } else {
            std::cout << "non implemented communicator " << std::endl;
            exit(0);
        }

        int reqIndOut = 0;
        for (const auto &target: outgoingTargets) {
            //
            const auto start{std::chrono::steady_clock::now()};
            T *ptr = &(outgoingBuffers.data()[outgoingStarts[target.second] * numBuff]);

            MPI_Isend(ptr,
                      (outgoingEnds[target.second] - outgoingStarts[target.second]) * scale * numBuff,
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

        std::swap(outgoingBuffers, incommingBuffers);
        std::swap(outRequests, inRequests);
    }

    template<class Tind>
    void indirectIndexes(ManVec<Tind> &indirectIncommingAddresses, ManVec<Tind> &indirectOutgoingAddresses){
        for (int i = 0; i < outgoingIndexes.size(); ++i) {
            //
            outgoingIndexes[i] = indirectOutgoingAddresses[outgoingIndexes[i]];
        }
        for (int i = 0; i < incommingIndexes.size(); ++i) {
            //
            incommingIndexes[i] = indirectIncommingAddresses[incommingIndexes[i]];
        }

    }

    void initFromTransporter(TransporterV2 &transport){

        outgoingGlobalKeys.resize(transport.outTargets.size());
        incommingGlobalKeys.resize(transport.inTargets.size());
        int self = getLocalRank();

        int j = 0;
        for (auto &target: transport.outTargets) {
            outgoingTargets[target.targetRank] = j;

            outgoingGlobalKeys[j].resize(target.globalKeys.size());
            auto *outgoingKeys = outgoingGlobalKeys[j].ptr;
            // todo fix names
            auto *outgoingObjs = target.globalKeys.ptr;
            Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int i) {
                outgoingKeys[i] = outgoingObjs[i];
            }, target.count);

            j++;
        }
        j = 0;
        for (auto &target: transport.inTargets) {
            incommingTargets[target.targetRank] = j;

            incommingGlobalKeys[j].resize(target.globalKeys.size());
            auto *incommingKeys = incommingGlobalKeys[j].ptr;
            // todo fix names
            auto *incommingObjs = target.globalKeys.ptr;
            Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int i) {
                incommingKeys[i] = incommingObjs[i];
            }, target.count);
            j++;
        }

        Iterator::syncAll();

    }

};


#endif //PHELL_COMMUNICATORPACKED_H
