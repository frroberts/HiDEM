//
// Created by Fredrik Robertsen on 05/11/2022.
//

#ifndef PHELL_COMBINEDACCELSMEM_H
#define PHELL_COMBINEDACCELSMEM_H


#include "CombinedAccel.h"
#include <ParallelReduce.h>

#include <Logger/Logger.h>
#include <Transport/CommunicatorIndividual.h>

#ifdef GPU

#include "DevPortabilityLayer.h"
#include <Util/Unique.h>

#if (ENABLE_CUDA==1)

#include <cub/cub.cuh>

namespace cubX = cub;
#elif (ENABLE_HIP==1)
//#include <cub/cub.cuh>

#include <hipcub/hipcub.hpp>
namespace cubX = hipcub;
#endif
#endif

__host__ __device__
    int getIndexInBlock(int blockId, int threadId, int i, int indexBlocSize, int accelxGrid) {
        //
        int startOffset = indexBlocSize*blockId;
        int inBlockOffset = (i*accelxGrid) + threadId;
        return startOffset + inBlockOffset;
    }

class CombinedAccelSmem : public CombinedAccel {
public:

#if ENABLE_HIP
    static constexpr int accelTotThreads = 512; // total threads in threadblock
    static constexpr int accelxGrid = 128; // threads in x direction, rest will be filled with y, totThreads%accelxGrid == 0
    static constexpr int numInCache = 620; // total number of particles in cache
#else
    static constexpr int accelTotThreads = 384; // total threads in threadblock
    static constexpr int accelxGrid = 128; // threads in x direction, rest will be filled with y, totThreads%accelxGrid == 0
    static constexpr int numInCache = 620; // total number of particles in cache
#endif
    static constexpr int indPerThread = (numInCache / accelxGrid) +
                                        8; // size of index array per thread, numInCache/accelxGrid +1 for ciel, +1 for new indexes
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

    ManVec<pInd_t> nonCachableIndesxes;

    ManVec<uint8_t> sideState;

    CombinedAccelSmem() {
        Logger::writeAtFirst(fmt::format("cache space reserved combined {} B \n",
                                         (3 * sizeof(hpReal_t) * numInCache) + (7 * sizeof(lpReal_t) * numInCache)));
    }

    template<class Policy, typename NFun, typename LFun, class FullArgs>
    void iterateCombinedPre(Policy policy, ParticleData &pData, LinkData &lData,
                            NFun &nFunc, LFun lFunc, FullArgs args) {


        if (nonCachableIndesxes.size() != 0) {
            CombinedAccel::iterateCombined(policy, nonCachableIndesxes, pData, lData, nFunc, lFunc, args);
        }
#ifdef ENABLE_ASYNC_COMM

        iterateCombined(policy, preCommIndexes, pData, lData, nFunc, lFunc, args);

#warning "compiling async comm"
#else
        iterateCombined(policy, pData.ownedIndexes, pData, lData, nFunc, lFunc, args);
#endif
    }

    template<class Policy, typename NFun, typename LFun, class FullArgs>
    void iterateCombinedPost(Policy policy, ParticleData &pData, LinkData &lData,
                             NFun &nFunc, LFun lFunc, FullArgs args) {
#ifdef ENABLE_ASYNC_COMM

            iterateCombined(policy, postCommIndexes, pData, lData, nFunc, lFunc, args);

#endif
    }

    template <class T>
    void generatePrePostCommIndexes(ParticleData& pData, LinkData& lData, T& comm) {
        xvtxRangePush("generatePrePostCommIndexes");
#ifdef ENABLE_ASYNC_COMM

        int lineLen = accelxGrid; // line length in particles

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

        if (firstCount % accelxGrid != 0) {
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
        //
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

    void generateIndexes(ParticleData &pData, LinkData &lData, double searchDistance, double interactDist) {
        //
        RangeV2 r5("accel generate", 2);

        int blocks = 1 + (iterationOrder.size() / accelxGrid);

        int maxNeighLinked = nList.maxNeighbours;// std::max(lData.allocatedMaxLinks, static_cast<int>(nList.maxNeighbours));
        indexBlockSize = maxNeighLinked * accelxGrid;

        cachingIndexes.resize(blocks * numInCache, false);
        activeInBlock.resize(blocks, false);

        linkedPartIndInCache.resize(blocks * indexBlockSize, false);
        neighPartIndInCache.resize(blocks * indexBlockSize, false);
        linkCTInd.resize(blocks * indexBlockSize, false);

        selfPartIndInCache.resize(iterationOrder.size(), false);

        blockIndexes.resize(pData.numLocal, false);
        auto blockIndexPtr = blockIndexes.data();
        Iterator::iterate(OffloadExec(),[=] DEVLAMBDA (int i) {
            blockIndexPtr[i] = -1;
        }, pData.numLocal);

        Iterator::syncAll();

        auto range = xvtxRangeStart("accel");

#ifdef GPU
        // change indexes to AoSoA layout ?

        // GPU only
        auto start = std::chrono::steady_clock::now();


        DevKernels2::iterateNonExitKern<<<1 + (iterationOrder.size() / accelxGrid), accelxGrid>>>([]
                __device__
                (NeighbourGrid &grid, NeighAccel &nList, LinkData &lData, CombinedAccelSmem &accel) {
            int idx = blockIdx.x * blockDim.x + threadIdx.x;
            // fill indexes with int max

            __shared__
            SharedUniq<CombinedAccelSmem::accelxGrid, CombinedAccelSmem::indPerThread, CombinedAccelSmem::numInCache> uniq;

            __shared__ int numCurrentlyCached;
            __shared__ int maxIterations;
            int currentRow;
            numCurrentlyCached = 0;
            maxIterations = 0;
            currentRow = 0;
            // set all indexes in shared to int max
            __shared__ pInd_t indexesShrd[CombinedAccelSmem::indPerThread * CombinedAccelSmem::accelxGrid];
            for (int i = threadIdx.x;
                 i < CombinedAccelSmem::indPerThread * CombinedAccelSmem::accelxGrid; i += blockDim.x) {
                indexesShrd[i] = INT32_MAX;
            }

            __syncthreads();
            // todo everyone can keep track of current row
            // define add index lambda
            // this work by adding by adding the current index to the next row
            // then grab the unique vales from the entire column
            auto addIndex = [&](pInd_t ind) {
                indexesShrd[threadIdx.x + (currentRow * blockDim.x)] = ind;

                currentRow++;
                __syncthreads();

                // make sure we are not over the current row
                if (currentRow >= CombinedAccelSmem::indPerThread - 1) {

                    // if we are get uniques from cached indexes
                    bool ret = true;

                    // get the unique vales in the shared cache
                    ret = uniq.get(indexesShrd, numCurrentlyCached, accel.activeInBlock);

                    currentRow = ceil((float) numCurrentlyCached / (float) CombinedAccelSmem::accelxGrid);

                    return ret;
                } else {
                    // if not return true and continue
                    return true;
                }
            };

            pInd_t pInd = INT32_MAX;
            // add own index to cache
            if (idx < accel.iterationOrder.size()) {
                pInd = accel.iterationOrder[idx];
                // add block index to blockIndexes
                accel.blockIndexes[pInd] = blockIdx.x;
            } else {
                // dont add it ? or do we add the last value, no it will fail on single ranks
            }
            addIndex(pInd);

            // get max neihgbours in block
            int numNeighs = 0;
            if (idx < accel.iterationOrder.size()) {
                numNeighs = nList.numNeighs[pInd];
            }
            // all threads need to call addIndex the same number of time
            // figure out who has the most
            atomicMax(&maxIterations, numNeighs);
            __syncthreads();

            // for max neibhbours
            for (int k = 0; k < maxIterations; ++k) {

                //  if under current neighs add pIndOther, else add int32_max as a filler
                pInd_t pIndOther = INT32_MAX;
                if (k < numNeighs) {
                    pIndOther = nList.getPartNeighId(k, pInd);
                }
                if (!addIndex(pIndOther))
                    return;
            }

            // do the same for the linked particles
            int particleLinkCount = 0;
            if (idx < accel.iterationOrder.size()) {
                particleLinkCount = accel.lAccel.linksPerParticle[pInd];
            }
            maxIterations = 0;
            __syncthreads();

            atomicMax(&maxIterations, particleLinkCount);
            __syncthreads();

            for (int k = 0; k < maxIterations; ++k) {
                pInd_t pIndOther = INT32_MAX;
                if (k < particleLinkCount) {
                    lInd_t lInd = accel.lAccel.getPartLinkId(k, pInd);
                    pIndOther = lData.localPartIndB[lInd];
                    if (pIndOther == pInd) {
                        pIndOther = lData.localPartIndA[lInd];
                    }
                }
                if (!addIndex(pIndOther))
                    return;
            }

            // last get unique
            if (!uniq.get(indexesShrd, numCurrentlyCached, accel.activeInBlock))
                return;

            // dump out cache indexes
            for (int i = threadIdx.x; i < numCurrentlyCached; i += blockDim.x) {
                accel.cachingIndexes[(blockIdx.x * numInCache) + i] = indexesShrd[i];
            }

            // write out how many are in the cache
            if (threadIdx.x == 0) {
                accel.activeInBlock[blockIdx.x] = numCurrentlyCached;
            }
        }, grid, nList, lData, *this);

        Iterator::syncAll();


        // for each block, build the iteration table with cache indexes
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        //std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";

        start = std::chrono::steady_clock::now();

        // generate the cache indexes for the neighbours and links
        // that is, at what index in the cache is the local index found at
        Iterator::iterateBlocked(OffloadExec(), [=] __device__(int b, int i, CombinedAccelSmem &accel,
                                                               ParticleData &pData, LinkData &lData,
                                                               NeighAccel &nList) {
            pInd_t ind = ((b * accelxGrid) + i);

            if (ind >= accel.iterationOrder.size())
                return;
            pInd_t pInd = accel.iterationOrder[ind];

            // uncachable block
            int activeInBlock = accel.activeInBlock[b];
            if (activeInBlock == -1) {
                return;
            }

            // this is a binary search
            auto getCachedIndex = [&](pInd_t pInd) {
                int loc;
                int left = 0;
                int right = activeInBlock - 1;
                while (left <= right) {
                    loc = (left + right) / 2;
                    auto val = accel.cachingIndexes[(b * numInCache) + loc];
                    if (val == pInd) {
                        return loc;
                    } else if (val < pInd)
                        left = loc + 1;
                    else
                        right = loc - 1;
                }
                return -1;
            };
            accel.selfPartIndInCache[ind] = getCachedIndex(pInd);

            for (size_t k = 0; k < nList.numNeighs[pInd]; k++) {
                pInd_t pIndOther = nList.getPartNeighId(k, pInd);
                accel.neighPartIndInCache[getIndexInBlock(b, threadIdx.x, k, accel.indexBlockSize, accel.accelxGrid)] = getCachedIndex(pIndOther);
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
                accel.linkCTInd[getIndexInBlock(b, threadIdx.x, k, accel.indexBlockSize,accel.accelxGrid)] = ctInd;
                accel.linkedPartIndInCache[getIndexInBlock(b, threadIdx.x, k, accel.indexBlockSize,accel.accelxGrid)] = getCachedIndex(pIndOther);
            }

        }, (size_t) accelxGrid, iterationOrder.size(), *this, pData, lData, nList);
        Iterator::syncAll();

        end = std::chrono::steady_clock::now();
        elapsed_seconds = end - start;
#else
        // not quite 100% working ?
        int numBlocks = 1 + (iterationOrder.size() / accelxGrid);
        for (int b = 0; b < numBlocks;b++) {
            std::unordered_map<int, int> cacheHits;
            //
            auto addIndex = [&](pInd_t pInd){
                if(cacheHits.find(pInd) == cacheHits.end()){
                    cacheHits[pInd] = 0;
                }
                cacheHits[pInd]++;
            };

            for (int i = 0; i < accelxGrid; ++i) {
                int idx = b * accelxGrid + i;
                //
                pInd_t pInd = INT32_MAX;
                // add own index to cache
                if (idx < iterationOrder.size()) {
                    pInd = iterationOrder[idx];
                    // add block index to blockIndexes
                    blockIndexes[pInd] = b;
                } else {
                    // dont add it ? or do we add the last value, no it will fail on single ranks
                    continue;
                }

                addIndex(pInd);

                nList.iteratePossibleNeighbours([&](pInd_t pIndOther) {
                    addIndex(pIndOther);
                }, pInd, pData);

                for (int k = 0; k < nList.numNeighs[pInd]; ++k) {
                    pInd_t pIndOther = INT32_MAX;
                    lInd_t lInd = lData.getPartLinkId(k, pInd);
                    pIndOther = lData.localPartIndB[lInd];
                    if (pIndOther == pInd) {
                        pIndOther = lData.localPartIndA[lInd];
                    }
                    addIndex(pIndOther);

                }
                lAccel.iterateParticleLinksCT([&](lInd_t lInd, lInd_t ctInd, pInd_t pIndOther){
                    addIndex(pIndOther);
                }, pInd);

            }

            std::vector<int> cacheIds;
            for (auto p: cacheHits) {
                cacheIds.push_back(p.first);
            }
    /*
            std::sort(cacheIds.begin(), cacheIds.end(), [](auto a, auto b){
                return a.second < b.second;
            });
    */

            for(int i = 0; i < cacheIds.size(); i++){
                cachingIndexes[(b * numInCache) + i] = cacheIds[i];
            }
            // write out how many are in the cache
            activeInBlock[b] = (int) cacheIds.size();

            for (int i = 0; i < accelxGrid; ++i) {
                int ind = b * accelxGrid + i;
                //
                pInd_t pInd = INT32_MAX;
                // add own index to cache
                if (ind < iterationOrder.size()) {
                    pInd = iterationOrder[ind];
                    // add block index to blockIndexes
                    blockIndexes[pInd] = b;
                } else {
                    // dont add it ? or do we add the last value, no it will fail on single ranks
                    continue;
                }
                selfPartIndInCache[ind] = std::distance(cacheIds.begin(),
                                                        std::find(cacheIds.begin(), cacheIds.end(), pInd));

                for (size_t k = 0; k < nList.numNeighs[pInd]; k++) {
                    pInd_t pIndOther = nList.getPartNeighId(k, pInd);

                    neighPartIndInCache[ind + (k * iterationOrder.size())] =
                             std::distance(cacheIds.begin(),
                                                        std::find(cacheIds.begin(), cacheIds.end(), pIndOther)); // figure out what the real index here is actually
                }
                int particleLinkCount = lData.linksPerParticle[pInd];



                for (size_t k = 0; k < particleLinkCount; k++) {
                    //
                    lInd_t lInd = lData.getPartLinkId(k, pInd);

                    pInd_t pIndOther = lData.localPartIndB[lInd];
                    lInd_t ctInd;

                    if (pIndOther == pInd) {
                        pIndOther = lData.localPartIndA[lInd];
                        ctInd = lData.sideIndEndB[lInd];
                    } else {
                        ctInd = lData.sideIndEndA[lInd];
                    }

                    linkCTInd[ind + (k * iterationOrder.size())] = ctInd; // should be pind right ? no should be ind
                    linkedPartIndInCache[ind + (k * iterationOrder.size())] =
                            std::distance(cacheIds.begin(),std::find(cacheIds.begin(), cacheIds.end(), pIndOther));;
                }
            }
        }
#endif

        int min = 100000;
        int max = 0;
        //std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";
        ParallelReduce<int>::minMax(min, max, activeInBlock);

        int uncachable = ParallelReduce<int>::transformReduce(activeInBlock.begin(), activeInBlock.end(), 0, std::plus<int>(),
                                             []DEVCALLABLE(int blkAktive) {
                                                 if (blkAktive == -1) return 1;
                                                 return 0;
                                             });

        xvtxRangeEnd(range);

        // run select from iteration order here for non cachable ?
        nonCachableIndesxes.clear();
        for (int i = 0; i < blocks; i++) {
            if (activeInBlock[i] == -1) {
                for (int j = 0; j < accelxGrid; j++) {
                    //
                    int targetInd = (i * accelxGrid) + j;
                    // size of the iteration otder here ?
                    if (targetInd < iterationOrder.size())
                        nonCachableIndesxes.push_back(iterationOrder[targetInd]);
                }
            }
        }

        sideState.resize(linkCTInd.size());
        auto statePtr = lData.state.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i, CombinedAccelSmem &accel) {
            pInd_t pInd = accel.iterationOrder[i];
            accel.lAccel.iterateParticleLinksCT([&](auto lInd, auto sideInd, auto pIndOther) {
                accel.sideState[sideInd] = statePtr[lInd];
            }, pInd);
        }, iterationOrder.size(), *this);
        Iterator::syncAll();



        Logger::writeAtFirst(fmt::format(
            "Cache stats, max space {}, world stats: max used {}, min used {}, max uncachable {} \n",
            numInCache, Logger::getMaxValWorld(max), Logger::getMaxValWorld(min), uncachable));

        //Logger::writeAtFirst(fmt::format("Max cached combined {} out of {} \n", Logger::getStats(max, ""), numInCache));
        //Logger::writeAtFirst(fmt::format("Number of uncachable blocks {} \n", Logger::getStats(uncachable, "")));
        //Logger::writeAtFirst(fmt::format("Number of cachable blocks {} \n", Logger::getStats(blocks - uncachable, "")));


        //xvtxRangePop();
    }

    template<class Policy, typename NFun, typename LFun, class FullArgs>
    void iterateCombined(Policy policy, ManVec<pInd_t> &indexes, ParticleData &pData, LinkData &lData,
                         NFun &nFunc, LFun lFunc, FullArgs args);

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
            //auto sideA = getAtIndTuple(bParticleInd, partArgs);
            //auto sizeB = getAtIndTuple(bParticleInd, partArgs);

            uint8_t initialState = lData.state[lInd];

            auto all = std::tuple_cat(partA, partB);

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

union dbl {
    double d;
    uint32_t parts[2];
};

/*
__device__
inline void storeShared(int ind, double val, double *ptr, int offset) {
    dbl storeVal;
    storeVal.d = val;
    uint32_t *ptr4 = reinterpret_cast<uint32_t *>(ptr);

    ptr4[ind] = storeVal.parts[0];
    ptr4[ind + offset] = storeVal.parts[1];

}

__device__
inline double loadShared(int ind, double *ptr, int offset) {
    dbl val;
    uint32_t *ptr4 = reinterpret_cast<uint32_t *>(ptr);

    val.parts[0] = ptr4[ind];
    val.parts[1] = ptr4[ind + offset];

    return val.d;

}
*/

__device__
inline void storeShared(int ind, double val, double *ptr, int offset) {
    //
    ptr[ind] = val;
}

__device__
inline double loadShared(int ind, double *ptr, int offset) {

    return ptr[ind];
}

__device__
inline void storeShared(int ind, float val, float *ptr, int offset) {
    //
    ptr[ind] = val;
}

__device__
inline float loadShared(int ind, float *ptr, int offset) {

    return ptr[ind];
}

template<typename NFun, typename LFun, class FullArgs>
__global__
void
pairIteratorCombinedCache(pInd_t *indexes,
                          CombinedAccelSmem &accel, NFun nFunc,
                          LFun lFunc, pInd_t *activeInBlockPtr, pInd_t *blockIndexesPtr, pInd_t *cachingIndexesPtr,
                          lInd_t *linkCTIndPtr,
                          int16_t *linkedPartIndInCachePtr, int *linksPerParticlePtr,
                          int16_t *neighPartIndInCachePtr,
                          int *numNeighsPtr,
                          int16_t *selfPartIndInCachePtr, uint8_t *statePtr,
                          uint8_t *sideStatePtr,
                          hpReal_t *xCordPtr, hpReal_t *yCordPtr, hpReal_t *zCordPtr,
                          size_t numIndexes,
                          int indexBlockSize, const FullArgs args) {

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

    const auto cordArgs = std::make_tuple(xCordPtr, yCordPtr, zCordPtr);
    const auto cordArgShrdPtr = makeSharedPtrs(&baseShardPtr[shrdOffset], CombinedAccelSmem::numInCache, cordArgs);

    shrdOffset += CombinedAccelSmem::numInCache * 3 * sizeof(hpReal_t );
    const auto neighArgsShrdPtr = makeSharedPtrs(&baseShardPtr[shrdOffset], CombinedAccelSmem::numInCache, neighArgs);


    int nidx = threadIdx.x;

    //int iteratedLinks = 0;
    //int iteratedNeighs = 0;

    auto force = zeroTuple(forceArgs);

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
    if (activeInBlock == -1) {
        return;
        // handled by other kernel
    } else {
        for (int i = tid; i < activeInBlock; i += (blockDim.x * blockDim.y)) {
            //
            pid_t readInd = cachingIndexesPtr[(cacheBlockIdx * CombinedAccelSmem::numInCache) + i];
            setAtIndTuple(i, cordArgShrdPtr, getAtIndTuple(readInd, cordArgs));
            setAtIndTuple(i, neighArgsShrdPtr, getAtIndTuple(readInd, neighArgs));
        }

        __syncthreads();
        if (idx >= numIndexes) return;
        pInd_t pInd = indexes[idx];

        pInd_t pIndC = selfPartIndInCachePtr[idxCache]; // not idx

        auto cord = getAtIndTuple(pIndC, cordArgShrdPtr);
        auto self = getAtIndTuple(pIndC, neighArgsShrdPtr);

        int numNeighs = numNeighsPtr[pInd];
        int particleLinkCount = linksPerParticlePtr[pInd];

#pragma unroll 3
        for (size_t i = threadIdx.y; i < numNeighs; i = i + blockDim.y) {
            pInd_t pIndOther = neighPartIndInCachePtr[getIndexInBlock(cacheBlockIdx, threadIdx.x, i, indexBlockSize, blockDim.x)];
            auto cordOther = getAtIndTuple(pIndOther, cordArgShrdPtr);
            auto neigh = getAtIndTuple(pIndOther, neighArgsShrdPtr);

            lpReal_t xDist = std::get<0>(cord) - std::get<0>(cordOther);//xCord - xCordOth;
            lpReal_t yDist = std::get<1>(cord) - std::get<1>(cordOther);//yCord - yCordOth;
            lpReal_t zDist = std::get<2>(cord) - std::get<2>(cordOther);//zCord - zCordOth;

            lpReal_t mask = 1;

            auto all = std::tuple_cat(self, neigh);
            auto diff = std::make_tuple(mask, xDist, yDist, zDist);
            callFun3(nFunc, diff, all, force);

            //iteratedNeighs++;
        }

#pragma unroll 3
        for (int i = threadIdx.y; i < particleLinkCount; i = i + blockDim.y) {
            pInd_t pIndOther = linkedPartIndInCachePtr[getIndexInBlock(cacheBlockIdx, threadIdx.x, i, indexBlockSize, blockDim.x)];
            lInd_t ctInd = linkCTIndPtr[getIndexInBlock(cacheBlockIdx, threadIdx.x, i, indexBlockSize, blockDim.x)];
            lInd_t lInd = accel.lAccel.getPartLinkId(i, pInd);

            auto cordOther = getAtIndTuple(pIndOther, cordArgShrdPtr);
            auto neigh = getAtIndTuple(pIndOther, neighArgsShrdPtr);
            auto ct = getAtIndTuple(ctInd, linkSideArgs);
            auto link = getAtIndTuple(lInd, linkArgs);

            lpReal_t mask = 1;
            // uint8_t state = statePtr[lInd];
            uint8_t state = sideStatePtr[ctInd];
            if (state == 0) { mask = 0; } // can i get rid of this read

            lpReal_t xDist = std::get<0>(cord) - std::get<0>(cordOther);//xCord - xCordOth;
            lpReal_t yDist = std::get<1>(cord) - std::get<1>(cordOther);//yCord - yCordOth;
            lpReal_t zDist = std::get<2>(cord) - std::get<2>(cordOther);//zCord - zCordOth;

            auto all = std::tuple_cat(self, neigh, link);
            auto diff = std::make_tuple(mask, xDist, yDist, zDist);
            callFun4(lFunc, diff, all, ct, force);

            setAtIndTuple(ctInd, linkSideArgs, ct);
        }
    }
    __syncthreads();

    // todo remove 2 int offset here
    auto forceShrd = makeSharedPtrs(&baseShardPtr[sizeof(int)*2*CombinedAccelSmem::accelxGrid],
                                    CombinedAccelSmem::accelxGrid, forceArgs);

    if (threadIdx.y == 0) {
        setAtIndTuple(nidx, forceShrd, zeroTuple(forceShrd));
    }

    __syncthreads();

    // one y block at a time, write out the values to shared
    for (int i = 0; i < blockDim.y; ++i) {
        if (i == threadIdx.y) {
            addAtIndTuple(nidx, forceShrd, force);
        }
        __syncthreads();
    }

    __syncthreads();
    // dump shared to vram
    if (threadIdx.y == 0) {
        if (idx >= numIndexes) return;
        pInd_t pInd = indexes[idx];

        setAtIndTuple(pInd, forceArgs, getAtIndTuple(nidx, forceShrd));
    }
}


#endif

template<class Policy, typename NFun, typename LFun, class FullArgs>
void CombinedAccelSmem::iterateCombined(Policy policy, ManVec<pInd_t> &indexes, ParticleData &pData,
                                             LinkData &lData,
                                             NFun &nFunc, LFun lFunc, FullArgs args) {
#ifdef GPU
    RangeV2("Neigh force", 2);
    //
    uint32_t blockSizeX = CombinedAccelSmem::accelxGrid;
    uint32_t blockSizeY = CombinedAccelSmem::accelTotThreads / CombinedAccelSmem::accelxGrid;
    uint32_t grd = 1 + (indexes.size() / CombinedAccelSmem::accelxGrid);

    dim3 threads{blockSizeX, blockSizeY, 1};

    int sharedSize = 3* sizeof(hpReal_t );
    auto [neighArgs,linkSideArgs,linkArgs,forceArgs] = args.split();

    //std::apply([&](auto... t){sharedSize += sizeof(t);}, neighArgs);

    sharedSize += std::tuple_size<decltype(neighArgs)>{}* sizeof(lpReal_t); // whatever the neigh part of the pack is

    sharedSize *= numInCache;

    // std::cout << "shared size " << sharedSize << std::endl;


    // getErrorDevApi(cudaFuncSetAttribute(pairIteratorCombinedCache<NFun, LFun, FullArgs>, cudaFuncAttributePreferredSharedMemoryCarveout, cudaSharedmemCarveoutMaxShared));
    if (indexes.size() != 0)
        getErrorDev(((pairIteratorCombinedCache<<<grd, threads, sharedSize>>>(indexes.data(),
                *this, nFunc, lFunc,

                activeInBlock.data(),
                blockIndexes.data(),
                cachingIndexes.data(),

                linkCTInd.data(),
                linkedPartIndInCache.data(),
                lAccel.linksPerParticle.data(),
                neighPartIndInCache.data(),
                nList.numNeighs.data(),
                selfPartIndInCache.data(),
                lData.state.data(),
                sideState.data(),

                pData.xCord.data(),
                pData.yCord.data(),
                pData.zCord.data(),

                indexes.size(),
                indexBlockSize,
                args
            )
        )));

    // Iterator::syncAll();
#else
#endif
}

#endif //PHELL_COMBINEDACCELSMEM_H

