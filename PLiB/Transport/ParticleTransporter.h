//
// Created by Fredrik Robertsen on 08/07/2021.
//

#ifndef PHELL_PARTICLETRANSPORTER_H
#define PHELL_PARTICLETRANSPORTER_H

#include "Transporter.h"
template <class ParticleDataT>
class ParticleTrasporterV2 : public TransporterV2 {

public:
    ParticleTrasporterV2() {
        tag = 1000;
    }

    void getIncomming(ParticleDataT &pData) {

        int incomingCount = syncTargets();

        int oldSize = pData.numLocal;
        int newSize = oldSize + incomingCount;

        pData.resize(newSize);

        int tupleCombinedSize = std::tuple_size<decltype(pData.getFieldTupleInput())>::value;
        std::vector<MPI_Request> requests;
        std::vector<MPI_Datatype> gatherTypes;
        requests.reserve(outTargets.size() * (tupleCombinedSize));
        gatherTypes.reserve(outTargets.size() * (tupleCombinedSize));

        transmitFileds(oldSize, requests, gatherTypes, pData.getFieldTupleInput(), 1, outTargets, inTargets);

        MPI_Waitall(requests.size(), requests.data(), MPI_STATUSES_IGNORE);
        for(int i = 0; i < gatherTypes.size(); i++){
            MPI_Type_free(&gatherTypes[i]);
        }

        generateOwnedNonOwned(pData.owners, pData.ownedIndexes, pData.nonOwnedIndexes, pData.numLocal);
    }

    template<class Fun>
    void packOutgoingIf(World &world, ParticleDataT &pData, Fun predicate, bool reassign)
    {
        clear();
        int numDomains = world.domains.size();
        outTargets.resize(numDomains);

        for (int i = 0; i < numDomains; ++i){
            new(&outTargets[i]) TransportTargetV2{};
            outTargets[i].count = 0;
            outTargets[i].targetRank = i;//world.domains[i].owner;
        }

        // world domains are a normal vector, hence a copy here
        ManVec<Domain> domains(numDomains);
        for (int i = 0; i < numDomains; ++i) {
            domains[i] = world.domains[i];
        }
        auto domainsPtr = domains.data();

        // done this way so it can be easier parallelized
        pData.iterateOwnedParticles(OffloadExec(), [=] DEVCALLABLE (pInd_t pInd, ParticleDataT &pData,
                                                       ParticleTrasporterV2<ParticleDataT> &transporter) {
            for (int i = 0; i < numDomains; ++i) {
                auto &d = domainsPtr[i];
                // check if particle is something someone else needs to know about
                if (predicate(pInd, pData, d)) {
                    if (reassign) {
                        pData.owners[pInd] = d.owner;
                    }
                    Iterator::atomicIncVal(transporter.outTargets[d.owner].count);
                }
            }
        }, pData, *this);

        Iterator::syncAll();

        for (int i = 0; i < numDomains; ++i) {
            outTargets[i].indexes.resize(outTargets[i].count);
            outTargets[i].globalKeys.resize(outTargets[i].count);
            outTargets[i].count = 0;
        }
        Iterator::syncAll();

        pData.iterateOwnedParticles(OffloadExec(), [=] DEVCALLABLE (pInd_t pInd, ParticleDataT &pData,
                                                       ParticleTrasporterV2<ParticleDataT> &transporter) {
            for (int i = 0; i < numDomains; ++i) {
                auto &d = domainsPtr[i];
                // check if particle is something someone else needs to know about
                if (predicate(pInd, pData, d)) {
                    // add to outgoing object
                    int place;
                    place = Iterator::atomicIncVal(transporter.outTargets[d.owner].count);
                    transporter.outTargets[d.owner].indexes[place] = pInd;
                    transporter.outTargets[d.owner].globalKeys[place] = pData.globalKeys[pInd];
                }
            }


        }, pData, *this);

        Iterator::syncAll();
/*
        // drop empty outgoing objects
        outTargets.erase(std::remove_if(outTargets.begin(),
                                              outTargets.end(),
                                              [](auto &x) { return x.count == 0; }), outTargets.end());
*/
    }

    void getOutgoingHalo(World &world, ParticleDataT &pData) {
        int self = world.self;
        packOutgoingIf(world, pData, [=] DEVCALLABLE (pInd_t pInd, ParticleDataT &particleData, Domain &d) {
            return d.withinHalo({particleData.xCord[pInd], particleData.yCord[pInd], particleData.zCord[pInd]})
                   && d.owner != self && particleData.owners[pInd] == self;
        }, false);

    }

    void reassignAndGetOutgoing(World &world, ParticleDataT &pData) {
        int self = world.self;

        packOutgoingIf(world, pData, [=] DEVCALLABLE  (pInd_t pInd, ParticleDataT &particleData, Domain &d) {
            return d.within({particleData.xCord[pInd], particleData.yCord[pInd], particleData.zCord[pInd]})
                   && d.owner != self;
        }, true);

        pData.generateOnwedList();
    }
};


#endif //PHELL_PARTICLETRANSPORTER_H
