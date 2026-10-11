//
// Created by Fredrik Robertsen on 7/7/24.
//

#ifndef PARTICLEDATAHANDLER_H
#define PARTICLEDATAHANDLER_H

#include "Util/DropReorder.h"
#include "Generators/ElementaryParticle.h"

// any autocomplete software will have a freakout about this file,
// just trust that the template it inherits from provides the missing variables

template<class UserData>
class ParticleDataHandler : public UserData {
public:
    ParticleDataHandler() {
    }
    void dropParticles(ManVec<pInd_t> &droppedInd) {
        ManVec<pInd_t> newOrder;
        getDropReorder(droppedInd, newOrder, this->numLocal);
        reorder(newOrder);
        resize(this->numLocal - droppedInd.size());
        this->generateOnwedList();
    }

    void reorder(ManVec<pInd_t> &newOrder) {
        //nvtxRangePush("reorder particle");
        // regenerate owned particles, assuming owner is 0,

        ManVec<pInd_t> orderInverted;
        orderInverted.resize(newOrder.size());

        pInd_t *newOrderPtr = newOrder.data();
        pInd_t *orderInvertedPtr = orderInverted.data();
        Iterator::iterate(OffloadExec(), [=] DEVLAMBDA (pInd_t i) {
            orderInvertedPtr[newOrderPtr[i]] = i;
        }, newOrder.size());

        Iterator::syncAll();

        Iterator::iterate(OffloadExec(), [=] DEVLAMBDA (pInd_t i, ParticleDataHandler<UserData> &pData) {
            pData.ownedIndexes[i] = orderInvertedPtr[pData.ownedIndexes[i]];
        }, this->ownedIndexes.size(), *this);

        Iterator::iterate(OffloadExec(), [=] DEVLAMBDA (pInd_t i, ParticleDataHandler<UserData> &pData) {
            pData.nonOwnedIndexes[i] = orderInvertedPtr[pData.nonOwnedIndexes[i]];
        }, this->nonOwnedIndexes.size(), *this);

        Iterator::syncAll();

        ParallelSort sorter;

        sorter.sortDirect(this->ownedIndexes);
        sorter.sortDirect(this->nonOwnedIndexes);
        //std::sort(ownedIndexes.begin(), ownedIndexes.end());
        //std::sort(nonOwnedIndexes.begin(), nonOwnedIndexes.end());
        Iterator::syncAll();

        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr) {
            reorderDevVec(*vecPTr, newOrder);
        });
    }

    void resize(size_t newSize) {
        this->numLocal = newSize;

        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr) {
            vecPTr->resize(newSize);
        });
        callForTupleElem(this->getFieldTupleOutput(), [&](auto vecPTr) {
            vecPTr->resize(newSize);
        });
    }

    void dropNonOwned() {
        //nvtxRangePush("dropNonOwned particle");
        // this is just a memcpy
        ManVec<pInd_t> reorderOrder;

        reorderOrder.resize(this->numLocal);

        Iterator::copy(reorderOrder.data(), this->ownedIndexes.data(),
                       this->ownedIndexes.size() * sizeof(pInd_t));
        Iterator::copy(reorderOrder.data() + this->ownedIndexes.size(),
                       this->nonOwnedIndexes.data(), this->nonOwnedIndexes.size() * sizeof(pInd_t));
        Iterator::syncAll();

        size_t newSize = this->ownedIndexes.size();

        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr) {
            reorderDevVec(*vecPTr, reorderOrder);
        });

        resize(newSize);

        // no non owned indexes left
        this->nonOwnedIndexes.clear();
        // owned indexes are now 0-newSize
        Iterator::iterate(OffloadExec(), [] DEVCALLABLE (int ind, ParticleDataHandler<UserData> &pData) {
            pData.ownedIndexes[ind] = ind;
        }, newSize, *this);
        Iterator::syncAll();
        //nvtxRangePop();
    }


    void clear() {
        this->numLocal = 0;

        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr) {
            vecPTr->clear();
        });
        callForTupleElem(this->getFieldTupleOutput(), [&](auto vecPTr) {
            vecPTr->clear();
        });

        this->ownedIndexes.clear();
        this->nonOwnedIndexes.clear();
    }

    void appendInitParticles(ManVec<ElementaryParticle> &particles) {

        resize(particles.size());

        auto particlesPtr = particles.data();
        Iterator::syncAll();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(ind_t i,  UserData &pData) {
            auto &p = particlesPtr[i];
            int outInd = i;
            // does not work anyway, this pointer now needs to be a member function
            pData.processInitParticle(p, pData, outInd);
        }, particles.size(), *this);

        Iterator::syncAll();

        generateOwnedNonOwned(this->owners, this->ownedIndexes,
                              this->nonOwnedIndexes, this->numLocal);
    }
    int getCurrentAllocated() {
        uint64_t sum = 0;

        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr) {
            sum+= vecPTr->cap;
        });
        callForTupleElem(this->getFieldTupleOutput(), [&](auto vecPTr) {
            sum+= vecPTr->cap;
        });
        return sum;
    }

    int getCurrentSize() {
        uint64_t sum = 0;

        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr) {
            sum+= vecPTr->size();
        });
        callForTupleElem(this->getFieldTupleOutput(), [&](auto vecPTr) {
            sum+= vecPTr->size();
        });
        return sum;
    }
};


#endif //PARTICLEDATAHANDLER_H
