//
// Created by Fredrik Robertsen on 28/12/2020.
//

#ifndef PHELL_COMMUNICATOR_H
#define PHELL_COMMUNICATOR_H

#include <mpi.h>

#include <ManVec.h>
#include <ParallelSort.h>

#include "../ParticleData.h"
#include "../Util/Util.h"
#include "../NeighbourGrid.h"
#include "Transporter.h"
#include "TupleHelpers.h"



class Communicator {

protected:
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

    int tagStart = 0;

    template <class gType, class lType>
    void makeLocalIndexes(GlobalLocalMapper<gType, lType> &mapper)
    {
            xvtxRangePush("makeLocalIndexes");

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

        for (const auto &target: outgoingTargets) {
            mapper.bulkGetLocal(outgoingGlobalKeys[target.second], outgoingIndexes, outgoingStarts[target.second]);
        }

        for (const auto &target: incommingTargets) {
            mapper.bulkGetLocal(incommingGlobalKeys[target.second], incommingIndexes, incommingStarts[target.second]);
        }

        Iterator::syncAll();

        MPI_Barrier(MPI_COMM_WORLD);
        xvtxRangePop();

    }

    template<class Tind>
    void indirectIndexes(ManVec<Tind> &indirectIncommingAddresses, ManVec<Tind> &indirectOutgoingAddresses){

        auto indirectOutgoingAddressesPtr = indirectOutgoingAddresses.data();
        auto outgoingIndexesPtr = outgoingIndexes.data();
        Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int i) {
            outgoingIndexesPtr[i] = indirectOutgoingAddressesPtr[outgoingIndexesPtr[i]];
        }, outgoingIndexes.size());
        auto indirectIncommingAddressesPtr = indirectIncommingAddresses.data();
        auto incommingIndexesPtr = incommingIndexes.data();
        Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int i) {
            incommingIndexesPtr[i] = indirectIncommingAddressesPtr[incommingIndexesPtr[i]];
        }, incommingIndexes.size());
        /*
        for (int i = 0; i < outgoingIndexes.size(); ++i) {
            //
            outgoingIndexes[i] = indirectOutgoingAddresses[outgoingIndexes[i]];
        }
        for (int i = 0; i < incommingIndexes.size(); ++i) {
            //
            incommingIndexes[i] = indirectIncommingAddresses[incommingIndexes[i]];
        }
        */

        Iterator::syncAll();

    }

    void initFromTransporter(TransporterV2 &transport){

        outgoingGlobalKeys.resize(transport.outTargets.size());
        incommingGlobalKeys.resize(transport.inTargets.size());
        int self = getLocalRank();

        outgoingTargets.clear();
        incommingTargets.clear();

        int j = 0;
        for (auto &target: transport.outTargets) {
            outgoingTargets[target.targetRank] = j;

            outgoingGlobalKeys[j].resize(target.count);
            auto *outgoingKeys = outgoingGlobalKeys[j].data();
            // todo fix names
            auto *outgoingObjs = target.globalKeysPtr;
            Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int i) {
                outgoingKeys[i] = outgoingObjs[i];
            }, target.count);

            j++;
        }
        j = 0;
        for (auto &target: transport.inTargets) {
            incommingTargets[target.targetRank] = j;

            incommingGlobalKeys[j].resize(target.count);
            auto *incommingKeys = incommingGlobalKeys[j].data();
            // todo fix names
            auto *incommingObjs = target.globalKeysPtr;
            Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int i) {
                incommingKeys[i] = incommingObjs[i];
            }, target.count);
            j++;
        }
        Iterator::syncAll();
    }
};

#endif //PHELL_COMMUNICATOR_H
