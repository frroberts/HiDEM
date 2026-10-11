//
// Created by Fredrik Robertsen on 28/12/2020.
//

#ifndef PHELL_COMMUNICATORINDIVIDUAL_H
#define PHELL_COMMUNICATORINDIVIDUAL_H

#include <mpi.h>

#include <ManVec.h>

#include "Communicator.h"


template<class T, int numBuffers>
class CommunicatorIndividual : public Communicator {

public:


#ifdef GPU
#ifdef ENABLE_GDR
    std::vector<ManVec<T, ScratchAllocator<T>>> outgoingBuffers;
    std::vector<ManVec<T, ScratchAllocator<T>>> incommingBuffers;
#else
    std::vector<ManVec<T, ScratchAllocator<T>>> outgoingBuffers_dev;
    std::vector<ManVec<T, ScratchAllocator<T>>> incommingBuffers_dev;

    std::vector<ManVec<T, PinnedAllocator<T>>> outgoingBuffers;
    std::vector<ManVec<T, PinnedAllocator<T>>> incommingBuffers;
#endif
#else
    std::vector<ManVec<T>> outgoingBuffers;
    std::vector<ManVec<T>> incommingBuffers;
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

        outgoingBuffers.resize(numBuffers);
        incommingBuffers.resize(numBuffers);

        outRequests.resize(numBuffers);
        inRequests.resize(numBuffers);

#if defined(GPU) && !defined(ENABLE_GDR)
        outgoingBuffers_dev.resize(numBuffers);
        incommingBuffers_dev.resize(numBuffers);
#endif
        for (int i = 0; i < numBuffers; ++i) {
            outgoingBuffers[i].resize(outgoingSize);
            incommingBuffers[i].resize(incommingSize);
#if defined(GPU) && !defined(ENABLE_GDR)
            outgoingBuffers_dev[i].resize(outgoingSize);
            incommingBuffers_dev[i].resize(incommingSize);
#endif
        }

        outRequests.resize(outgoingTargets.size() * numBuffers);
        inRequests.resize(incommingTargets.size() * numBuffers);
    }

    void packCommData(ManVec<T> &dataBuffer, int bufferInd) {
#if defined(GPU) && !defined(ENABLE_GDR)
        Iterator::gather(OffloadExec(), outgoingBuffers_dev[bufferInd].data(), dataBuffer.data(),
                 outgoingIndexes.data(), outgoingIndexes.size());
            getErrorDevApi(devMemcpy(outgoingBuffers[bufferInd].data(), outgoingBuffers_dev[bufferInd].data(),
                                    sizeof(T)*outgoingBuffers[bufferInd].size(), devMemcpyDefault));

#else
        Iterator::gather(OffloadExec(), outgoingBuffers[bufferInd].data(), dataBuffer.data(),
                         outgoingIndexes.data(), outgoingIndexes.size());
#endif
    }


    void unpackCommData(ManVec<T> &dataBuffer, int bufferInd) {
#if defined(GPU) && !defined(ENABLE_GDR)
            getErrorDevApi(devMemcpy(incommingBuffers_dev[bufferInd].data(), incommingBuffers[bufferInd].data(),
                                    sizeof(T)*incommingBuffers[bufferInd].size(), devMemcpyDefault));

        Iterator::scatter(OffloadExec(), dataBuffer.data(), incommingBuffers_dev[bufferInd].data(),
                  incommingIndexes.data(), incommingIndexes.size());

#else
        Iterator::scatter(OffloadExec(), dataBuffer.data(), incommingBuffers[bufferInd].data(),
                          incommingIndexes.data(), incommingIndexes.size());
#endif
    }

// todo update these to be better
    template<typename... Tdat>
    void packAllCommData(std::tuple<Tdat...> dataBuffers) {
#if defined(GPU) && !defined(ENABLE_GDR)
        auto commBufferTup = [&]<std::size_t... ints>(std::integer_sequence<std::size_t, ints...>) {
            return std::make_tuple(outgoingBuffers_dev[ints].data()...);
        }(std::make_index_sequence<numBuffers>{});
#else

        auto commBufferTup = [&]<std::size_t... ints>(std::integer_sequence<std::size_t, ints...>) {
            return std::make_tuple(outgoingBuffers[ints].data()...);
        }(std::make_index_sequence<numBuffers>{});

#endif

        auto dataBufferTup = [&]<std::size_t... ints>(std::integer_sequence<std::size_t, ints...>) {
            return std::make_tuple(std::get<ints>(dataBuffers)->data()...);
        }(std::make_index_sequence<numBuffers>{});

        auto combinedTup = zipTuples(commBufferTup, dataBufferTup);

        [&]<std::size_t... ints>(std::integer_sequence<std::size_t, ints...>) {
            Iterator::gatherMulti(OffloadExec(), outgoingIndexes.size(), outgoingIndexes.data(), std::get<ints>(combinedTup)...);
        }(std::make_index_sequence<numBuffers * 2>{});

        Iterator::syncAll();

#if defined(GPU) && !defined(ENABLE_GDR)
        for (int i = 0; i < outgoingBuffers.size(); ++i) {
            getErrorDevApi(devMemcpyAsync(outgoingBuffers[i].data(), outgoingBuffers_dev[i].data(),
                sizeof(lpReal_t)*outgoingBuffers[i].size(), devMemcpyDefault));
        }
#endif
        Iterator::syncAll();

    }

    template<typename... Tdat>
    void unpackAllCommData(std::tuple<Tdat...> dataBuffers) {
#if defined(GPU) && !defined(ENABLE_GDR)
        for (int i = 0; i < incommingBuffers.size(); ++i) {
            getErrorDevApi(devMemcpyAsync(incommingBuffers_dev[i].data(), incommingBuffers[i].data(),
                sizeof(lpReal_t)*incommingBuffers[i].size(), devMemcpyDefault));
        }
        Iterator::syncAll();

        auto commBufferTup = [&]<std::size_t... ints>(std::integer_sequence<std::size_t, ints...>) {
            return std::make_tuple(incommingBuffers_dev[ints].data()...);
        }(std::make_index_sequence<numBuffers>{});

#else

        auto commBufferTup = [&]<std::size_t... ints>(std::integer_sequence<std::size_t, ints...>) {
            return std::make_tuple(incommingBuffers[ints].data()...);
        }(std::make_index_sequence<numBuffers>{});

#endif

        auto dataBufferTup = [&]<std::size_t... ints>(std::integer_sequence<std::size_t, ints...>) {
            return std::make_tuple(std::get<ints>(dataBuffers)->data()...);
        }(std::make_index_sequence<numBuffers>{});

        auto combinedTup = zipTuples(dataBufferTup, commBufferTup);

        [&]<std::size_t... ints>(std::integer_sequence<std::size_t, ints...>) {
            Iterator::scatterMulti(OffloadExec(), incommingIndexes.size(), incommingIndexes.data(), std::get<ints>(combinedTup)...);
        }(std::make_index_sequence<numBuffers * 2>{});

        Iterator::syncAll();
    }

    void postRecvs()
    {
        MPI_Datatype type = getMPIType<T>();

        int reqIndIn = 0;
        for (int i = 0; i < numBuffers; ++i) {
            for (auto target: incommingTargets) {
                //
                T *ptr = &(incommingBuffers[i].data()[incommingStarts[target.second]]);
                MPI_Irecv(ptr,
                          (incommingEnds[target.second] - incommingStarts[target.second]),
                          type, target.first, i + tagStart, MPI_COMM_WORLD, &inRequests[reqIndIn]);
                reqIndIn++;
            }
        }
    }

    void postSends() {
        MPI_Datatype type = getMPIType<T>();

        int reqIndOut = 0;
        for (int i = 0; i < numBuffers; ++i) {
            for (auto target: outgoingTargets) {
                //
                T *ptr = &(outgoingBuffers[i].data()[outgoingStarts[target.second]]);

                MPI_Send(ptr,
                          (outgoingEnds[target.second] - outgoingStarts[target.second]),
                          type, target.first, i + tagStart, MPI_COMM_WORLD);
                reqIndOut++;
            }
        }
    }

    void syncRecvs() {
        //
        MPI_Waitall(inRequests.size(), inRequests.data(), MPI_STATUSES_IGNORE);
    }

    void syncSends() {
        //
        /*
            MPI_Waitall(outRequests.size(), outRequests.data(), MPI_STATUSES_IGNORE);
        */
    }

    void invert()
    {
        //
        std::swap(outgoingTargets, incommingTargets);
        std::swap(outgoingIndexes, incommingIndexes);

        std::swap(outgoingGlobalKeys, incommingGlobalKeys);

        std::swap(outgoingBuffers, incommingBuffers);
        #if defined(GPU) && !defined(ENABLE_GDR)
        std::swap(outgoingBuffers_dev, incommingBuffers_dev);
        #endif
        std::swap(outRequests, inRequests);
    }

};



#endif //PHELL_COMMUNICATORINDIVIDUAL_H
