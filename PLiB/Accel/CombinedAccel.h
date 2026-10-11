//
// Created by Fredrik Robertsen on 03/01/23.
//

#ifndef PHELL_HIDEMCOMBINEDACCEL_H
#define PHELL_HIDEMCOMBINEDACCEL_H

#include "NeighAccel.h"
#include "LinkAccelSide.h"

#include <ParallelSelect.h>
#include <TupleHelpers.h>
#include <Transport/Communicator.h>
#include <Transport/CommunicatorPacked.h>

#include "LoggerV2/RangeCollection.h"


class CombinedAccel : public ManagedParent {

public:

    NeighAccel nList;
    LinkAccelSide lAccel;
    NeighbourGrid grid;

    ManVec<pInd_t> preCommIndexes;
    ManVec<pInd_t> postCommIndexes;

    void generate(ParticleData &pData, LinkData &lData, double searchDistance, double interactDist) {
        //
        grid.generate(pData, searchDistance);
        nList.generateNeighbours(grid, pData, searchDistance, interactDist);
        lAccel.generateLinks(pData, lData);
    }

    template<class T>
    void generatePrePostCommIndexes(ParticleData &pData, LinkData &lData, T &comm){
        RangeV2 r("generate pre/post indexes", 3);

        preCommIndexes.clear();
        postCommIndexes.clear();

        std::vector<int> numOuthoingTargets;
        numOuthoingTargets.resize(pData.numLocal);
        for (int i = 0; i < pData.numLocal; i++) {
            numOuthoingTargets[i] = 0;
        }

        for (int i = 0; i < comm.outgoingIndexes.size(); i++) {
            numOuthoingTargets[comm.outgoingIndexes[i]]++;
        }
        int lineLen = 32; // line length in particles

        for (int i = 0; i < pData.ownedIndexes.size(); i += lineLen) {
            //
            bool commLine = false;
            for (int j = 0; j < lineLen; j++) {
                if (i + j >= pData.ownedIndexes.size())
                    continue;
                if (numOuthoingTargets[pData.ownedIndexes[i + j]] != 0) {
                    commLine = true;
                }
            }
            if (commLine) {
                for (int j = 0; j < lineLen; j++) {
                    if (i + j >= pData.ownedIndexes.size())
                        continue;
                    preCommIndexes.push_back(pData.ownedIndexes[i + j]);
                }

            } else {
                for (int j = 0; j < lineLen; j++) {
                    if (i + j >= pData.ownedIndexes.size())
                        continue;
                    postCommIndexes.push_back(pData.ownedIndexes[i + j]);
                }
            }
        }
    }

    template<class Policy, typename NFun, typename LFun, class FullArgs>
    void iterateCombinedPre(Policy policy, ParticleData &pData, LinkData &lData,
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
    void iterateCombinedPost(Policy policy, ParticleData &pData, LinkData &lData,
                         NFun &nFunc, LFun &lFunc, FullArgs args) {
#ifdef ENABLE_ASYNC_COMM

            iterateCombined(policy, postCommIndexes, pData, lData, nFunc, lFunc, args);

            Iterator::syncAll();
#endif
    }

    template<class Policy, typename NFun, typename LFun, class FullArgs>
    void iterateCombined(Policy policy, ManVec<pInd_t> &indexes, ParticleData &pData, LinkData &lData,
                         NFun &nFunc, LFun &lFunc, FullArgs &args) {
        // because OMP we cant use structured bindings here
        auto pack = args.split();

        auto neighArgs = std::get<0>(pack);
        auto linkArgs = std::get<1>(pack);
        auto linkSideArgs = std::get<2>(pack);
        auto forceArgs = std::get<3>(pack);

        //auto range = nvtxRangeStart("it");
        pData.iterateParticlesIndex(policy, [=] DEVCALLABLE(pInd_t pInd, ParticleData &pData,
                                                          CombinedAccel &accel, LinkData &lData) {

            hpReal_t xCord = pData.xCord[pInd];
            hpReal_t yCord = pData.yCord[pInd];
            hpReal_t zCord = pData.zCord[pInd];

            auto force = zeroTuple(forceArgs);

            auto self = getAtIndTuple(pInd, neighArgs);

            auto nFuncSetup = [&]  (pInd_t nInd) {

                auto neigh = getAtIndTuple(nInd, neighArgs);


                hpReal_t xCordOther = pData.xCord[nInd];
                hpReal_t yCordOther = pData.yCord[nInd];
                hpReal_t zCordOther = pData.zCord[nInd];

                lpReal_t mask = 1;

                lpReal_t xDist = xCord - xCordOther;
                lpReal_t yDist = yCord - yCordOther;
                lpReal_t zDist = zCord - zCordOther;

                auto all = std::tuple_cat(self, neigh);
                auto diff = std::make_tuple(mask, xDist, yDist, zDist);
                callFun3(nFunc, diff, all, force);
            };

            accel.nList.iteratePossibleNeighbours(nFuncSetup, pInd);

            accel.lAccel.iterateParticleLinksCT([&](lInd_t lInd, lInd_t ctInd, pInd_t pIndOther, LinkData &lData) {
                hpReal_t xCordOther = pData.xCord[pIndOther];
                hpReal_t yCordOther = pData.yCord[pIndOther];
                hpReal_t zCordOther = pData.zCord[pIndOther];

                auto neigh = getAtIndTuple(pIndOther, neighArgs);
                auto ct = getAtIndTuple(ctInd, linkSideArgs);
                auto link = getAtIndTuple(ctInd, linkArgs);


                lpReal_t xDist = xCord - xCordOther;
                lpReal_t yDist = yCord - yCordOther;
                lpReal_t zDist = zCord - zCordOther;

                lpReal_t mask = 1;
                if (lData.state[lInd] == 0) { mask = 0; }

                auto all = std::tuple_cat(self, neigh, link);
                auto diff = std::make_tuple(mask, xDist, yDist, zDist);
                callFun4(lFunc, diff, all, ct, force);

                setAtIndTuple(ctInd, linkSideArgs, ct);

            }, pInd, lData);


            setAtIndTuple(pInd, forceArgs, force);

        }, indexes, pData, *this, lData);

//         nvtxRangeEnd(range);
    }


    template<class Policy, typename LFun, class FullArgs>
    void iterateLinks(Policy policy, ParticleData &pData, LinkData &lData,
                         LFun &lFunc, FullArgs args) {
        //

        lData.iterateAllLinks(policy, [=] DEVCALLABLE(lInd_t lInd, pInd_t aParticleInd, pInd_t bParticleInd,
            LinkData &lData, LinkAccel &lAccel) {
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
            if(updatedState != initialState && updatedState == 0) {
                // break link in accel ?

            }
        }, lData, lAccel);

    }


};

#endif //PHELL_HIDEMCOMBINEDACCEL_H
