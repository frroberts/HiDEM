//
// Created by Fredrik Robertsen on 28/12/2020.
//

#ifndef PHELL_COMMUNICATOR_H
#define PHELL_COMMUNICATOR_H

#include <mpi.h>

#include <ManVec.h>
#include <ScratchMem.h>
#include <ParallelSort.h>

#include "../ParticleData.h"
#include "../Util/Util.h"
#include "../NeighbourGrid.h"

template<class T>
class Communicator {

public:

    std::vector<ManVec<ind_t>> outgoingGlobalKeys;
    std::vector<ManVec<ind_t>> incommingGlobalKeys;

    std::unordered_map<int, int> outgoingTargets;
    std::unordered_map<int, int> incommingTargets;

    ManVec<pInd_t> outgoingIndexes;
    ManVec<pInd_t> incommingIndexes;

//#define ENABLE_GDR
#ifdef GPU
#ifdef ENABLE_GDR
    std::vector<ScratchMem<T>> outgoingBuffers;
    std::vector<ScratchMem<T>> incommingBuffers;
#else
    std::vector<ScratchMem<T>> outgoingBuffers_dev;
    std::vector<ScratchMem<T>> incommingBuffers_dev;

    std::vector<PinnedMem<T>> outgoingBuffers;
    std::vector<PinnedMem<T>> incommingBuffers;
#endif
#else
    std::vector<ManVec<T>> outgoingBuffers;
    std::vector<ManVec<T>> incommingBuffers;
#endif
    std::vector<int> incommingStarts;
    std::vector<int> incommingEnds;
    std::vector<int> outgoingStarts;
    std::vector<int> outgoingEnds;

    int outgoingSize = 0;
    int incommingSize = 0;



    std::vector<std::vector<MPI_Request>> outRequests;
    std::vector<std::vector<MPI_Request>> inRequests;

    int numBuff;

    int tagStart = 0;

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
        numBuff = numBuffers;

        for (int i = 0; i < numBuff; ++i) {
            outRequests[i].resize(outgoingTargets.size());
            inRequests[i].resize(incommingTargets.size());
        }

    }
    void packCommData(ManVec<T> &dataBuffer, int bufferInd) {
#if defined(GPU) && !defined(ENABLE_GDR)
        Iterator::gather(OffloadExec(), outgoingBuffers_dev[bufferInd].data(), dataBuffer.data(),
                 outgoingIndexes.data(), outgoingIndexes.size());
            getErrorDev(devMemcpy(outgoingBuffers[bufferInd].data(), outgoingBuffers_dev[bufferInd].data(),
                                    sizeof(T)*outgoingBuffers[bufferInd].size(), devMemcpyDefault));

#else
        Iterator::gather(OffloadExec(), outgoingBuffers[bufferInd].data(), dataBuffer.data(),
                         outgoingIndexes.data(), outgoingIndexes.size());
#endif
    }


    void unpackCommData(ManVec<T> &dataBuffer, int bufferInd) {
#if defined(GPU) && !defined(ENABLE_GDR)
            getErrorDev(devMemcpyAsync(incommingBuffers_dev[bufferInd].data(), incommingBuffers[bufferInd].data(),
                                    sizeof(T)*incommingBuffers[bufferInd].size(), devMemcpyDefault));

        Iterator::scatter(OffloadExec(), dataBuffer.data(), incommingBuffers_dev[bufferInd].data(),
                  incommingIndexes.data(), incommingIndexes.size());

#else
        Iterator::scatter(OffloadExec(), dataBuffer.data(), incommingBuffers[bufferInd].data(),
                          incommingIndexes.data(), incommingIndexes.size());
#endif
    }

    void postRecvs()
    {
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



        for (int i = 0; i < numBuff; ++i) {
            int reqIndIn = 0;
            for (auto target: incommingTargets) {
                //
                T* ptr = &(incommingBuffers[i].data()[incommingStarts[target.second]]);
                MPI_Irecv(ptr,
                          (incommingEnds[target.second] - incommingStarts[target.second]) * scale,
                    type, target.first, i+tagStart, MPI_COMM_WORLD, &inRequests[i][reqIndIn]);
                reqIndIn++;
            }
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


        for (int i = 0; i < numBuff; ++i) {
            int reqIndOut = 0;
            for (auto target: outgoingTargets) {
                //
                const auto start{std::chrono::steady_clock::now()};
                T *ptr = &(outgoingBuffers[i].data()[outgoingStarts[target.second]]);

                MPI_Isend(ptr,
                          (outgoingEnds[target.second] - outgoingStarts[target.second]) * scale,
                          type, target.first, i + tagStart, MPI_COMM_WORLD, &outRequests[i][reqIndOut]);
                reqIndOut++;

                const auto end{std::chrono::steady_clock::now()};
                const std::chrono::duration<double> elapsed_seconds{end - start};
            }
        }

    }

    void syncRecvs()
    {
        //
        for (int i = 0; i < numBuff; i++){
            MPI_Waitall(inRequests[i].size(), inRequests[i].data(), MPI_STATUSES_IGNORE);

        }
    }

    void syncSends()
    {
        //
        for (int i = 0; i < numBuff; i++) {
            MPI_Waitall(outRequests[i].size(), outRequests[i].data(), MPI_STATUSES_IGNORE);
        }
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

};


#endif //PHELL_COMMUNICATOR_H
