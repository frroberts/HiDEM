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
#include "GlobalLocalMapper.h"
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


    // in reality the same as sizeof the cordinate vectors but...
    size_t numLocal = 0;
// from here down i want to move off to user data

    ParticleData(const ParticleData &old_obj) = delete;

    ParticleData(ParticleData &&) = delete;

    ParticleData() = default;

    /*
    hpReal_t getXDiff(lInd_t indA, lInd_t indB) {
        return xCord[indA] - xCord[indB];
    }
    hpReal_t getYDiff(lInd_t indA, lInd_t indB) {
        return yCord[indA] - yCord[indB];
    }
    hpReal_t getZDiff(lInd_t indA, lInd_t indB) {
        return zCord[indA] - zCord[indB];
    }

    hpReal_t getXCord(lInd_t ind) {
        return xCord[ind];
    }
    hpReal_t getYCord(lInd_t ind) {
        return yCord[ind];
    }
    hpReal_t getZCord(lInd_t ind) {
        return zCord[ind];
    }

    hpReal_t getXCordPrev(lInd_t ind) {
        return xCordPrev[ind];
    }
    hpReal_t getYCordPrev(lInd_t ind) {
        return yCordPrev[ind];
    }
    hpReal_t getZCordPrev(lInd_t ind) {
        return zCordPrev[ind];
    }

    void setXCord(hpReal_t val, lInd_t ind) {
        xCord[ind] = val;
    }
    void setYCord(hpReal_t val, lInd_t ind) {
        yCord[ind] = val;
    }
    void setZCord(hpReal_t val, lInd_t ind) {
        zCord[ind] = val;
    }

    void setXCordPrev(hpReal_t val, lInd_t ind) {
        xCordPrev[ind] = val;
    }
    void setYCordPrev(hpReal_t val, lInd_t ind) {
        yCordPrev[ind] = val;
    }
    void setZCordPrev(hpReal_t val, lInd_t ind) {
        zCordPrev[ind] = val;
    }
    */


    std::tuple<hpReal_t, hpReal_t, hpReal_t, hpReal_t, hpReal_t, hpReal_t> getLimits() {
        //

        hpReal_t xMax = std::numeric_limits<hpReal_t>::min();
        hpReal_t yMax = std::numeric_limits<hpReal_t>::min();
        hpReal_t zMax = std::numeric_limits<hpReal_t>::min();
        hpReal_t xMin = std::numeric_limits<hpReal_t>::max();
        hpReal_t yMin = std::numeric_limits<hpReal_t>::max();
        hpReal_t zMin = std::numeric_limits<hpReal_t>::max();

        reducer.minMax(xMin, xMax, xCord);
        reducer.minMax(yMin, yMax, yCord);
        reducer.minMax(zMin, zMax, zCord);

        return std::make_tuple(xMax, yMax, zMax, xMin, yMin, zMin);
    }

    std::tuple<hpReal_t, hpReal_t, hpReal_t, hpReal_t, hpReal_t, hpReal_t> getLimitsOwned() {
        hpReal_t xMax = std::numeric_limits<hpReal_t>::min();
        hpReal_t yMax = std::numeric_limits<hpReal_t>::min();
        hpReal_t zMax = std::numeric_limits<hpReal_t>::min();
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

    void remap(GlobalLocalMapper<gKey_t, pInd_t> &mapper) {
        //nvtxRangePush("ParticleData remap");
        auto start = std::chrono::steady_clock::now();
        int self = 0;
        MPI_Comm_rank(MPI_COMM_WORLD, &self);
        mapper.setSize(globalKeys.size());
        mapper.clear();

        for (size_t i = 0; i < globalKeys.size(); ++i) {
            if(globalKeys[i] < 100){
            //printf("Fail remap %lld \n", globalKeys[i]);
        }
            mapper.map(globalKeys[i], i);
        }

        mapper.finalize();
        if (mapper.numActive() != numLocal)
            std::cout << "mapping different number of particles than state particles " << mapper.numActive() << " " << numLocal << " " << globalKeys.size() << std::endl;
        //std::cout << " remap " << globalKeys.size() << " " << mapper.globalToLocal.size() << std::endl;
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        //std::cout << globalKeys.size() << " elapsed time part: " << elapsed_seconds.count() << "s\n";
        //nvtxRangePop();
    }

    void generateOnwedList() {
        ownedIndexes.clear();
        nonOwnedIndexes.clear();

        for (int i = 0; i < numLocal; i++) {
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
