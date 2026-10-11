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

#include "Communicator.h"
#include "../World.h"
#include "DevPortabilityLayer.h"
#include "Util/MPIHelpers.h"
#include "LoggerV2/RangeCollection.h"

class TransporterV2 : public ManagedParent {

protected:
public:
    struct TransportTargetV2 {
        int targetRank;
        int count;
        int recvOffset;
        ManVec<int> indexes;
        ManVec<gKey_t> globalKeys;
    };

    int tag = 0;
    ManVec<TransportTargetV2> outTargets;
    ManVec<TransportTargetV2> inTargets;
    int
    syncTargets() {

        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        int self = 0;
        MPI_Comm_rank(MPI_COMM_WORLD, &self);

        int *outgoingNum = new int[worldSize];
        int *outgoingCount = new int[worldSize];
        int *incomingNum = new int[worldSize];
        int *incomingCount = new int[worldSize];
        for (int i = 0; i < worldSize; ++i) {
            outgoingNum[i] = 0;
            outgoingCount[i] = 0;
        }

        MPI_Request* gKeyRequests = new MPI_Request [outTargets.size()];
        for (int t = 0; t < outTargets.size(); ++t) {
            //
            MPI_Isend(outTargets[t].globalKeys.data(),
                      outTargets[t].globalKeys.size(),
                      MPI_LONG_LONG_INT, outTargets[t].targetRank, tag, MPI_COMM_WORLD, &gKeyRequests[t]);
            // communicate the rest
            outgoingNum[outTargets[t].targetRank] = 1;
            outgoingCount[outTargets[t].targetRank] = outTargets[t].globalKeys.size();
        }

        MPI_Allreduce(outgoingNum, incomingNum, worldSize, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(outgoingCount, incomingCount, worldSize, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

        inTargets.resize(incomingNum[self]);

        int offset = 0;
        for(auto &t: inTargets) {
            new(&t) TransportTargetV2{};

            //
            MPI_Status status;
            MPI_Probe(MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
            int incommingCount;
            MPI_Get_count(&status, MPI_LONG_LONG_INT, &incommingCount);

            t.targetRank = status.MPI_SOURCE;
            t.globalKeys.resize(incommingCount);

            t.count = incommingCount;

            t.recvOffset = offset;
            offset+=incommingCount;

            MPI_Recv(t.globalKeys.data(), incommingCount, MPI_LONG_LONG_INT,
                     status.MPI_SOURCE, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        MPI_Waitall(outTargets.size(), gKeyRequests, MPI_STATUSES_IGNORE);
        MPI_Barrier(MPI_COMM_WORLD);

        int ret = incomingCount[self];
        delete[] outgoingNum;
        delete[] outgoingCount;
        delete[] incomingNum;
        delete[] incomingCount;

        return ret;
    }


    template<typename CommType>
    Communicator<CommType> getCommunicator() {
        Communicator<CommType> comm;
        comm.outgoingGlobalKeys.resize(outTargets.size());
        comm.incommingGlobalKeys.resize(inTargets.size());
        int self = getLocalRank();

        int j = 0;
        for (auto &target: outTargets) {
            comm.outgoingTargets[target.targetRank] = j;

            comm.outgoingGlobalKeys[j].resize(target.globalKeys.size());
            auto *outgoingKeys = comm.outgoingGlobalKeys[j].ptr;
            // todo fix names
            auto *outgoingObjs = target.globalKeys.ptr;
            Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int i) {
                outgoingKeys[i] = outgoingObjs[i];
            }, target.count);

            j++;
        }
        j = 0;
        for (auto &target: inTargets) {
            comm.incommingTargets[target.targetRank] = j;

            comm.incommingGlobalKeys[j].resize(target.globalKeys.size());
            auto *incommingKeys = comm.incommingGlobalKeys[j].ptr;
            // todo fix names
            auto *incommingObjs = target.globalKeys.ptr;
            Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int i) {
                incommingKeys[i] = incommingObjs[i];
            }, target.count);
            j++;
        }

        Iterator::syncAll();

        return comm;
    }

    void clear() {
        inTargets.clear();
        outTargets.clear();
    }

    template<typename FieldT>
    void
    transmitFileds(int recvStartOffset, std::vector<MPI_Request> &requests, std::vector<MPI_Datatype> &gatherTypes, FieldT fieldTuple,
                   int tagStart,
                   ManVec<TransportTargetV2> &targetsOut,
                   ManVec<TransportTargetV2> &targetsIn) {
        RangeV2 r6("transmit field", 3);

        for (int t = 0; t < targetsOut.size(); ++t) {
            //
            int tagOFfset = tagStart;
            callForTupleElem(fieldTuple, [&]<typename T>(ManVec<T> *ptr) {
                // todo reconsider these types and do manual gather instead ?
                gatherTypes.push_back(MPI_Datatype());
                requests.push_back(MPI_Request());

                MPI_Type_create_indexed_block(targetsOut[t].indexes.size(), 1, targetsOut[t].indexes.data(), getMPIType<T>(),
                                              &gatherTypes.back());
                MPI_Type_commit(&gatherTypes.back());

                MPI_Isend(ptr->data(), 1, gatherTypes.back(), targetsOut[t].targetRank, tag + tagOFfset,
                          MPI_COMM_WORLD, &requests.back());

                tagOFfset++;
            });
        }
        for(auto &t: targetsIn) {

            int tagOFfset = tagStart;
            callForTupleElem(fieldTuple, [&]<typename T>(ManVec<T> *ptr) {
                MPI_Recv(&(ptr->data()[recvStartOffset + t.recvOffset]), t.count, getMPIType<T>(), t.targetRank, tag + tagOFfset,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                tagOFfset++;
            });
        }
    }

    template<typename FieldT>
    void
    transmitFiledsFull(int recvStartOffset, FieldT fieldTuple,
                   ManVec<TransportTargetV2> &targetsOut,
                   ManVec<TransportTargetV2> &targetsIn) {
        RangeV2 r6("tranmit field", 3);

        std::vector<MPI_Request> requests;
        std::vector<MPI_Datatype> gatherTypes;
        requests.reserve(targetsOut.size() * std::tuple_size<decltype(fieldTuple)>::value);
        gatherTypes.reserve(targetsOut.size() * std::tuple_size<decltype(fieldTuple)>::value);

        int tagStart = 1;

        for(auto &t: targetsIn) {
            requests.push_back(MPI_Request());

            int tagOFfset = tagStart;
            callForTupleElem(fieldTuple, [&]<typename T>(ManVec<T> *ptr) {
                MPI_Irecv(&(ptr->data()[recvStartOffset + t.recvOffset]), t.count, getMPIType<T>(), t.targetRank, tag + tagOFfset,
                         MPI_COMM_WORLD, &requests.back());
                tagOFfset++;
            });
        }

        for (int t = 0; t < targetsOut.size(); ++t) {
            //
            int tagOFfset = tagStart;
            callForTupleElem(fieldTuple, [&]<typename T>(ManVec<T> *ptr) {
                // todo reconsider these types and do manual gather instead ?
                ManVec<T> buff;

                buff.resize(targetsOut[t].indexes.size(), false);

                Iterator::gather(OffloadExec(), buff.data(), ptr->data(), targetsOut[t].indexes.data(), targetsOut[t].indexes.size());

                Iterator::syncAll();

                MPI_Send(buff.data(), targetsOut[t].indexes.size(), getMPIType<T>(), targetsOut[t].targetRank, tag + tagOFfset,
                          MPI_COMM_WORLD);

                tagOFfset++;
            });
        }

        MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
        for (int i = 0; i < gatherTypes.size(); i++) {
            MPI_Type_free(&gatherTypes[i]);
        }

    }


};

#endif //PHELL_TRANSPORTER_H
