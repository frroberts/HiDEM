//
// Created by Fredrik Robertsen on 03/01/2021.
//

#ifndef PHELL_TRANSPORTER_H
#define PHELL_TRANSPORTER_H

#include <vector>
#include <fstream>
#include <mpi.h>
#include <chrono>

#include <ManVec.h>

#include "../World.h"
#include "DevPortabilityLayer.h"
#include "Util/MPIHelpers.h"
#include "LoggerV2/RangeCollection.h"

class TransportTargetV2 {
public:
    int targetRank;
    int count;
    int* indexesPtr;
    gKey_t* globalKeysPtr;
    int offset;

#if defined(GPU) && !defined(ENABLE_GDR)
    gKey_t* globalKeysPtr_host;

    // ManVec<gKey_t, PinnedAllocator<gKey_t>> globalKeys_host;
#endif

};


class TransporterV2 : public ManagedParent {

protected:



    ManVec<int> indexesOut;
    ManVec<int> indexesIn;
    ManVec<gKey_t> globalKeysOut;
    ManVec<gKey_t> globalKeysIn;
#if defined(GPU) && !defined(ENABLE_GDR)
    ManVec<gKey_t, PinnedAllocator<gKey_t>> globalKeysOut_host;
    ManVec<gKey_t, PinnedAllocator<gKey_t>> globalKeysIn_host;
#endif

public:

    int tag = 0;
    std::vector<TransportTargetV2> outTargets;
    std::vector<TransportTargetV2> inTargets;

    int syncTargets() {
        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        int self = 0;
        MPI_Comm_rank(MPI_COMM_WORLD, &self);

        std::vector<int> numIncoming;
        std::vector<int> numOutgoing;

        numIncoming.resize(worldSize);
        numOutgoing.resize(worldSize);
        for (int i = 0; i < worldSize; ++i) {
            numIncoming[i] = 0;
            numOutgoing[i] = 0;
        }

        for (int t = 0; t < outTargets.size(); ++t) {
            numOutgoing[outTargets[t].targetRank] = outTargets[t].count;
        }

        MPI_Alltoall(numOutgoing.data(), 1, MPI_INT, numIncoming.data(), 1,
            MPI_INT, MPI_COMM_WORLD);
        MPI_Request *gKeyRequests = new MPI_Request [outTargets.size()];

        inTargets.clear();

        int offset = 0;
        for (int t = 0; t < numIncoming.size(); ++t) {
            if (numIncoming[t] > 0) {
                inTargets.push_back(TransportTargetV2());

                int incomingCount = numIncoming[t];
                inTargets.back().offset = offset;

                inTargets.back().targetRank =  t;
                inTargets.back().count = incomingCount;
                offset+=incomingCount;
            }
        }

        indexesIn.resize(offset);
        globalKeysIn.resize(offset);

        for (auto &t: inTargets) {
            t.globalKeysPtr = globalKeysIn.data() + t.offset;
            t.indexesPtr = indexesIn.data() + t.offset;
        }

#if defined(GPU) && !defined(ENABLE_GDR)
        globalKeysIn_host.resize(globalKeysIn.size());
        globalKeysOut_host.resize(globalKeysOut.size());
#endif

        int i = 0;
        for (auto &t: outTargets) {
#if defined(GPU) && !defined(ENABLE_GDR)
            t.globalKeysPtr_host = globalKeysOut_host.data() + t.offset;
            getErrorDevApi(devMemcpy(t.globalKeysPtr_host, t.globalKeysPtr,
                sizeof(gKey_t)*t.count, devMemcpyDefault));
            Iterator::syncAll();
            MPI_Isend(t.globalKeysPtr_host, t.count,
                      MPI_LONG_LONG_INT, t.targetRank, tag, MPI_COMM_WORLD, &gKeyRequests[i]);
#else
            MPI_Isend(t.globalKeysPtr, t.count,
                      MPI_LONG_LONG_INT, t.targetRank, tag, MPI_COMM_WORLD, &gKeyRequests[i]);
#endif
            i++;
        }

        for(auto &t: inTargets) {
#if defined(GPU) && !defined(ENABLE_GDR)
            t.globalKeysPtr_host = globalKeysIn_host.data() + t.offset;
            MPI_Recv(t.globalKeysPtr_host, t.count, MPI_LONG_LONG_INT, t.targetRank, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            getErrorDevApi(devMemcpy(t.globalKeysPtr, t.globalKeysPtr_host, sizeof(gKey_t)*t.count, devMemcpyDefault));
#else
                MPI_Recv(t.globalKeysPtr, t.count, MPI_LONG_LONG_INT,t.targetRank, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#endif
        }
        for (int t = 0; t < outTargets.size(); ++t) {
            //

            if (outTargets[t].count > 0) {
                MPI_Wait(&gKeyRequests[t], MPI_STATUSES_IGNORE);
            }
        }
        MPI_Barrier(MPI_COMM_WORLD);

        int ret = offset;

        delete[] gKeyRequests;

        return ret;
    }

    void clear() {
        inTargets.clear();
        outTargets.clear();
    }

    template<typename FieldT>
    void transmitFiledsFull(int recvStartOffset, FieldT fieldTuple,
                            std::vector<TransportTargetV2> &targetsOut,
                            std::vector<TransportTargetV2> &targetsIn) {
        xvtxRangePush("tranmit field");
        RangeV2 r6("tranmit field", 3);

        std::vector<MPI_Request> requests;
        requests.reserve(targetsOut.size() * std::tuple_size<decltype(fieldTuple)>::value);

        int tagStart = 1;

        auto inPtrs = vecPtrToDataPtr(fieldTuple);
        int bufferSize = 0;

        for (auto &t: targetsIn) {
            bufferSize += t.count;
        }


        callForTupleElemRef(inPtrs, [&]<typename T>(T *&ptr) {
            ptr = new T[bufferSize];
        });

        for (auto &t: targetsIn) {

            // vecPtrToDataPtr
            if (t.count == 0)
                continue;
            requests.push_back(MPI_Request());

            int tagOFfset = tagStart;
            callForTupleElem(inPtrs, [&]<typename T>(T *ptr) {
                MPI_Irecv(&(ptr[t.offset]), t.count, getMPIType<T>(), t.targetRank, tag + tagOFfset,
                          MPI_COMM_WORLD, &requests.back());
                tagOFfset++;
            });
        }

        for (int t = 0; t < targetsOut.size(); ++t) {
            if (outTargets[t].count == 0)
                continue;

            //
            int tagOFfset = tagStart;
            callForTupleElem(fieldTuple, [&]<typename T>(ManVec<T> *ptr) {
#if defined(GPU) // && !defined(ENABLE_GDR) // remove this when cray mpich no longer leaks memory
                ManVec<T, PinnedAllocator<T>> hostBuff;
                ManVec<T, ScratchAllocator<T>> devBuff;

                hostBuff.resize(targetsOut[t].count, false);
                devBuff.resize(targetsOut[t].count, false);

                Iterator::gather(OffloadExec(), devBuff.data(), ptr->data(), targetsOut[t].indexesPtr, targetsOut[t].count);

                Iterator::syncAll();

                getErrorDevApi(devMemcpy(hostBuff.data(), devBuff.data(), hostBuff.size()*sizeof(T), devMemcpyDefault));

                Iterator::syncAll();

                MPI_Send(hostBuff.data(), targetsOut[t].count, getMPIType<T>(), targetsOut[t].targetRank, tag + tagOFfset,
                         MPI_COMM_WORLD);

#else
                ManVec<T, PinnedAllocator<T>> hostBuff;
                hostBuff.resize(targetsOut[t].count, false);
                Iterator::gather(OffloadExec(), hostBuff.data(), ptr->data(), targetsOut[t].indexesPtr, targetsOut[t].count);

                Iterator::syncAll();

                MPI_Send(hostBuff.data(), targetsOut[t].count, getMPIType<T>(), targetsOut[t].targetRank, tag + tagOFfset,
                         MPI_COMM_WORLD);

#endif
                tagOFfset++;
            });
        }


        MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);

        callForTuple2Elem(fieldTuple, inPtrs, [&]<typename T>(ManVec<T> *outPtr, T *buffPtr) {
            Iterator::copy(&(outPtr->data()[recvStartOffset]), buffPtr, bufferSize * sizeof(T));
        });

        callForTupleElem(inPtrs, [&]<typename T>(T *&ptr) {
            delete[] ptr;
        });

        xvtxRangePop();

    }


};

#endif //PHELL_TRANSPORTER_H
