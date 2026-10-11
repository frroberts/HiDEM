//
// Created by Fredrik Robertsen on 7/7/24.
//

#ifndef LINKDATAHANDLER_H
#define LINKDATAHANDLER_H

#include <Util/DropReorder.h>
#include "Generators/ElementrayLink.h"
#include "ParticleData.h"
#include <LoggerV2/RangeCollection.h>

#include "ParallelListGen.h"

template<class UserData>
class LinkDataHandler : public UserData { 
    ManVec<uint8_t, ScratchAllocator<uint8_t>> scratchSpace;
    ManVec<lInd_t> reorderOrder;

public:
    void dropLinks(ManVec<lInd_t> &droppedLocalIndexes) {
        //nvtxRangePush("dropLinks links");

        ManVec<lInd_t> newOrder;
        getDropReorder(droppedLocalIndexes, newOrder, this->numLocal);

        size_t newSize = this->numLocal - droppedLocalIndexes.size();

        ManVec<lInd_t> droppedSideInd;
        droppedSideInd.resize(droppedLocalIndexes.size() * 2);

        auto droppedSideIndPtr = droppedSideInd.data();
        auto droppedLocalIndexesPtr = droppedLocalIndexes.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (ind_t i, LinkDataHandler<UserData> &lData) {
            auto lInd = droppedLocalIndexesPtr[i];
            droppedSideIndPtr[(i * 2) + 0] = lData.sideIndEndA[lInd];
            droppedSideIndPtr[(i * 2) + 1] = lData.sideIndEndB[lInd];
        }, droppedLocalIndexes.size(), *this);
        Iterator::syncAll();

        reorder(newOrder);

        dropSideData(droppedSideInd);

        resize(newSize);

        this->generateOwnedLinksList();
        //nvtxRangePop();
    }

    void dropUnusedLinks(GlobalLocalMapper<gKey_t, pInd_t> &pMapper) {
        xvtxRangePush("dropUnusedLinks links");
        ManVec<lInd_t> toBeDropped;
        for (size_t i = 0; i < this->numLocal; ++i) {
            ind_t pIndA = pMapper.getLocalInd(this->globalKeyEndA[i]);
            ind_t pIndB = pMapper.getLocalInd(this->globalKeyEndB[i]);
            if (pIndA == -1 || pIndB == -1)
                toBeDropped.push_back(i);
        }
        dropLinks(toBeDropped);
        xvtxRangePop();
    }

    void dropBrokenLinks() {
        xvtxRangePush("dropBrokenLinks links");
        /*
        ManVec<lInd_t> toBeDropped;
        toBeDropped.resize(this->numLocal);
        auto ptr = toBeDropped.data();
        Iterator::iterate(OffloadExec(),[=]DEVCALLABLE(int ind) {
            ptr[ind] = ind;
        }, this->numLocal);
        Iterator::syncAll();

        auto statePtr = this->state.data();
        auto point = thrust::stable_partition(toBeDropped.begin(), toBeDropped.end(), [=](int ind) {
            return statePtr[ind] == 0;
        });
        toBeDropped.resize(std::distance(toBeDropped.begin(), point));
*/
        auto statePtr = this->state.data();
        auto toBeDropped = listGenPredicate<lInd_t>(this->numLocal, [=] DEVCALLABLE (int ind) {
            return statePtr[ind] == 0;
        });
        dropLinks(toBeDropped);
        xvtxRangePop();
    }

    void dropBrokenNonOwnedLinks() {
        xvtxRangePush("drop non owned and broken");
        // port this to the rest of the drop flagged links

        int self = getLocalRank();
        auto statePtr = this->state.data();
        auto ownerPtrs = this->owners.data();

        ManVec<lInd_t> newLinkOrder = listGenSeq<lInd_t>(this->numLocal);

        auto point = stx::stable_partition(EXECPOLICY newLinkOrder.begin(), newLinkOrder.end(), [=] DEVCALLABLE (int ind) {
            return (statePtr[ind] != 0 && ownerPtrs[ind] == self);
        });
        int toKeep = std::distance(newLinkOrder.begin(), point);

        ManVec<lInd_t> newSideOrder;
        newSideOrder.resize(this->numLocal * 2);

        auto newSideOrderPtr = newSideOrder.data();
        auto newLinkOrderPtr = newLinkOrder.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (ind_t i, LinkDataHandler<UserData> &lData) {
            auto lInd = newLinkOrderPtr[i];
            newSideOrderPtr[(i * 2) + 0] = lData.sideIndEndA[lInd];
            newSideOrderPtr[(i * 2) + 1] = lData.sideIndEndB[lInd];
        }, this->numLocal, *this);
        Iterator::syncAll();

        reorder(newLinkOrder);

        reorderSideData(newSideOrder);

        resizeSideData(toKeep*2);
        resize(toKeep);
        this->generateOwnedLinksList();

        xvtxRangePop();
    }

    void resize(size_t newSize) {
        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr) {
            vecPTr->resize(newSize);
        });
        callForTupleElem(this->getFieldTupleGenerated(), [&](auto vecPTr) {
            vecPTr->resize(newSize);
        });

        this->numLocal = newSize;
    }

    void clear() {
        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr) {
            vecPTr->clear();
        });
        callForTupleElem(this->getFieldTupleGenerated(), [&](auto vecPTr) {
            vecPTr->clear();
        });
        callForTupleElem(this->getFieldTupleSideInput(), [&](auto vecPTr) {
            vecPTr->clear();
        });

        this->numLocal = 0;

        this->ownedIndexes.clear();
        this->nonOwnedIndexes.clear();
    }


    void reorder(ManVec<lInd_t> &newOrder) {
        // odd types
        xvtxRangePush("reorder links");

        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPtr) {
            reorderDevVecScratch(*vecPtr, newOrder, scratchSpace);
        });
        // particleLinkIds,
        callForTupleElem(this->getFieldTupleGenerated(), [&](auto vecPtr) {
            reorderDevVecScratch(*vecPtr, newOrder, scratchSpace);
        });

        this->generateOwnedLinksList();
        xvtxRangePop();
    }


    void dropNonOwnedLinks() {
        dropLinks(this->nonOwnedIndexes);
    }


    void reorderSideData(ManVec<lInd_t> &newOrder) {
        //
        xvtxRangePush("reorderCT link");

        callForTupleElem(this->getFieldTupleSideInput(), [&](auto vecPtr) {
            reorderDevVecScratch(*vecPtr, newOrder, scratchSpace);
        });

        this->reindexSideData(newOrder);
        xvtxRangePop();
    }

    void resizeSideData(size_t newSize) {
        callForTupleElem(this->getFieldTupleSideInput(), [&](auto vecPTr) {
            vecPTr->resize(newSize);
        });
    }

    void dropSideData(ManVec<lInd_t> &droppedSideInd) {
        xvtxRangePush("dropCT link");
        // dropped reorder needs to replace elements in the back of it to work, not just leave to old ones there
        // the reversed lookup for the new order/indexing does not work if its not
        ManVec<lInd_t> newOrder;
        getDropReorder(droppedSideInd, newOrder, (this->numLocal * 2));
        size_t newSize = (this->numLocal * 2) - droppedSideInd.size();
        reorderSideData(newOrder);
        resizeSideData(newSize);
        xvtxRangePop();
    }




    void appendInitLinks(ManVec<ElementaryLink> &links) {
        size_t oldCount = this->numLocal;
        size_t newCount = this->numLocal + links.size();
        size_t numAdded = links.size();

        if (newCount >= std::numeric_limits<int>::max()) {
            std::cout << "More than int_max links on a single rank, recompile with 64 bit indexes and rerun" << std::endl;
            exit(1);
        }

        this->numLocal = newCount;

        resize(newCount);
        resizeSideData(newCount * 2);

        auto linksPtr = links.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(ind_t i, UserData &lData) {

            lInd_t  outInd = i + oldCount;
            auto &l = linksPtr[i];

            lData.processInitLink(l, lData, outInd, outInd*2, outInd*2 + 1);

        }, numAdded, *this);
        Iterator::syncAll();

        generateOwnedNonOwned(this->owners, this->ownedIndexes, this->nonOwnedIndexes, this->numLocal);
    }

    // unsure of where to actually put this
    template<typename StateT>
    void updateStateInd(ManVec<int> &indexes, StateT &newStates) {

            auto *bufferPtr = newStates.data();
            auto *indPtr = indexes.data();
            auto *sideIndEndAPtr = this->sideIndEndA.data();
            auto *sideIndEndBPtr = this->sideIndEndB.data();

            auto *statePtr = this->state.data();
            auto *CT0_flat = this->CT0_flat.data();
            auto *CT1_flat = this->CT1_flat.data();
            auto *CT2_flat = this->CT2_flat.data();
            auto *CT3_flat = this->CT3_flat.data();
            auto *CT4_flat = this->CT4_flat.data();
            auto *CT5_flat = this->CT5_flat.data();

            Iterator::iterate(OffloadExec(), [=]
                DEVCALLABLE (int i) {
                int lInd = indPtr[i];

                bool origState = statePtr[lInd];
                bool newState = bufferPtr[i];

                // pDataPtr->flag[lData.localPartIndB[lInd]] = -1;
                if (origState != newState) {
                    statePtr[lInd] = newState;

                    if (newState == 1) {

                        pInd_t pIndA = sideIndEndAPtr[lInd];
                        pInd_t pIndB = sideIndEndBPtr[lInd];

                        CT0_flat[pIndB] = 0;
                        CT1_flat[pIndB] = 0;
                        CT2_flat[pIndB] = 0;
                        CT3_flat[pIndB] = 0;
                        CT4_flat[pIndB] = 0;
                        CT5_flat[pIndB] = 0;

                        CT0_flat[pIndA] = 0;
                        CT1_flat[pIndA] = 0;
                        CT2_flat[pIndA] = 0;
                        CT3_flat[pIndA] = 0;
                        CT4_flat[pIndA] = 0;
                        CT5_flat[pIndA] = 0;
                    }
                }
            }, indexes.size());
    }

    int getCurrentAllocated() {
        uint64_t sum = 0;

        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr) {
            sum+= vecPTr->cap;
        });
        callForTupleElem(this->getFieldTupleGenerated(), [&](auto vecPTr) {
            sum+= vecPTr->cap;
        });
        callForTupleElem(this->getFieldTupleSideInput(), [&](auto vecPTr) {
            sum+= vecPTr->cap;
        });
        return sum;
    }

    int getCurrentSize() {
        uint64_t sum = 0;

        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr) {
            sum+= vecPTr->size();
        });
        callForTupleElem(this->getFieldTupleGenerated(), [&](auto vecPTr) {
            sum+= vecPTr->size();
        });
        callForTupleElem(this->getFieldTupleSideInput(), [&](auto vecPTr) {
            sum+= vecPTr->size();
        });
        return sum;
    }
};


#endif //LINKDATAHANDLER_H
