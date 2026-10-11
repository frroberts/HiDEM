//
// Created by Fredrik Robertsen on 17/01/23.
//

#ifndef PHELL_HIDEMCOMBINEDSIMDACCEL_H
#define PHELL_HIDEMCOMBINEDSIMDACCEL_H

#include "../../PLiB/Accel/CombinedAccel.h"

#ifdef ENABLE_SIMD

#include <vectorclass.h>
#include <vectorf256.h>
#include <vectormath_exp.h>
#include <immintrin.h>

#endif

class CombinedAccelSIMD : public CombinedAccel {
public:
    //
#ifdef ENABLE_SIMD
        template<class Policy, typename NFun, typename LFun, class FullArgs>
    void iterateCombinedPre(Policy policy, HiDEMParticleData &pData, HiDEMLinkData &lData,
                         NFun &nFunc, LFun &lFunc, FullArgs args) {
// #define ENABLE_ASYNC_COMM
#ifdef ENABLE_ASYNC_COMM

        iterateCombined(policy, preCommIndexes, pData, lData, nFunc, lFunc, args);

#warning "compiling async comm"
#else
        iterateCombined(policy, pData.ownedIndexes, pData, lData, nFunc, lFunc, args);
#endif
    }

    template<class Policy, typename NFun, typename LFun, class FullArgs>
    void iterateCombinedPost(Policy policy, HiDEMParticleData &pData, HiDEMLinkData &lData,
                         NFun &nFunc, LFun &lFunc, FullArgs args) {
#ifdef ENABLE_ASYNC_COMM

            iterateCombined(policy, postCommIndexes, pData, lData, nFunc, lFunc, args);

            Iterator::syncAll();
#endif
    }

// iterateCombinedSimd<Vec16f, Vec16i, __mmask16, 16>(policy, indexes, pData, lData,  nFunc, lFunc);

    template< typename VecT, typename VecIT, typename MaskT, int width, class Policy, typename NFun, typename LFun, class FullArgs>
    void iterateCombinedSimd(Policy policy, ManVec<pInd_t> &indexes, HiDEMParticleData &pData, HiDEMLinkData &lData,
                         NFun nFunc, LFun lFunc, FullArgs args) {

        auto pack = args.split();

        auto neighArgs = std::get<0>(pack);
        auto linkArgs = std::get<1>(pack);
        auto linkSideArgs = std::get<2>(pack);
        auto forceArgs = std::get<3>(pack);

        int indexSize = indexes.size();

        int blocks;// = 2560;
        int blockSize;// = 1 + (indexSize / blocks);

        blockSize = 128  ;
        blocks = 1 + (indexSize / blockSize);

        std::tuple<double *, double *, double *> cordTup = std::make_tuple(pData.xCord.data(), pData.yCord.data(), pData.zCord.data());


#pragma omp parallel
        {
            MaskT mask;
            clearMask(mask);


            int numMasks = 0;
            alignas(64) MaskT selfMasks[width];
            alignas(64) int outIndex[width];

            // used to build input vector
            alignas(64) int selfInd[width];
            alignas(64) int neighInd[width];

            for (int i = 0; i < width; ++i) {
                clearMask(selfMasks[i]);
            }

            int filledSlots = 0;

            alignas(64) pInd_t linkSideInd[width];

            MaskT fullMask;

            auto doNeighs = [&]() {
                auto dist = computePsDiffPdTup(cordTup, numMasks, selfMasks, outIndex, neighInd);
                auto forceTup = zeroTupFromPtr(forceArgs);

                VecIT neighIndVec = getNeighInd(neighInd);

                auto self = bCastFromMasksTuple(outIndex, numMasks, selfMasks, neighArgs);

                auto neigh = std::apply([&](auto ...x) {
                    return std::make_tuple(gather_ps(neighIndVec, x)...);
                }, neighArgs);

                VecT one = b2fMask(fullMask);

                auto diff = std::tuple_cat(std::make_tuple(one), dist);
                auto all = std::tuple_cat(self, neigh);

                callFun3(nFunc, diff, all, forceTup);

                reduceSumByMasksTuple(forceArgs, numMasks, selfMasks, outIndex, forceTup);

            };

            auto doLinks = [&]() {
                auto dist = computePsDiffPdTup(cordTup, numMasks, selfMasks, outIndex, neighInd);

                auto forceTup = zeroTupFromPtr(forceArgs);

                VecIT neighIndVec = getNeighInd(neighInd);

                auto self = bCastFromMasksTuple(outIndex, numMasks, selfMasks, neighArgs);

                // this is probably the most optimal way to do this
                auto neigh = std::apply([&](auto ...x) {
                    return std::make_tuple(gather_ps(neighIndVec, x)...);
                }, neighArgs);

                auto linkSide = blockGatherTuple(linkSideInd, filledSlots, linkSideArgs);

                VecT one = b2fMask(fullMask);

                auto diff = std::tuple_cat(std::make_tuple(one), dist);

                auto all = std::tuple_cat(self, neigh); // incldue side link

                callFun4(lFunc, diff, all, linkSide, forceTup);

                blockScatterTuple(linkSideInd, filledSlots, linkSideArgs, linkSide);

                reduceSumByMasksTuple(forceArgs, numMasks, selfMasks, outIndex, forceTup);

            };

            // needs same treatment as links, dont inncrement num masks etc if there was really no active neighbours for the current praticles


#pragma omp for
            for (int b = 0; b < blocks; ++b) {
                //
                clearMask(mask);
                numMasks = 0;
                filledSlots = 0;
                for (int ii = b * blockSize; ii < indexSize && ii < (b + 1) * blockSize; ++ii) {
                    pInd_t pInd = indexes[ii];
                    pData.xForce[pInd] = 0;
                    pData.yForce[pInd] = 0;
                    pData.zForce[pInd] = 0;
                    pData.uForce[pInd] = 0;
                    pData.vForce[pInd] = 0;
                    pData.wForce[pInd] = 0;
                    int end = nList.numNeighs[pInd];
                    for (int i = 0; i < end; ++i) {
// fill a slot
                        setMask(mask, filledSlots);// mask = mask | maskLookup[filledSlots];
                        selfInd[filledSlots] = pInd;
                        neighInd[filledSlots] = nList.getPartNeighId(i, pInd);

                        filledSlots++;
                        if (filledSlots >= width) {
                            outIndex[numMasks] = pInd;
                            selfMasks[numMasks] = mask;
                            numMasks++;

                            fullMask = getFullMask(numMasks, selfMasks);

                            doNeighs();

                            filledSlots = 0;
                            clearMask(mask);
                            numMasks = 0;

                        }
                    }
                    // push out current mask
                    selfMasks[numMasks] = mask;
                    clearMask(mask);
                    outIndex[numMasks] = pInd;
                    numMasks++;

                }
                if (filledSlots != 0) {
                    doNeighs();
                }

                for (int i = 0; i < width; ++i) {
                    clearMask(selfMasks[i]);

                    // selfMasks[i] = 0;
                }

                clearMask(mask);
                numMasks = 0;
                filledSlots = 0;

                for (int i = 0; i < width; ++i) {
                    outIndex[i] = 0;
                    selfInd[i] = 0;
                    neighInd[i] = 0;
                    linkSideInd[i] = 0;
                }

                for (int ii = b * blockSize; ii < indexSize && ii < (b + 1) * blockSize; ++ii) {
                    pInd_t pInd = indexes[ii];
                    int end = lAccel.linksPerParticle[pInd];
                    int numActive = 0;
                    for (int i = 0; i < end; ++i) {
                        lInd_t lInd = lAccel.getPartLinkId(i, pInd);

                        if (lData.state[lInd] == 0) {
                        } else {
                            setMask(mask, filledSlots);
                        }

                        numActive++;

                        selfInd[filledSlots] = pInd;
                        neighInd[filledSlots] = lAccel.otherPartLink[(lAccel.numPraticlesAllocated * i) + pInd];
                        lInd_t ctInd = lAccel.linkSideLind[(lAccel.numPraticlesAllocated * i) + pInd];
                        linkSideInd[filledSlots] = ctInd;
                        filledSlots++;

                        if (filledSlots == width) {

                            outIndex[numMasks] = pInd;
                            selfMasks[numMasks] = mask;
                            numMasks++;

                            fullMask = getFullMask(numMasks, selfMasks);

                            doLinks();

                            filledSlots = 0;
                            clearMask(mask);
                            numMasks = 0;
                        }
                    }
                    // push out current mask
                    // causes issues with lots of particles with no or 1 ? links ?
                    if (numActive != 0 && filledSlots != 0) {
                        // clearMask(selfMasks[numMasks]);
                        selfMasks[numMasks] = mask;
                        clearMask(mask);
                        outIndex[numMasks] = pInd;
                        numMasks++;
                    }
                }

                if (filledSlots != 0)
                    doLinks();

            }
        }

    }



    template<class Policy, typename NFun, typename LFun, class FullArgs>
    void iterateCombined(Policy policy, ManVec<pInd_t> &indexes, HiDEMParticleData &pData, HiDEMLinkData &lData,
                         NFun &nFunc, LFun &lFunc, FullArgs args) {
#if (ENABLE_SIMD512 == 1)
        iterateCombinedSimd<Vec16f, Vec16i, __mmask16, 16>(policy, indexes, pData, lData,  nFunc, lFunc, args);
#endif

#if (ENABLE_SIMD256 == 1)
        iterateCombinedSimd<Vec8f, Vec8i, __m256i, 8>(policy, indexes, pData, lData,  nFunc, lFunc, args);
#endif
    }

#endif


};


#endif //PHELL_HIDEMCOMBINEDSIMDACCEL_H
