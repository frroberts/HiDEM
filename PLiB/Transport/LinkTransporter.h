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

    ManVec<lInd_t> sideIndexesOut;
    ManVec<lInd_t> sideIndexesIn;

    bool keepInactive = true;

    LinkTransporterV2() {
        tag = 2000;
    }

    ~LinkTransporterV2() {

    }

    void getIncomming(LinkDataT &lData) {

        int incomingCount = syncTargets();

        int oldSize = lData.numLocal;
        int newSize = oldSize + incomingCount;

        lData.resize(newSize);
        lData.resizeSideData(newSize*2);

        std::vector<TransportTargetV2> sideOutTargets;
        std::vector<TransportTargetV2> sideInTargets;
        sideOutTargets.resize(outTargets.size());
        sideInTargets.resize(inTargets.size());
        int sideSize = 0;
        for (auto &t: outTargets) {
            sideSize += t.count;
        }
        sideIndexesOut.resize(sideSize * 2);
        sideSize = 0;
        for (auto &t: inTargets) {
            sideSize += t.count;
        }
        sideIndexesIn.resize(sideSize * 2);

        for (int i = 0; i < outTargets.size(); i++) {
            sideOutTargets[i].targetRank = outTargets[i].targetRank;
            sideOutTargets[i].count = 2 * outTargets[i].count;
            sideOutTargets[i].offset = outTargets[i].offset * 2;
            sideOutTargets[i].indexesPtr = sideIndexesOut.data() + sideOutTargets[i].offset;

            for (int j = 0; j < outTargets[i].count; j++) {
                int ind = outTargets[i].indexesPtr[j];
                sideOutTargets[i].indexesPtr[(j * 2)] = lData.sideIndEndA[ind];
                sideOutTargets[i].indexesPtr[(j * 2) + 1] = lData.sideIndEndB[ind];
            }
        }

        for (int i = 0; i < inTargets.size(); i++) {
            sideInTargets[i].count = 2 * inTargets[i].count;
            sideInTargets[i].targetRank = inTargets[i].targetRank;
            sideInTargets[i].offset = inTargets[i].offset * 2;

            sideInTargets[i].indexesPtr = sideIndexesIn.data() + sideInTargets[i].offset;
        }

        transmitFiledsFull(oldSize, lData.getFieldTupleInput(), outTargets, inTargets);
        transmitFiledsFull(oldSize*2, lData.getFieldTupleSideInput(), sideOutTargets, sideInTargets);

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
            int pos = world.domains[i].owner;
            outTargets[pos].count = 0;
            outTargets[pos].targetRank = world.domains[i].owner;
        }

        Iterator::syncAll();

        lData.iterateOwnedLinks(OffloadExec(), [=] DEVCALLABLE (lInd_t lInd, pInd_t aParticleInd, pInd_t bParticleInd,
                                                                LinkDataT &lData, ParticleDataT &pData, LinkTransporterV2<ParticleDataT, LinkDataT> &transporter) {
            if (!transporter.keepInactive && lData.state[lInd] == 0)
                return;
            if(reassign){
                lData.owners[lInd] = pData.owners[aParticleInd];
            }
            int targetRank = getTargetRank(aParticleInd, bParticleInd, lInd, pData, lData);
            if (targetRank != -1) {
                Atomic::atomicIncVal(transporter.outTargets[targetRank].count);
            }
        }, lData, pData, *this);

        Iterator::syncAll();

        int totalCount = 0;
        for (int i = 0; i < numDomains; ++i) {
            totalCount += outTargets[i].count;
        }

        indexesOut.resize(totalCount);
        globalKeysOut.resize(totalCount);

        int offset = 0;
        for (int i = 0; i < numDomains; ++i) {
            outTargets[i].indexesPtr = indexesOut.data() + offset;
            outTargets[i].globalKeysPtr = globalKeysOut.data() + offset;
            outTargets[i].offset = offset;
            offset += outTargets[i].count;
            outTargets[i].count = 0;
        }

        Iterator::syncAll();
        lData.iterateOwnedLinks(OffloadExec(), [=] DEVCALLABLE (lInd_t lInd, pInd_t aParticleInd, pInd_t bParticleInd,
                                                                LinkDataT &lData, ParticleDataT &pData, LinkTransporterV2<ParticleDataT, LinkDataT> &transporter) {
            if (!transporter.keepInactive && lData.state[lInd] == 0)
                return;
            int targetRank = getTargetRank(aParticleInd, bParticleInd, lInd, pData, lData);;
            if (targetRank != -1) {
                // add to outgoing object
                int place = Atomic::atomicIncVal(transporter.outTargets[targetRank].count);

                transporter.outTargets[targetRank].indexesPtr[place] = lInd;
                transporter.outTargets[targetRank].globalKeysPtr[place] = lData.globalKeys[lInd];

            }
        }, lData, pData, *this);

        Iterator::syncAll();
        // drop empty outgoing objects

        outTargets.erase(std::remove_if(outTargets.begin(),
                                              outTargets.end(),
                                              [](auto &x) { return x.count == 0; }), outTargets.end());

    }

    void getOutgoingHalo(World &world, ParticleDataT &pData, LinkDataT &lData) {
        int self = world.self;

        packOutgoingGetRank(world, pData, lData,
                            [=] DEVCALLABLE (pInd_t aParticleInd, pInd_t bParticleInd, lInd_t lInd, ParticleData &pData, LinkData &lData) {
                                int targetRank = -1;
                                /*
                                if (pData.owners[aParticleInd] != self) {
                                    targetRank = pData.owners[aParticleInd];
                                } else
                                */
                                // owner should always be particle A
                                if (pData.owners[bParticleInd] != self) {
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
                                if (targetRank != self){ // && lData.state[lInd] != 0) {
                                    return targetRank;
                                } else {
                                    return -1;
                                }
                            }, true);

        lData.generateOwnedLinksList();
    }
};

#endif //PHELL_LINKTRANSPORTER_H
