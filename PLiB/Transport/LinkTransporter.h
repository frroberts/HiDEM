//
// Created by Fredrik Robertsen on 08/07/2021.
//

#ifndef PHELL_LINKTRANSPORTER_H
#define PHELL_LINKTRANSPORTER_H

#include "LinkData.h"
#include "Transporter.h"

template <class ParticleDataT, class LinkDataT>
class LinkTransporterV2 : public TransporterV2 {

public:
    LinkTransporterV2() {
        tag = 2000;
    }

    void getIncomming(LinkDataT &lData) {

        int incomingCount = syncTargets();

        int oldSize = lData.numLocal;
        int newSize = oldSize + incomingCount;
        lData.numLocal = newSize;

        lData.resize(newSize);
        lData.resizeSideData(newSize*2);

        ManVec<TransportTargetV2> sideOutTargets;
        ManVec<TransportTargetV2> sideInTargets;
        sideOutTargets.resize(outTargets.size());
        sideInTargets.resize(inTargets.size());
        for(int i = 0; i < outTargets.size(); i++){
            new(&sideOutTargets[i]) TransportTargetV2{};

            sideOutTargets[i].targetRank = outTargets[i].targetRank;
            sideOutTargets[i].count = outTargets[i].count;
            sideOutTargets[i].count *= 2;
            sideOutTargets[i].indexes.clear();
            for(auto &ind: outTargets[i].indexes){
                sideOutTargets[i].indexes.push_back(lData.sideIndEndA[ind]);
                sideOutTargets[i].indexes.push_back(lData.sideIndEndB[ind]);
            }
        }

        int offset = 0;
        for(int i = 0; i < inTargets.size(); i++) {
            new(&sideInTargets[i]) TransportTargetV2{};

            sideInTargets[i].count = 2*inTargets[i].count;
            sideInTargets[i].targetRank = inTargets[i].targetRank;
            sideInTargets[i].recvOffset = inTargets[i].recvOffset*2;
            offset+=inTargets[i].count;
        }
/*
        int tupleCombinedSize = std::tuple_size<decltype(lData.getFieldTupleInput())>::value +
                                std::tuple_size<decltype(lData.getFieldTupleSideInput())>::value;
        std::vector<MPI_Request> requests;
        std::vector<MPI_Datatype> gatherTypes;
        requests.reserve(outTargets.size() * (tupleCombinedSize));
        gatherTypes.reserve(outTargets.size() * (tupleCombinedSize));
*/
        transmitFiledsFull(oldSize, lData.getFieldTupleInput(), outTargets, inTargets);
        transmitFiledsFull(oldSize*2, lData.getFieldTupleSideInput(), sideOutTargets, sideInTargets);

/*
        MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
        for(int i = 0; i < gatherTypes.size(); i++){
            MPI_Type_free(&gatherTypes[i]);
        }
*/
        for(int i = oldSize; i < newSize; i++){
            lData.sideIndEndA[i] = i*2;
            lData.sideIndEndB[i] = (i*2)+1;
            lData.localPartIndA[i] = -1;
            lData.localPartIndB[i] = -1;
        }

        generateOwnedNonOwned(lData.owners, lData.ownedIndexes, lData.nonOwnedIndexes, lData.numLocal);
    }

    template<class Fun>
    void packOutgoingGetRank(World &world, ParticleDataT &pData, LinkDataT &lData, Fun getTargetRank, bool reassign) {
        //
        clear();

        outTargets.resize(world.domains.size());

        int numDomains = world.domains.size();

        for (int i = 0; i < world.domains.size(); ++i) {
            new(&outTargets[i]) TransportTargetV2{};

            outTargets[i].count = 0;
            outTargets[i].targetRank = i;//world.domains[i].owner;
        }

        Iterator::syncAll();

        lData.iterateOwnedLinks(OffloadExec(), [=] DEVCALLABLE (lInd_t lInd, pInd_t aParticleInd, pInd_t bParticleInd,
                                                                LinkDataT &lData, ParticleDataT &pData, LinkTransporterV2<ParticleDataT, LinkDataT> &transporter) {
            if (lData.state[lInd] == 0)
                return;
            if(reassign){
                lData.owners[lInd] = pData.owners[aParticleInd];
            }
            int targetRank = getTargetRank(aParticleInd, bParticleInd, lInd, pData, lData);
            if (targetRank != -1) {
                Iterator::atomicIncVal(transporter.outTargets[targetRank].count);
            }
        }, lData, pData, *this);

        Iterator::syncAll();

        for (int i = 0; i < world.domains.size(); ++i) {
            outTargets[i].indexes.resize(outTargets[i].count);
            outTargets[i].globalKeys.resize(outTargets[i].count);

            outTargets[i].count = 0;
        }

        Iterator::syncAll();
        lData.iterateOwnedLinks(OffloadExec(), [=] DEVCALLABLE (lInd_t lInd, pInd_t aParticleInd, pInd_t bParticleInd,
                                                                LinkDataT &lData, ParticleDataT &pData, LinkTransporterV2<ParticleDataT, LinkDataT> &transporter) {
            if (lData.state[lInd] == 0)
                return;
            int targetRank = getTargetRank(aParticleInd, bParticleInd, lInd, pData, lData);;
            if (targetRank != -1) {
                // add to outgoing object
                int place = Iterator::atomicIncVal(transporter.outTargets[targetRank].count);

                transporter.outTargets[targetRank].indexes[place] = lInd;
                transporter.outTargets[targetRank].globalKeys[place] = lData.globalKeys[lInd];

            }
        }, lData, pData, *this);

        Iterator::syncAll();
        // drop empty outgoing objects
        /*
        outTargets.erase(std::remove_if(outTargets.begin(),
                                              outTargets.end(),
                                              [](auto &x) { return x.count == 0; }), outTargets.end());
*/
    }

    void getOutgoingHalo(World &world, ParticleDataT &pData, LinkDataT &lData) {
        int self = world.self;

        packOutgoingGetRank(world, pData, lData,
                            [=] DEVCALLABLE (pInd_t aParticleInd, pInd_t bParticleInd, lInd_t lInd, ParticleData &pData, LinkData &lData) {
                                int targetRank = -1;

                                if (pData.owners[aParticleInd] != self) {
                                    targetRank = pData.owners[aParticleInd];
                                } else if (pData.owners[bParticleInd] != self) {
                                    targetRank = pData.owners[bParticleInd];
                                }
                                return targetRank;
                            }, false);
    }

    void reassignAndGetOutgoing(World &world, ParticleDataT &pData, LinkDataT &lData) {
        int self = world.self;

        packOutgoingGetRank(world, pData, lData,
                            [=] DEVCALLABLE (pInd_t aParticleInd, pInd_t bParticleInd, lInd_t lInd, ParticleData &pData, LinkData &lData) {
                                int targetRank = lData.owners[lInd];
                                if (targetRank != self && lData.state[lInd] != 0) {
                                    return targetRank;
                                } else {
                                    return -1;
                                }
                            }, true);

        lData.generateOwnedLinksList();
    }
};

#endif //PHELL_LINKTRANSPORTER_H
