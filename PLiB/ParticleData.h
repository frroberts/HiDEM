//
// Created by Fredrik Robertsen on 15/06/2021.
//

#ifndef PHELL_PARTICLEDATA_H
#define PHELL_PARTICLEDATA_H

#include <chrono>
#include <mpi.h>

#include <ManVec.h>
#include <Allocator.h>
#include <ExecutionPolicy.h>
#include <Iterator.h>
#include <ParallelReduce.h>

#include "Global.h"
#include "OffloadHashMap.h"
#include "Util/Util.h"


class ParticleData : public ManagedParent {
public:
    ManVec<pInd_t> ownedIndexes;
    ManVec<pInd_t> nonOwnedIndexes;

    ParallelReduce<hpReal_t> reducer;

    ManVec<int> owners;
    ManVec<gKey_t> globalKeys;
    ManVec<uint8_t> type;

    ManVec<hpReal_t> xCord;
    ManVec<hpReal_t> yCord;
    ManVec<hpReal_t> zCord;

    ManVec<lpReal_t> radius;

    size_t numLocal = 0;

    ParticleData(const ParticleData &old_obj) = delete;

    ParticleData(ParticleData &&) = delete;

    ParticleData() = default;

    std::tuple<hpReal_t, hpReal_t, hpReal_t, hpReal_t, hpReal_t, hpReal_t> getLimits() {

        auto [xMin, xMax] = stx::minmax_element(EXECPOLICY xCord.begin(), xCord.end());
        auto [yMin, yMax] = stx::minmax_element(EXECPOLICY yCord.begin(), yCord.end());
        auto [zMin, zMax] = stx::minmax_element(EXECPOLICY zCord.begin(), zCord.end());
        if (xMin == xMax) {
            std::cout << "Generating gird for empty data, exiting... " << std::endl;
            exit(1);
            return std::make_tuple(0,0,0,0,0,0);
        }

        return std::make_tuple(*xMax, *yMax, *zMax, *xMin, *yMin, *zMin);
    }

    std::tuple<hpReal_t, hpReal_t, hpReal_t, hpReal_t, hpReal_t, hpReal_t> getLimitsOwned() {

        hpReal_t xMax = std::numeric_limits<hpReal_t>::lowest();
        hpReal_t yMax = std::numeric_limits<hpReal_t>::lowest();
        hpReal_t zMax = std::numeric_limits<hpReal_t>::lowest();
        hpReal_t xMin = std::numeric_limits<hpReal_t>::max();
        hpReal_t yMin = std::numeric_limits<hpReal_t>::max();
        hpReal_t zMin = std::numeric_limits<hpReal_t>::max();

        reducer.minMaxInderect(xMin, xMax, xCord, ownedIndexes);
        reducer.minMaxInderect(yMin, yMax, yCord, ownedIndexes);
        reducer.minMaxInderect(zMin, zMax, zCord, ownedIndexes);

        return std::make_tuple(xMax, yMax, zMax, xMin, yMin, zMin);
    }

    template<class Policy, typename Fun, class... Args>
    void iterateParticlesIndex(Policy policy, Fun fun, ManVec<pInd_t> &indexes, Args &... args) {
        Iterator::iterateIndirect(policy, fun, indexes, args...);
    }

    template<class Policy, typename Fun, class... Args>
    void iterateOwnedParticles(Policy policy, Fun fun, Args &... args) {
        Iterator::iterateIndirect(policy, fun, ownedIndexes, args...);
    }

    template<class Policy, typename Fun, class... Args>
    void iterateAllParticles(Policy policy, Fun fun, Args &... args) {
        Iterator::iterate(policy, fun, numLocal, args...);
    }

    void remap(OffloadHashMap<gKey_t, pInd_t> &mapper) {
        //xvtxRangePush("ParticleData remap");
        auto start = std::chrono::steady_clock::now();
        int self = 0;
        MPI_Comm_rank(MPI_COMM_WORLD, &self);
        mapper.setSize(globalKeys.size());
        mapper.clear();

        mapper.bulkCreate(globalKeys);

        mapper.finalize();
        if (mapper.numActive() != numLocal)
            std::cout << "mapping different number of particles than state particles " << mapper.numActive() << " " << numLocal << " " << globalKeys.size() << std::endl;
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        //std::cout << globalKeys.size() << " elapsed time part: " << elapsed_seconds.count() << "s\n";
        //xvtxRangePop();
    }

    void generateOnwedList() {
        ownedIndexes.clear();
        nonOwnedIndexes.clear();

        for (size_t i = 0; i < numLocal; i++) {
            if (owners[i] == getLocalRank()) {
                ownedIndexes.push_back(i);
            } else {
                nonOwnedIndexes.push_back(i);
            }
        }
    }


    virtual void dropParticles(ManVec<pInd_t> &droppedInd) = 0;

    virtual void dropNonOwned() = 0;

    virtual void reorder(ManVec<pInd_t> &newOrder) = 0;

    virtual void resize(size_t newSize) = 0;

    virtual void clear() = 0;
};


#endif //PHELL_PARTICLEDATA_H
