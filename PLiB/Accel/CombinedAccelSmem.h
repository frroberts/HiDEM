//
// Created by Fredrik Robertsen on 05/11/2022.
//

#ifndef PHELL_COMBINEDACCELSMEM_H
#define PHELL_COMBINEDACCELSMEM_H


#include "CombinedAccel.h"
#include <ParallelReduce.h>

#include <Logger/Logger.h>
#include <Transport/CommunicatorIndividual.h>

#include "SmemEnumerator.h"

#ifdef GPU

#include "Tune.h"

#include "DevPortabilityLayer.h"
#include <Util/Unique.h>

#if (ENABLE_CUDA==1)

#include <cooperative_groups.h>
#include <cuda/barrier>


#include <cub/cub.cuh>

namespace cubX = cub;
#elif (ENABLE_HIP==1)
//#include <cub/cub.cuh>

#include <hipcub/hipcub.hpp>
namespace cubX = hipcub;
#endif

__host__ __device__
    int getIndexInBlock(int blockId, int threadId, int i, int indexBlocSize) {
    //
    int startOffset = indexBlocSize*blockId;
    int inBlockOffset = (i*CombinedSmemPairTune::X_DIMM_THREADS) + threadId;
    return startOffset + inBlockOffset;
}

#endif


class CombinedAccelSmem : public CombinedAccel {
public:

    static constexpr int numInCache = CombinedSmemPairTune::NUM_IN_CACHE_LARGE; // total number of particles in cache
    int indexBlockSize;
public:


    ManVec<pInd_t> cachingIndexes;
    ManVec<int> activeInBlock;
    ManVec<lInd_t> linkCTInd;
    ManVec<int16_t> linkedPartIndInCache;
    ManVec<int16_t> neighPartIndInCache;
    ManVec<int16_t> selfPartIndInCache;
    ManVec<pInd_t> iterationOrder;

    ManVec<int> blockIndexes;
    ManVec<int> numNeighsPossiblyPruned;

    ManVec<pInd_t> nonCachableIndexes;
    ManVec<pInd_t> largeCachableIndexes;
    ManVec<pInd_t> cachableIndexes;

    ManVec<uint8_t> sideState;

    // link smem accel stuff
    ManVec<int> activeInBlockLink;
    // ManVec<int> blockIndexes;
    ManVec<int> cachingIndexesLink;
    ManVec<int16_t> localPartIndACache;
    ManVec<int16_t> localPartIndBCache;

    int cacheCountLink = 768;

    uint32_t linksPerBlock = 2048;
    uint32_t numLinkBlocks;

    CombinedAccelSmem() {
    }

    template<class Policy, typename NFun, typename LFun, class FullArgs>
    void iterateCombinedPre(Policy policy, ParticleData &pData, LinkData &lData,
                            NFun &nFunc, LFun lFunc, FullArgs args) {


        if (nonCachableIndexes.size() != 0) {
            RangeV2 r("non cachable part force", 3);
            CombinedAccel::iterateCombined(policy, nonCachableIndexes, pData, lData, nFunc, lFunc, args);
        }


        if (largeCachableIndexes.size() != 0) {
            RangeV2 r("double cachable part force", 3);
            iterateCombined(policy, largeCachableIndexes, pData, lData, CombinedSmemPairTune::NUM_IN_CACHE_LARGE, nFunc, lFunc, args);
        }

        RangeV2 r("normal part force", 2);

#ifdef ENABLE_ASYNC_COMM

        iterateCombined(policy, preCommIndexes, pData, lData, CombinedSmemPairTune::NUM_IN_CACHE, nFunc, lFunc, args);

#warning "compiling async comm"
#else
        iterateCombined(policy, pData.ownedIndexes, pData, lData, CombinedSmemPairTune::NUM_IN_CACHE, nFunc, lFunc, args);
#endif
    }

    template<class Policy, typename NFun, typename LFun, class FullArgs>
    void iterateCombinedPost(Policy policy, ParticleData &pData, LinkData &lData,
                             NFun &nFunc, LFun lFunc, FullArgs args) {
#ifdef ENABLE_ASYNC_COMM

            iterateCombined(policy, postCommIndexes, pData, lData, CombinedSmemPairTune::NUM_IN_CACHE, nFunc, lFunc, args);

#endif
    }

    template <class T>
    void generatePrePostCommIndexes(ParticleData& pData, LinkData& lData, T& comm) {
        xvtxRangePush("generatePrePostCommIndexes");
#ifdef ENABLE_ASYNC_COMM

        int lineLen = CombinedSmemPairTune::X_DIMM_THREADS; // line length in particles

        ManVec<bool,ScratchAllocator<bool>> outgoingFlags;
        outgoingFlags.resize(pData.numLocal, false);
        auto outgoingFlagsPtr = outgoingFlags.data();

        Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int i) {
            outgoingFlagsPtr[i] = false;
        }, pData.numLocal);

        auto outIndexPtr = comm.outgoingIndexes.data();
        Iterator::iterate(OffloadExec(), [=]DEVCALLABLE(int i){
            outgoingFlagsPtr[outIndexPtr[i]] = true;
        }, comm.outgoingIndexes.size());

        iterationOrder.resize(pData.ownedIndexes.size(), false); // pData.numLocal);
        ManVec<pInd_t, ScratchAllocator<pInd_t>> iterationOrderNew;
        iterationOrderNew.resize(pData.ownedIndexes.size(), false);

        auto iterationOrderPtr = iterationOrderNew.data();

        // this could be a block for the internal look
        Iterator::iterate(OffloadExec(), [=]DEVCALLABLE (int i, ParticleData& pData)
        {
            bool transportFlag = false;
            for (int j = 0; j < lineLen; j++)
            {
                if ((i * lineLen) + j >= pData.ownedIndexes.size())
                    continue;
                pInd_t pInd = pData.ownedIndexes[(i * lineLen) + j];
                transportFlag = transportFlag || outgoingFlagsPtr[pInd];
                iterationOrderPtr[(i * lineLen) + j] = pInd;
            }
            for (int j = 0; j < lineLen; j++)
            {
                if ((i * lineLen) + j >= pData.ownedIndexes.size())
                    continue;
                pInd_t pInd = pData.ownedIndexes[(i * lineLen) + j];
                outgoingFlagsPtr[pInd] = transportFlag;
            }
        }, 1 + (pData.ownedIndexes.size() / lineLen), pData);
        Iterator::syncAll();

        auto partitionPoint = stx::stable_partition( EXECPOLICY iterationOrderNew.begin(), iterationOrderNew.end(),
            [=]DEVCALLABLE(int i)
        {
            return outgoingFlagsPtr[i] == true;
        });

        int preSize = std::distance(iterationOrderNew.begin(), partitionPoint);
        int postSize = std::distance(partitionPoint, iterationOrderNew.end());

        preCommIndexes.resize(preSize, false);
        postCommIndexes.resize(postSize, false);
        Iterator::copy(preCommIndexes.data(), iterationOrderNew.begin(), preSize*sizeof(pInd_t));
        Iterator::copy(postCommIndexes.data(), &(iterationOrderNew.begin()[preSize]), postSize*sizeof(pInd_t));
        Iterator::syncAll();

        // the half full block must be last in the iteration order, hence we need to figure out what parts is %128
        // and then pack iteration order so that the full blocks come first
        auto firstPtr = postCommIndexes.data();
        int firstCount = postCommIndexes.size();
        auto secondPtr = preCommIndexes.data();
        int secondCount = preCommIndexes.size();

        if (firstCount % CombinedSmemPairTune::X_DIMM_THREADS != 0) {
            std::swap(firstPtr, secondPtr);
            std::swap(firstCount, secondCount);
        }
        Iterator::iterate(OffloadExec(),
                          [=] DEVCALLABLE(int i, CombinedAccelSmem &accel, ParticleData &pData) {
                              accel.iterationOrder[i] = firstPtr[i];
                          },
                          firstCount, *this, pData);

        Iterator::iterate(OffloadExec(),
                          [=] DEVCALLABLE(int i, CombinedAccelSmem &accel, ParticleData &pData) {
                              accel.iterationOrder[i + firstCount] = secondPtr[i];
                          },
                          secondCount, *this, pData);
        Iterator::syncAll();

#else
        iterationOrder.resize(pData.ownedIndexes.size());//pData.numLocal);
        Iterator::iterate(OffloadExec(),
                          [=] DEVCALLABLE(int i, CombinedAccelSmem &accel, ParticleData &pData) {
                              accel.iterationOrder[i] = pData.ownedIndexes[i];
                          },
                          pData.ownedIndexes.size(), *this, pData);
#endif
        xvtxRangePop();

    }

    void generateNeighbours(ParticleData &pData, LinkData &lData, double searchDistance, double interactDist) {
        {
            RangeV2 r("grid gen", 3);
            grid.generate(pData, searchDistance);
        } {
            RangeV2 r("nlist gen", 3);
            nList.generateNeighbours(grid, pData, searchDistance, interactDist);
        }
    }

    void generateLinks(ParticleData &pData, LinkData &lData, double searchDistance, double interactDist) {
        {
            RangeV2 r("llist gen", 3);
            lAccel.generateLinks(pData, lData);
        }
    }

    void setupSideStates(LinkData &lData) {
        // this sets up the side state values
        // side state is used for optimization, so that the link state is replicated to each side of the link,
        // allowing for optimal data layout when checking the state of a link.
        sideState.resize( lData.numSideData);
        auto statePtr = lData.state.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i, CombinedAccelSmem &accel) {
            pInd_t pInd = accel.iterationOrder[i];
            accel.lAccel.iterateParticleLinksCT([&](auto lInd, auto sideInd, auto pIndOther) {
                accel.sideState[sideInd] = statePtr[lInd];
            }, pInd);
        }, iterationOrder.size(), *this);
        Iterator::syncAll();
    }

    void getNonCachableIndexes() {
        nonCachableIndexes.clear();
        nonCachableIndexes.resize(iterationOrder.size());
        Iterator::copy(nonCachableIndexes.data(), iterationOrder.data(), nonCachableIndexes.size()*sizeof(pInd_t));
        auto activeBlockPtr = activeInBlock.data();
        auto endPos = stx::stable_partition(EXECPOLICY nonCachableIndexes.begin(), nonCachableIndexes.end(),
            [=, this] DEVCALLABLE (pInd_t pInd) {
            int blocInd = blockIndexes[pInd];// pInd/CombinedSmemPairTune::X_DIMM_THREADS;
            if (activeBlockPtr[blocInd] < CombinedSmemPairTune::NUM_IN_CACHE_LARGE) {
                return false;
            }
            return true;
        });

        largeCachableIndexes.clear();
        largeCachableIndexes.resize(std::distance(endPos, nonCachableIndexes.end()));
        Iterator::copy(largeCachableIndexes.data(), endPos, largeCachableIndexes.size()*sizeof(pInd_t));
        auto endPosDouble = stx::stable_partition(EXECPOLICY largeCachableIndexes.begin(), largeCachableIndexes.end(),
            [=, this] DEVCALLABLE (pInd_t pInd) {
            int blocInd = blockIndexes[pInd];// pInd/CombinedSmemPairTune::X_DIMM_THREADS;
            if (activeBlockPtr[blocInd] < CombinedSmemPairTune::NUM_IN_CACHE) {
                return false;
            }
            return true;
        });
        largeCachableIndexes.resize(std::distance(largeCachableIndexes.begin(), endPosDouble));
        nonCachableIndexes.resize(std::distance(nonCachableIndexes.begin(), endPos));

    }

    void generateLinkCacheIndexes(LinkData& lData) {
        //xvtxRangePop();
        // todo this has no check for going over the cache size
        RangeV2 r("smem links gen", 3);

        numLinkBlocks = 1+ (lData.ownedIndexes.size()/linksPerBlock);

        activeInBlockLink.resize(numLinkBlocks);

        cachingIndexesLink.resize(numLinkBlocks * cacheCountLink);
        localPartIndACache.resize(lData.ownedIndexes.size());
        localPartIndBCache.resize(lData.ownedIndexes.size());

        getErrorDev((DevKernels2::iterateNonExitKern<<<numLinkBlocks, 256>>>(
            [] __device__ (CombinedAccelSmem &accel, LinkData &lData) {
            //
            __shared__ SmemEnumerator<pInd_t, int16_t, 4096> enumerator;
            enumerator.numMapped = 0;
            enumerator.clear();
            __syncthreads();
            int g = blockIdx.x;
            //             size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
            int tid = threadIdx.x;

            for (int t = tid; t < accel.linksPerBlock; t = t + blockDim.x) {
                int ind = t + (g * accel.linksPerBlock);
                if (ind >= lData.ownedIndexes.size())
                    continue;
                pInd_t pIndA = lData.localPartIndA[ind];
                pInd_t pIndB = lData.localPartIndB[ind];
                enumerator.mapValue(pIndA);
                enumerator.mapValue(pIndB);
            }
            __syncthreads();

            for (int t = tid; t < accel.linksPerBlock; t = t + blockDim.x) {
                int ind = t + (g * accel.linksPerBlock);
                if (ind >= lData.ownedIndexes.size())
                    continue;
                pInd_t pIndA = lData.localPartIndA[ind];
                pInd_t pIndB = lData.localPartIndB[ind];

                accel.localPartIndACache[ind] = enumerator.getValue(pIndA);
                accel.localPartIndBCache[ind] = enumerator.getValue(pIndB);
            }
            enumerator.storeCompressedKeys(&accel.cachingIndexesLink[(blockIdx.x * accel.cacheCountLink)]);
            if (tid == 0)
                accel.activeInBlockLink[g] = enumerator.numMapped;

        }, *this, lData)));

        Iterator::syncAll();
    }


    template <class PDataT>
    void generateIndexes(PDataT &pData, LinkData &lData, double searchDistance, double interactDist) {
        //
        RangeV2 r5("accel generate", 2);



        int blocks = 1 + (iterationOrder.size() / CombinedSmemPairTune::X_DIMM_THREADS);

        int maxNeighLinked = nList.maxNeighbours;// std::max(lData.allocatedMaxLinks, static_cast<int>(nList.maxNeighbours));
        indexBlockSize = maxNeighLinked * CombinedSmemPairTune::X_DIMM_THREADS;

        cachingIndexes.resize(blocks * numInCache, false);
        activeInBlock.resize(blocks, false);

        linkedPartIndInCache.resize(blocks * indexBlockSize, false);
        neighPartIndInCache.resize(blocks * indexBlockSize, false);
        linkCTInd.resize(blocks * indexBlockSize, false);

        selfPartIndInCache.resize(iterationOrder.size(), false);
        numNeighsPossiblyPruned.resize(nList.numNeighs.size(), false);

        blockIndexes.resize(pData.numLocal, false);
        auto blockIndexPtr = blockIndexes.data();
        Iterator::iterate(OffloadExec(),[=] DEVLAMBDA (int i) {
            blockIndexPtr[i] = -1;
        }, pData.numLocal);

        Iterator::syncAll();

        auto range = xvtxRangeStart("accel");

        // GPU only
        auto start = std::chrono::steady_clock::now();

        getErrorDev((DevKernels2::iterateNonExitKern<<<1 + (iterationOrder.size() / CombinedSmemPairTune::X_DIMM_THREADS), CombinedSmemPairTune::X_DIMM_THREADS>>>(
            [] __device__ (NeighAccel &nList, LinkData &lData, CombinedAccelSmem &accel) {
                int idx = blockIdx.x * blockDim.x + threadIdx.x;

                __shared__ SmemEnumerator<pInd_t, uint16_t, CombinedSmemPairTune::SHARED_ENUM_SET_SIZE> enumerator;
                enumerator.numMapped = 0;
                enumerator.clear();

                __syncthreads();

                // iterate over particles, neighbours, and linked ones to create the set of indexes in the cache and enumerate them
                // we do not need the extra threads of the last block in this section
                if (idx < accel.iterationOrder.size()) {

                    pInd_t pInd = INT32_MAX;
                    // add own index to cache
                    pInd = accel.iterationOrder[idx];
                    // add block index to blockIndexes
                    accel.blockIndexes[pInd] = blockIdx.x;
                    enumerator.mapValue(pInd);

                    int numNeighs = nList.numNeighs[pInd];

                    for (int k = 0; k < numNeighs; ++k) {
                        pInd_t pIndOther;
                        pIndOther = nList.getPartNeighId(k, pInd);
                        enumerator.mapValue(pIndOther);
                    }

                    int particleLinkCount = accel.lAccel.linksPerParticle[pInd];

                    for (int k = 0; k < particleLinkCount; ++k) {
                        pInd_t pIndOther;
                        lInd_t lInd = accel.lAccel.getPartLinkId(k, pInd);
                        pIndOther = lData.localPartIndB[lInd];
                        if (pIndOther == pInd) {
                            pIndOther = lData.localPartIndA[lInd];
                        }
                        enumerator.mapValue(pIndOther);
                    }
                }

                __syncthreads();
                // todo for full reproducibility the cache indexes would need to be sorted and re-enumerated here

                // here we need the last threads to ensure everything is written out
                // thread 0 writes out how many we have cached
                if (threadIdx.x == 0) {
                    accel.activeInBlock[blockIdx.x] = enumerator.numMapped;
                }
                // write out how many are in the cache
                if (enumerator.numMapped >= numInCache) {
                    // if this block is over the space we have in the cache, exit here
                    return;
                }

                enumerator.storeCompressedKeys(&accel.cachingIndexes[(blockIdx.x * numInCache)]);

                // if the thread is outside of the active particles return here,
                // after this we do not need the extra threads so we can exit them
                if (idx >= accel.iterationOrder.size())
                    return;

                pInd_t pInd = accel.iterationOrder[idx];

                pInd_t ind = ((blockIdx.x * CombinedSmemPairTune::X_DIMM_THREADS) + threadIdx.x);

                accel.selfPartIndInCache[ind] = enumerator.getValue(pInd);

                int particleNeighCount = nList.numNeighs[pInd];
                for (size_t k = 0; k < particleNeighCount; k++) {
                    pInd_t pIndOther = nList.getPartNeighId(k, pInd);
                    accel.neighPartIndInCache[getIndexInBlock(blockIdx.x, threadIdx.x, k, accel.indexBlockSize)] = enumerator.getValue(pIndOther);
                }

                int particleLinkCount = accel.lAccel.linksPerParticle[pInd];
                for (size_t k = 0; k < particleLinkCount; k++) {
                    //
                    lInd_t lInd = accel.lAccel.getPartLinkId(k, pInd);

                    pInd_t pIndOther = lData.localPartIndB[lInd];
                    lInd_t ctInd;

                    if (pIndOther == pInd) {
                        pIndOther = lData.localPartIndA[lInd];
                        ctInd = lData.sideIndEndB[lInd];
                    } else {
                        ctInd = lData.sideIndEndA[lInd];
                    }
                    accel.linkCTInd[getIndexInBlock(blockIdx.x, threadIdx.x, k, accel.indexBlockSize)] = ctInd;
                    accel.linkedPartIndInCache[getIndexInBlock(blockIdx.x, threadIdx.x, k, accel.indexBlockSize)] = enumerator.getValue(pIndOther);
                }

                if constexpr (CombinedSmemPairTune::FUSED_NEIGH) {
                    // if we run fused neighbors, remove the linked particles from the neighbor list
                    for (size_t nn = 0; nn < particleNeighCount; nn++) {
                        pInd_t pIndOther = accel.neighPartIndInCache[getIndexInBlock(blockIdx.x, threadIdx.x, nn, accel.indexBlockSize)];
                        bool found = false;
                        for (size_t nl = 0; nl < particleLinkCount; nl++) {
                            pInd_t pIndLinked = accel.linkedPartIndInCache[getIndexInBlock(blockIdx.x, threadIdx.x, nl, accel.indexBlockSize)];
                            if (pIndLinked == pIndOther) {
                                found = true;
                                break;
                            }
                        }
                        if (found) {
                            accel.neighPartIndInCache[getIndexInBlock(blockIdx.x, threadIdx.x, nn, accel.indexBlockSize)] = -1;
                        }
                    }

                    pInd_t write = 0;
                    for (pInd_t read = 0; read < particleNeighCount; ++read) {
                        if (accel.neighPartIndInCache[getIndexInBlock(blockIdx.x, threadIdx.x, read, accel.indexBlockSize)] != -1) {
                            accel.neighPartIndInCache[getIndexInBlock(blockIdx.x, threadIdx.x, write, accel.indexBlockSize)] =
                                    accel.neighPartIndInCache[getIndexInBlock(blockIdx.x, threadIdx.x, read, accel.indexBlockSize)];
                            ++write;
                        }
                    }
                    accel.numNeighsPossiblyPruned[pInd] = write;
                } else {
                    accel.numNeighsPossiblyPruned[pInd] = nList.numNeighs[pInd];
                }
            }, nList, lData, *this)));
        Iterator::syncAll();

        int min = 100000;
        int max = 0;
        ParallelReduce<int>::minMax(min, max, activeInBlock);

        xvtxRangeEnd(range);

        getNonCachableIndexes();

        setupSideStates(lData);

        Logger::writeAtFirst(fmt::format(
            "Cache stats, normal space {}, large space {}, world stats: max used {}, min used {}, max uncachable {}, max large cachable {} \n",
            CombinedSmemPairTune::NUM_IN_CACHE, CombinedSmemPairTune::NUM_IN_CACHE_LARGE,
            Logger::getMaxValWorld(max), Logger::getMaxValWorld(min),
            Logger::getMaxValWorld(nonCachableIndexes.size()), Logger::getMaxValWorld(largeCachableIndexes.size())));

#ifndef SMEM_LINKS
        generateLinkCacheIndexes(lData);
#endif
    }

    template<class Policy, typename NFun, typename LFun, class FullArgs>
    void iterateCombined(Policy policy, ManVec<pInd_t> &indexes, ParticleData &pData, LinkData &lData, int cacheSizeLimit,
                         NFun &nFunc, LFun lFunc, FullArgs args);

#ifdef SMEM_LINKS

    template<class Policy, typename LFun, class FullArgs>
    void iterateLinks(Policy policy, ParticleData &pData, LinkData &lData,
                  LFun &lFunc, FullArgs args);

#else
    // keep this disabled
    template<class Policy, typename LFun, class FullArgs>
    void iterateLinks(Policy policy, ParticleData &pData, LinkData &lData,
                      LFun &lFunc, FullArgs args) {
        //

        lData.iterateAllLinks(policy, [=]
        DEVCALLABLE(lInd_t lInd, pInd_t aParticleInd, pInd_t bParticleInd,
                    LinkData &lData, LinkAccelSide &lAccel, CombinedAccelSmem &accel) {
            auto pack = args.split();

            auto partArgs = std::get<0>(pack);
            auto linkSideArgs = std::get<1>(pack);
            auto linkArgs = std::get<2>(pack);

            auto partA = getAtIndTuple(aParticleInd, partArgs);
            auto partB = getAtIndTuple(bParticleInd, partArgs);
            auto sideA = getAtIndTuple(lData.sideIndEndA[lInd], linkSideArgs);
            auto sideB = getAtIndTuple(lData.sideIndEndB[lInd], linkSideArgs);

            uint8_t initialState = lData.state[lInd];

            auto all = std::tuple_cat(partA, partB, sideA, sideB);

            auto linkVals = getAtIndTuple(lInd, linkArgs);

            callFun2(lFunc, all, linkVals);

            setAtIndTuple(lInd, linkArgs, linkVals);

            uint8_t updatedState = lData.state[lInd];
            if (updatedState != initialState && updatedState == 0) {
                // break link in accel ?
                // get an idea of what the performance impact of this is
                accel.sideState[lData.sideIndEndB[lInd]] = 0;
                accel.sideState[lData.sideIndEndA[lInd]] = 0;

            }
        }, lData, lAccel, *this);

    }
#endif

    template<class Policy, class LData, typename LFun, class FullArgs>
    void iterateLinksRegen(Policy policy, ParticleData &pData, LData &lData,
                         LFun &lFunc, FullArgs args) {
        //

        lData.iterateOwnedLinks(policy, [=] DEVCALLABLE(lInd_t lInd, pInd_t aParticleInd, pInd_t bParticleInd,
            LData &lData, ParticleData &pData, CombinedAccelSmem &lAccel) {
            auto pack = args.split();

            auto partArgs = std::get<0>(pack);
            auto linkSideArgs = std::get<1>(pack);
            auto linkArgs = std::get<2>(pack);

            auto partA = getAtIndTuple(aParticleInd, partArgs);
            auto partB = getAtIndTuple(bParticleInd, partArgs);
            auto sideA = getAtIndTuple(lData.sideIndEndA[lInd], linkSideArgs);
            auto sideB = getAtIndTuple(lData.sideIndEndB[lInd], linkSideArgs);

            uint8_t initialState = lData.state[lInd];

            auto all = std::tuple_cat(partA, partB, sideA, sideB);

            auto linkVals = getAtIndTuple(lInd, linkArgs);

            callFun2(lFunc, all, linkVals);

            setAtIndTuple(lInd, linkArgs, linkVals);

            uint8_t updatedState = lData.state[lInd];
            if (updatedState != initialState && updatedState == 0) {
                // break link in accel ?
                // get an idea of what the performance impact of this is
                lAccel.sideState[lData.sideIndEndB[lInd]] = 0;
                lAccel.sideState[lData.sideIndEndA[lInd]] = 0;

            }
            if(updatedState != initialState && updatedState == 1) {
                // if regenerated compute the new length and zero CT
                                // break link in accel ?
                // get an idea of what the performance impact of this is
                lAccel.sideState[lData.sideIndEndB[lInd]] = 1;
                lAccel.sideState[lData.sideIndEndA[lInd]] = 1;

                lpReal_t xDist = pData.xCord[aParticleInd] - pData.xCord[bParticleInd];
                lpReal_t yDist = pData.yCord[aParticleInd] - pData.yCord[bParticleInd];
                lpReal_t zDist = pData.zCord[aParticleInd] - pData.zCord[bParticleInd];
                lpReal_t currentLength = sqrtf((xDist * xDist) + (yDist * yDist) + (zDist * zDist));

                ElementaryLink link;

                link.owner = lData.owners[lInd];
                link.globalKey = lData.globalKeys[lInd];
                link.globalKeyParticleA = lData.globalKeyEndA[lInd];
                link.globalKeyParticleB = lData.globalKeyEndB[lInd];
                link.particleLinkNum = lData.particleLinkNum[lInd];
                link.state = 1;
                link.initialLen = currentLength;

                lData.processInitLink(link, lData, lInd, lData.sideIndEndA[lInd], lData.sideIndEndB[lInd]);
            }
        }, lData, pData, *this);

    }
};


#ifdef GPU

#if (ENABLE_CUDA==1)
template <std::size_t... ints, class... Args, class BarrierT>
DEVCALLABLE void
cudaLoadToSharedTupleImpl(size_t indIn, size_t indOut,
                          const std::tuple<Args *...> &ptrsIn,
                          const std::tuple<Args *...> &ptrsOut,
                          std::integer_sequence<std::size_t, ints...>, BarrierT &barrier) {
  (cuda::memcpy_async(&std::get<ints>(ptrsOut)[indOut], &std::get<ints>(ptrsIn)[indIn],
                     cuda::aligned_size_t<sizeof(Args)>(sizeof(Args)), barrier), ...);
}

template <class... Args, class BarrierT>
DEVCALLABLE void cudaLoadToSharedTuple(size_t indIn, size_t indOut,
                                       const std::tuple<Args *...> &ptrsIn,
                                       const std::tuple<Args *...> &ptrsOut, BarrierT &barrier) {
    cudaLoadToSharedTupleImpl(indIn, indOut, ptrsIn, ptrsOut, std::make_index_sequence<sizeof...(Args)>(), barrier);
}
#endif


template<typename LFun, class FullArgs>
__global__
void
pairIteratorLinksCache(lInd_t *indexes, int numIndexes,
                          LFun lFunc,
                          int *activeInBlockPtr,
                          // int *blockIndexesPtr,
                          lInd_t *cachingIndexesPtr,
                          const int16_t *localPartIndACachePtr,const int16_t *localPartIndBCachePtr,
                          const lInd_t *sideIndEndAPtr, const lInd_t *sideIndEndBPtr, uint8_t *statePtr,
                          uint8_t *sideStatePtr,
                          int cacheCount,
                          int linksPerBlock,
                          const FullArgs args)
{
    // todo this has no way of handling the blocks with more links than the cache size

    auto pack = args.split();

    auto partArgs = std::get<0>(pack);
    auto linkSideArgs = std::get<1>(pack);
    auto linkArgs = std::get<2>(pack);

    extern __shared__ uint8_t baseShardPtr[];
    int shrdOffset = 0;
    const auto partArgsShrdPtr = makeSharedPtrs(&baseShardPtr[shrdOffset], cacheCount, partArgs);


    int activeInBlock = activeInBlockPtr[blockIdx.x];
    if (activeInBlock == -1) {
        return;
        // handled by other kernel todo implement other kernel
    } else {

        for (int i = threadIdx.x; i < activeInBlock; i += (blockDim.x)) {
            lInd_t readInd = cachingIndexesPtr[(blockIdx.x * cacheCount) + i];
            setAtIndTuple(i, partArgsShrdPtr, getAtIndTupleLdg(readInd, partArgs));
        }

        __syncthreads();



        for (int i = threadIdx.x; i < linksPerBlock; i += blockDim.x)
        {
            int idx = (blockIdx.x * linksPerBlock) + i;
            if (idx >= numIndexes) return;

            lInd_t lInd;
            lInd = idx;// indexes[idx]; // needs to read the index if it should replace iterate owned links

            auto pIndACache = localPartIndACachePtr[lInd];
            auto pIndBCache = localPartIndBCachePtr[lInd];


            auto sideA = getAtIndTuple(sideIndEndAPtr[lInd], linkSideArgs);
            auto sideB = getAtIndTuple(sideIndEndBPtr[lInd], linkSideArgs);

            auto partA = getAtIndTuple(pIndACache, partArgsShrdPtr);
            auto partB = getAtIndTuple(pIndBCache, partArgsShrdPtr);

            uint8_t initialState = statePtr[lInd];

            auto all = std::tuple_cat(partA, partB, sideA, sideB);

            auto linkVals = getAtIndTuple(lInd, linkArgs);

            callFun2(lFunc, all, linkVals);

            setAtIndTuple(lInd, linkArgs, linkVals);

            uint8_t updatedState = statePtr[lInd];
            if (updatedState != initialState && updatedState == 0) {
                // break link in accel ?
                // get an idea of what the performance impact of this is
                sideStatePtr[sideIndEndBPtr[lInd]] = 0;
                sideStatePtr[sideIndEndAPtr[lInd]] = 0;

            }
        }
    }
}


template<typename NFun, typename LFun, class FullArgs>
__global__
void
__launch_bounds__(CombinedSmemPairTune::THREADS_PER_BLOCK, CombinedSmemPairTune::BLOCKS_PER_SM)
pairIteratorCombinedCache(pInd_t *indexes,
                            NFun nFunc,
                          LFun lFunc, pInd_t *activeInBlockPtr, pInd_t *blockIndexesPtr, pInd_t *cachingIndexesPtr,
                          lInd_t *linkCTIndPtr,
                          int16_t *linkedPartIndInCachePtr, int *linksPerParticlePtr,
                          int16_t *neighPartIndInCachePtr,
                          const int *numNeighsPtr,
                          const int16_t *selfPartIndInCachePtr, const uint8_t *statePtr,
                          const uint8_t *sideStatePtr,
                          hpReal_t *xCordPtr, hpReal_t *yCordPtr, hpReal_t *zCordPtr,
                          pInd_t *particleLinkIdPtr, const int numPraticlesAllocatedLink,
                          const size_t numIndexes,
                          const int indexBlockSize, const int linkSideBlockCount, int elementsInCache, const FullArgs args) {

    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int tid = (blockDim.x * threadIdx.y) + threadIdx.x;

    // because OMP we cant use structured bindings here
    auto pack = args.split();
    //auto [neighArgs,linkArgs,linkSideArgs,forceArgs] = args.split4();

    const auto neighArgs = std::get<0>(pack);
    const auto linkArgs = std::get<1>(pack);
    const auto linkSideArgs = std::get<2>(pack);
    const auto forceArgs = std::get<3>(pack);

    // accel.blockIndexes[idx]

    extern __shared__ uint8_t baseShardPtr[];


    int shrdOffset = 0;
/*
    auto block = cooperative_groups::this_thread_block();
    auto thread = cooperative_groups::this_thread();
    using barrier_t = cuda::barrier<cuda::thread_scope_block>;

    __shared__ barrier_t barrier;
    shrdOffset += sizeof(barrier_t);
    if (block.thread_rank() == 0) {
        init(&barrier, block.size());
    }
    __syncthreads();
*/
    const auto cordArgs = std::make_tuple(xCordPtr, yCordPtr, zCordPtr);
    const auto cordArgShrdPtr = makeSharedPtrs(&baseShardPtr[shrdOffset], elementsInCache, cordArgs);

    shrdOffset += elementsInCache * 3 * sizeof(hpReal_t );
    const auto neighArgsShrdPtr = makeSharedPtrs(&baseShardPtr[shrdOffset], elementsInCache, neighArgs);

    auto force = zeroTuple(forceArgs);

    auto allCachable = std::tuple_cat(cordArgs, neighArgs);
    auto allSharedPtr = std::tuple_cat(cordArgShrdPtr, neighArgsShrdPtr);

    pInd_t pInd;
    ind_t idxCache;
    if (idx >= numIndexes) {
        pInd = indexes[numIndexes - 1];
    } else {
        pInd = indexes[idx];
    }
    int cacheBlockIdx = blockIndexesPtr[pInd];
    idxCache = cacheBlockIdx * blockDim.x + threadIdx.x;

    int activeInBlock = activeInBlockPtr[cacheBlockIdx];
    if (activeInBlock >= elementsInCache) {
        return;
        // handled by other kernel
    } else {

        for (int i = tid; i < activeInBlock; i += (blockDim.x * blockDim.y)) {
            //
            pid_t readInd = cachingIndexesPtr[(cacheBlockIdx * CombinedAccelSmem::numInCache) + i];

            // cudaLoadToSharedTuple(readInd, i, cordArgs, cordArgShrdPtr, barrier);
            // cudaLoadToSharedTuple(readInd, i, neighArgs, neighArgsShrdPtr, barrier);

            setAtIndTuple(i, allSharedPtr, getAtIndTupleLdg(readInd, allCachable));
        }

        // Wait for all copies to complete.
        // barrier.arrive_and_wait();

        __syncthreads();
        if (idx >= numIndexes) return;

        pInd_t pIndC = selfPartIndInCachePtr[idxCache]; // not idx

        auto cord = getAtIndTuple(pIndC, cordArgShrdPtr);
        auto self = getAtIndTuple(pIndC, neighArgsShrdPtr);

        int numNeighs = numNeighsPtr[pInd];
        int particleLinkCount = linksPerParticlePtr[pInd];

        for (size_t i = threadIdx.y; i < numNeighs; i = i + blockDim.y) {
            pInd_t pIndOther = neighPartIndInCachePtr[getIndexInBlock(cacheBlockIdx, threadIdx.x, i, indexBlockSize)];
            auto cordOther = getAtIndTuple(pIndOther, cordArgShrdPtr);
            auto neigh = getAtIndTuple(pIndOther, neighArgsShrdPtr);

            lpReal_t xDist = std::get<0>(cord) - std::get<0>(cordOther);//xCord - xCordOth;
            lpReal_t yDist = std::get<1>(cord) - std::get<1>(cordOther);//yCord - yCordOth;
            lpReal_t zDist = std::get<2>(cord) - std::get<2>(cordOther);//zCord - zCordOth;

            lpReal_t mask = 1;

            auto all = std::tuple_cat(self, neigh);
            auto diff = std::make_tuple(mask, xDist, yDist, zDist);
            callFun3(nFunc, diff, all, force);
        }

        for (int i = threadIdx.y; i < particleLinkCount; i = i + blockDim.y) {
            pInd_t pIndOther = linkedPartIndInCachePtr[getIndexInBlock(cacheBlockIdx, threadIdx.x, i, indexBlockSize)];
            // this does not work, or well it works on one rank but not multi rank
            //lInd_t linkSideInd = getIndexInBlock(cacheBlockIdx, threadIdx.x, i, linkSideBlockCount);

            lInd_t linkSideInd = linkCTIndPtr[getIndexInBlock(cacheBlockIdx, threadIdx.x, i, indexBlockSize)];

            lInd_t lInd = particleLinkIdPtr[pInd + (i * numPraticlesAllocatedLink)];//accel.lAccel.getPartLinkId(i, pInd);

            auto cordOther = getAtIndTuple(pIndOther, cordArgShrdPtr);
            auto neigh = getAtIndTuple(pIndOther, neighArgsShrdPtr);

            auto ct = getAtIndTupleFlagged(linkSideInd, linkSideArgs);
            auto link = getAtIndTuple(lInd, linkArgs);

            lpReal_t xDist = std::get<0>(cord) - std::get<0>(cordOther);//xCord - xCordOth;
            lpReal_t yDist = std::get<1>(cord) - std::get<1>(cordOther);//yCord - yCordOth;
            lpReal_t zDist = std::get<2>(cord) - std::get<2>(cordOther);//zCord - zCordOth;

            lpReal_t mask = 1;
            auto diff = std::make_tuple(mask, xDist, yDist, zDist);

            if constexpr (CombinedSmemPairTune::FUSED_NEIGH) {
                auto allNeigh = std::tuple_cat(self, neigh);
                callFun3(nFunc, diff, allNeigh, force);

            }

            // uint8_t state = statePtr[lInd];
            uint8_t state = sideStatePtr[linkSideInd];
            if (state == 0) { mask = 0; }
            diff = std::make_tuple(mask, xDist, yDist, zDist);
            auto allLink = std::tuple_cat(self, neigh, link);
            callFun4(lFunc, diff, allLink, ct, force);

            setAtIndTupleFlagged(linkSideInd, linkSideArgs, ct);
        }
    }
    if constexpr (CombinedSmemPairTune::THREADS_PER_BLOCK != CombinedSmemPairTune::X_DIMM_THREADS) {
        // gather from different Y blocks if there are any
        __syncthreads();

        auto forceShrd = makeSharedPtrs(&baseShardPtr[0],
                                        CombinedSmemPairTune::THREADS_PER_BLOCK, forceArgs);

        setAtIndTuple(threadIdx.x + (blockDim.x * threadIdx.y), forceShrd, force);
        __syncthreads();

        if (threadIdx.y == 0) {
            auto forceOut = force;
            for (int i = 1; i < blockDim.y; i++) {
                auto forceOther = getAtIndTuple(threadIdx.x + (blockDim.x * i), forceShrd);
                addTuple(forceOut, forceOther);
            }
            setAtIndTuple(pInd, forceArgs, forceOut);
        }
    } else {
        setAtIndTuple(pInd, forceArgs, force);
    }
}


#endif

template<class Policy, typename NFun, typename LFun, class FullArgs>
void CombinedAccelSmem::iterateCombined(Policy policy, ManVec<pInd_t> &indexes, ParticleData &pData,
                                             LinkData &lData, int cacheSizeLimit,
                                             NFun &nFunc, LFun lFunc, FullArgs args) {
#ifdef GPU
    //
    uint32_t blockSizeX = CombinedSmemPairTune::X_DIMM_THREADS;
    uint32_t blockSizeY = CombinedSmemPairTune::THREADS_PER_BLOCK / CombinedSmemPairTune::X_DIMM_THREADS;
    uint32_t grd = 1 + (indexes.size() / CombinedSmemPairTune::X_DIMM_THREADS);

    dim3 threads{blockSizeX, blockSizeY, 1};

    int sharedSize = 3* sizeof(hpReal_t );
    auto [neighArgs,linkSideArgs,linkArgs,forceArgs] = args.split();

    //std::apply([&](auto... t){sharedSize += sizeof(t);}, neighArgs);

    sharedSize += std::tuple_size<decltype(neighArgs)>{}* sizeof(lpReal_t); // whatever the neigh part of the pack is

    sharedSize *= cacheSizeLimit;

    // std::cout << "shared size " << sharedSize << std::endl;
#if (ENABLE_CUDA==1)
    if(sharedSize > 48000) {
        // really 48k
        getErrorDev((cudaFuncSetAttribute(
            pairIteratorCombinedCache<NFun, LFun, FullArgs>,cudaFuncAttributeMaxDynamicSharedMemorySize,sharedSize
        )));
    }

#endif
    // getErrorDevApi(cudaFuncSetAttribute(pairIteratorCombinedCache<NFun, LFun, FullArgs>, cudaFuncAttributePreferredSharedMemoryCarveout, cudaSharedmemCarveoutMaxShared));
    if (indexes.size() != 0)

        getErrorDev(((pairIteratorCombinedCache<<<grd, threads, sharedSize>>>(indexes.data(),
                nFunc, lFunc,

                activeInBlock.data(),
                blockIndexes.data(),
                cachingIndexes.data(),

                linkCTInd.data(),
                linkedPartIndInCache.data(),
                lAccel.linksPerParticle.data(),
                neighPartIndInCache.data(),
                numNeighsPossiblyPruned.data(),
                selfPartIndInCache.data(),
                lData.state.data(),
                sideState.data(),

                pData.xCord.data(),
                pData.yCord.data(),
                pData.zCord.data(),
                lAccel.particleLinkId.data(), lAccel.numPraticlesAllocated,
                indexes.size(),
                indexBlockSize, lAccel.allocatedMaxLinks*128, cacheSizeLimit,
                args
            )
        )));

    // Iterator::syncAll();
#else
#endif
}

#ifdef SMEM_LINKS

template<class Policy, typename LFun, class FullArgs>
void CombinedAccelSmem::iterateLinks(Policy policy, ParticleData &pData, LinkData &lData,
                  LFun &lFunc, FullArgs args) {



    uint32_t sharedSize = 0;


    auto [partArgs,linkSideArgs,linkArgs] = args.split();

    sharedSize += (sizeof(hpReal_t) + sizeof(lpReal_t)) * 3;
    sharedSize *= cacheCountLink;

    uint32_t threads = 512;
    uint32_t grid = numLinkBlocks;


    getErrorDev(((pairIteratorLinksCache<<<grid, threads, sharedSize>>>(
        lData.ownedIndexes.data(), lData.ownedIndexes.size(),
        lFunc,
        activeInBlockLink.data(),
        // blockIndexes.data(),
        cachingIndexesLink.data(),
        localPartIndACache.data(), localPartIndBCache.data(),
        lData.sideIndEndA.data(), lData.sideIndEndB.data(), lData.state.data(),
        sideState.data(),
        cacheCountLink,
        linksPerBlock,
        args
    ))));

    Iterator::syncAll();

}
#endif

#endif //PHELL_COMBINEDACCELSMEM_H

