//
// Created by Fredrik Robertsen on 19/06/2021.
//

#ifndef PHELL_LINKDATA_H
#define PHELL_LINKDATA_H

#include <ManVec.h>
#include <Allocator.h>
#include <ExecutionPolicy.h>
#include <Iterator.h>
#include "Logger/Logger.h"
#include "ParticleData.h"
#include "TupleHelpers.h"
#include "LoggerV2/RangeCollection.h"
#include "Util/OwnedListGen.h"

class LinkData : public ManagedParent {
public:
    size_t numLocal = 0;

    // indexes
    ManVec<lInd_t> ownedIndexes;
    ManVec<lInd_t> nonOwnedIndexes;

    // sized numLocal
    ManVec<int> particleLinkNum;

    ManVec<gKey_t> globalKeyEndA;
    ManVec<gKey_t> globalKeyEndB;

    ManVec<lInd_t> sideIndEndA;
    ManVec<lInd_t> sideIndEndB;

    ManVec<gKey_t> globalKeys;

    ManVec<int> owners;
    ManVec<uint8_t> state; // 0 is inactive everything else is seen as state

    // generated
    ManVec<pInd_t> localPartIndA;
    ManVec<pInd_t> localPartIndB;

    bool keepInactive = true;


    LinkData(const LinkData &old_obj) = delete;

    LinkData(LinkData &&) = delete;

    LinkData() = default;

    template<class Policy, typename Fun, class... Args>
    void iterateAllLinks(Policy policy, Fun fun, Args &... args) {
        Iterator::iterate(policy, [=] DEVCALLABLE(lInd_t lInd, LinkData &lData, Args &... args) {
            auto indA = lData.localPartIndA[lInd];
            auto indB = lData.localPartIndB[lInd];
            fun(lInd, indA, indB, args...);
        }, numLocal, *this, args...);
    }

    template<typename Fun, class... Args>
    void iterateOwnedLinks(OffloadExec policy, Fun fun, Args &... args) {
        //
        Iterator::iterateIndirect(policy, [=] DEVCALLABLE(lInd_t lInd, LinkData &lData, Args &... args) {
            auto indA = lData.localPartIndA[lInd];
            auto indB = lData.localPartIndB[lInd];
            fun(lInd, indA, indB, args...);
        }, ownedIndexes, *this, args...);
    }

    template<class Policy, typename Fun, class... Args>
    void iterateOwnedLinks(Policy policy, Fun fun, Args &... args) {
        //
        Iterator::iterateIndirect(policy, [=](lInd_t lInd, LinkData &lData, Args &... args) {
            auto indA = lData.localPartIndA[lInd];
            auto indB = lData.localPartIndB[lInd];
            fun(lInd, indA, indB, args...);
        }, ownedIndexes, *this, args...);

    }

    void connectParticleLinks(GlobalLocalMapper<gKey_t, pInd_t> &pMapper) {
        RangeV2 r("connect particles", 3);

        makeLocalPartEnds(pMapper);
    }

    // todo make it start from a specific offset
    void makeLocalPartEnds(GlobalLocalMapper<gKey_t, pInd_t> &pMapper, int start = 0) {
        RangeV2 r("make local link ends", 3);

        int self;

        MPI_Comm_rank(MPI_COMM_WORLD, &self);

        // localPartIndA.clear();
        // localPartIndB.clear();
        localPartIndA.resize(numLocal);
        localPartIndB.resize(numLocal);

        pMapper.bulkGetLocal(globalKeyEndA, localPartIndA, start);
        pMapper.bulkGetLocal(globalKeyEndB, localPartIndB, start);

        Iterator::syncAll();
    }

    void reorderLinkTargets(ManVec<lInd_t> &newOrder) {
        RangeV2 r("reoder link targets", 3);

        ManVec<lInd_t> reverseLookup;
        reverseLookup.resize(newOrder.size(), false);
        lInd_t *reverseLookupPtr = reverseLookup.data();
        lInd_t *newOrderPtr = newOrder.data();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(lInd_t i) {
            reverseLookupPtr[newOrderPtr[i]] = i;
        }, newOrder.size());

        pInd_t *localPartIndAPtr = localPartIndA.data();
        pInd_t *localPartIndBPtr = localPartIndB.data();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(lInd_t i) {
            localPartIndAPtr[i] = reverseLookupPtr[localPartIndAPtr[i]];
            localPartIndBPtr[i] = reverseLookupPtr[localPartIndBPtr[i]];
        }, numLocal);

        Iterator::syncAll();
    }

    void remap(GlobalLocalMapper<gKey_t, lInd_t> &mapper) {
        RangeV2 r("remap links", 3);
        mapper.clear();
        mapper.setSize(globalKeys.size());
        mapper.bulkCreate(globalKeys);
        mapper.finalize();
    }

    void remapHalos(GlobalLocalMapper<gKey_t, lInd_t> &mapper, ParticleData &pData) {
        RangeV2 r("remap halo links", 3);

        mapper.setSize(globalKeys.size());
        mapper.clear();

        ManVec<gKey_t> activeKeys;
        activeKeys.resize(globalKeys.size());
        // todo possibility to optimize this to just map the halos
        mapper.bulkCreate(globalKeys);
        /*
        int localRank = getLocalRank();
        // do we really need the openmp parallelism here ?
        for (ind_t i = 0; i < globalKeys.size(); ++i) {
            if (!keepInactive && state[i] == 0)
                continue;
            if (pData.owners[localPartIndA[i]] != localRank || pData.owners[localPartIndB[i]] != localRank) {
                mapper.map(globalKeys[i], i);
            }
        }
        */
        mapper.finalize();
    }

    void generateOwnedLinksList() {
        RangeV2 r("generate owned links list", 3);

        generateOwnedNonOwned(owners, ownedIndexes, nonOwnedIndexes, numLocal);
    }

    ManVec<lInd_t> getNewOrder() {
        RangeV2 r("get new order links", 3);

        ManVec<lInd_t> indexesIn;
        indexesIn.resize(numLocal);
        auto indexesPtr = indexesIn.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(lInd_t i) {
            indexesPtr[i] = i;
        }, numLocal);

#ifdef SOA_CT
        ParallelSort::sortIndirectNLevel(indexesIn, packVecPtrs(particleLinkNum, localPartIndA));
#else
        ParallelSort::sortIndirectNLevel(indexesIn, packVecPtrs(localPartIndA, particleLinkNum));
#endif

        return indexesIn;
    }

    void reindexSideData(ManVec<lInd_t> &newSideOrder) {
        RangeV2 r("reinidex links side data", 3);

        ManVec<lInd_t> reverseLookup;
        reverseLookup.resize((newSideOrder.size()));

        // needed to get iterator to work, should figure out how to pass manVecs from the stack at some point
        lInd_t *reverseLookupPtr = reverseLookup.data();
        lInd_t *newCTOrderPtr = newSideOrder.data();
        lInd_t *sideIndEndAPtr = sideIndEndA.data();
        lInd_t *sideIndEndBPtr = sideIndEndB.data();


        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (lInd_t i) {
            reverseLookupPtr[newCTOrderPtr[i]] = i;
        }, newSideOrder.size());

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (lInd_t i) {
            sideIndEndAPtr[i] = reverseLookupPtr[sideIndEndAPtr[i]];
            sideIndEndBPtr[i] = reverseLookupPtr[sideIndEndBPtr[i]];
        }, sideIndEndA.size());
        Iterator::syncAll();
    }

    int getMaxLinksPerParticle(int numParticles) {
        ManVec<int> linksPerParticle;
        linksPerParticle.resize(numParticles);
        auto linksPerParticlePtr = linksPerParticle.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (pInd_t i) {
            linksPerParticlePtr[i] = 0;
        }, numParticles);
        auto *localPartIndAPtr = localPartIndA.data();
        auto *localPartIndBPtr = localPartIndB.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (lInd_t i) {
            pInd_t pIndA = localPartIndAPtr[i];
            pInd_t pIndB = localPartIndBPtr[i];
            Atomic::atomicIncVal(linksPerParticlePtr[pIndA]);
            Atomic::atomicIncVal(linksPerParticlePtr[pIndB]);
        },numLocal);
        Iterator::syncAll();
        auto elem = stx::max_element(EXECPOLICY linksPerParticle.begin(), linksPerParticle.end() );
        return *elem;
    }

    virtual void dropLinks(ManVec<lInd_t> &droppedLocalIndexes) = 0;

    virtual void dropUnusedLinks(GlobalLocalMapper<gKey_t, pInd_t> &pMapper) = 0;

    virtual void dropBrokenLinks() = 0;

    virtual void dropNonOwnedLinks() = 0;

    virtual void clear() = 0;

    virtual void resize(size_t newSize) = 0;

    virtual void reorder(ManVec<lInd_t> &newOrder) = 0;

    virtual void reorderSideData(ManVec<lInd_t> &newOrder) = 0;

    virtual void resizeSideData(size_t newSize) = 0;

    virtual void dropSideData(ManVec<lInd_t> &droppedSideInd) = 0;
};


#endif //PHELL_LINKDATA_H
