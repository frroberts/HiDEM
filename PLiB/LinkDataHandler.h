//
// Created by Fredrik Robertsen on 7/7/24.
//

#ifndef LINKDATAHANDLER_H
#define LINKDATAHANDLER_H

#include <Util/DropReorder.h>

template<class UserData>
class LinkDataHandler : public UserData {
public:
    void dropLinks(ManVec<lInd_t> &droppedLocalIndexes) {
        //nvtxRangePush("dropLinks links");

        ManVec<lInd_t> newOrder;
        getDropReorder(droppedLocalIndexes, newOrder, this->numLocal);

        size_t newSize = this->numLocal - droppedLocalIndexes.size();

        ManVec<lInd_t> droppedSideInd(droppedLocalIndexes.size() * 2);
        auto droppedSideIndPtr = droppedSideInd.data();
        auto droppedLocalIndexesPtr = droppedLocalIndexes.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (ind_t i, HiDEMLinkData &lData) {
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
        //nvtxRangePush("dropUnusedLinks links");
        ManVec<lInd_t> toBeDropped;
        for (size_t i = 0; i < this->numLocal; ++i) {
            ind_t pIndA = pMapper.getLocalInd(this->globalKeyEndA[i]);
            ind_t pIndB = pMapper.getLocalInd(this->globalKeyEndB[i]);
            if (pIndA == -1 || pIndB == -1)
                toBeDropped.push_back(i);
        }
        dropLinks(toBeDropped);
        //nvtxRangePop();
    }

    void dropBrokenLinks() {
        //nvtxRangePush("dropBrokenLinks links");
        ManVec<lInd_t> toBeDropped;
        for (size_t i = 0; i < this->numLocal; ++i) {
            if (this->state[i] == 0)
                toBeDropped.push_back(i);
        }
        dropLinks(toBeDropped);
        //nvtxRangePop();
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
        this->numPraticlesAllocated = 0;
    }


    void reorder(ManVec<lInd_t> &newOrder) {
        // odd types
        //nvtxRangePush("reorder links");

        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr) {
            reorderDevVec(*vecPTr, newOrder);
        });
        // particleLinkIds,
        callForTupleElem(this->getFieldTupleGenerated(), [&](auto vecPTr) {
            reorderDevVec(*vecPTr, newOrder);
        });

        this->generateOwnedLinksList();
        //nvtxRangePop();
    }


    void dropNonOwnedLinks() {
        dropLinks(this->nonOwnedIndexes);
    }


    void reorderSideData(ManVec<lInd_t> &newOrder) {
        //
        //nvtxRangePush("reorderCT link");

        if (newOrder.size() != this->CT0_flat.size()) {
            std::cout << "likely invalid CT reorder" << std::endl;
        }

        callForTupleElem(this->getFieldTupleSideInput(), [&](auto vecPTr) {
            reorderDevVec(*vecPTr, newOrder);
        });

        this->reindexSideData(newOrder);
        //nvtxRangePop();
    }

    void resizeSideData(size_t newSize) {
        callForTupleElem(this->getFieldTupleSideInput(), [&](auto vecPTr) {
            vecPTr->resize(newSize);
        });
    }

    void dropSideData(ManVec<lInd_t> &droppedSideInd) {
        //nvtxRangePush("dropCT link");
        // dropped reorder needs to replace elements in the back of it to work, not just leave to old ones there
        // the reversed lookup for the new order/indexing does not work if its not
        ManVec<lInd_t> newOrder;
        getDropReorder(droppedSideInd, newOrder, (this->numLocal * 2));
        size_t newSize = (this->numLocal * 2) - droppedSideInd.size();
        reorderSideData(newOrder);
        resizeSideData(newSize);
        //nvtxRangePop();
    }


    // move to Accel ?
    void sortSideData(ParticleData &pData) {

        RangeV2 r5("sort link side data", 3);
        ind_t numParticles = pData.numLocal;

        ManVec<lInd_t> newOrder(this->numLocal * 2);

#if defined(SOA_CT)

    #warning "compiling SOA_CT"
        ind_t orderInd = 0;

        for (int i = 0; i < this->allocatedMaxLinks; ++i) {
            for (int pInd = 0; pInd < numParticles; ++pInd) {
                int numLinks = this->linksPerParticle[pInd];
                if (i >= numLinks)
                    continue;

                lInd_t lInd = this->getPartLinkId(i, pInd);
                lInd_t ctInd;
                if(this->localPartIndA[lInd] == pInd)
                    ctInd = this->sideIndEndA[lInd];
                else {
                    ctInd = this->sideIndEndB[lInd];
                }
                newOrder[orderInd] = ctInd;
                orderInd++;
            }
        }

#elif defined(AOSOA_CT)
#warning "compiling AOSOA_CT"

        int numInBlock = 64;
        int numBlocks = 1 + (numParticles / numInBlock);


        ManVec<lInd_t> blockSizes;
        ManVec<lInd_t> blockStarts;
        blockSizes.resize(numBlocks, false);
        blockStarts.resize(numBlocks, false);
        auto ptr = blockSizes.ptr;
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i) {
            ptr[i] = 0;
        }, numBlocks);

        // get active in block
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int b, HiDEMLinkData &lData) {
            int c = 0;
            for (int l = 0; l < lData.allocatedMaxLinks; l++) {
                for (size_t i = 0; i < numInBlock; i++) {
                    int blockStart = (b * numInBlock);
                    if (blockStart + i >= numParticles)
                        continue;
                    pInd_t pInd = blockStart + i;
                    if (l >= lData.linksPerParticle[pInd])
                        continue;
                    c++;
                }
            }
            ptr[b] = c;
        }, numBlocks, *this);

        // running sum of sizes to get block starts
        ParallelScan scanner;

        scanner.exclusive(blockSizes, blockStarts);

        // for each block run all the links, one thread per block, its not the best but better than serial?
        auto newOrderPtr = newOrder.ptr;
        auto blockStartPtr = blockStarts.ptr;
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int b, HiDEMLinkData &lData) {
            int c = blockStartPtr[b]; // get block start
            for (int l = 0; l < lData.allocatedMaxLinks; l++) {
                for (size_t i = 0; i < numInBlock; i++) {
                    int blockStart = (b * numInBlock);
                    if (blockStart + i >= numParticles)
                        continue;

                    pInd_t pInd = blockStart + i;
                    if (l >= lData.linksPerParticle[pInd])
                        continue;

                    lInd_t lInd = lData.getPartLinkId(l, pInd);
                    lInd_t ctInd;
                    if (lData.localPartIndA[lInd] == pInd)
                        ctInd = lData.sideIndEndA[lInd];
                    else {
                        ctInd = lData.sideIndEndB[lInd];
                    }
                    newOrderPtr[c] = ctInd;
                    c++;
                }
            }
        }, numBlocks, *this);
        Iterator::syncAll();
#else

    #warning "compiling AOS_CT"

    // can be done with a scan to get the individual particle start ?
    // and then the particles can be done in parallel at least ?

    ManVec<lInd_t> particleSarts;
    particleSarts.resize(numParticles + 1, false);
    particleSarts[0] = 0;

    std::partial_sum(this->linksPerParticle.begin(), this->linksPerParticle.end(), particleSarts.begin() + 1);

    auto pStartPtr = particleSarts.ptr;
    auto newOrderPtr = newOrder.ptr;

    Iterator::iterate(ParallelExec(), [=]DEVCALLABLE(ind_t pInd, HiDEMLinkData &lData){
        //
        lInd_t orderInd = pStartPtr[pInd];
        for (int i = 0; i < lData.allocatedMaxLinks; ++i) {
            int numLinks = lData.linksPerParticle[pInd];
            if (i >= numLinks)
                continue;
            lInd_t lInd = lData.getPartLinkId(i,pInd);
            lInd_t ctInd;
            if (lData.localPartIndA[lInd] == pInd)
                ctInd = lData.sideIndEndA[lInd];
            else {
                ctInd = lData.sideIndEndB[lInd];
            }
            newOrderPtr[orderInd] = ctInd;
            orderInd++;
        }
    }, numParticles, *this);
    Iterator::syncAll();
    /*
#pragma omp parallel for
    for (int pInd = 0; pInd < numParticles; ++pInd) {
        lInd_t orderInd = particleSarts[pInd];
        for (int i = 0; i < maxLinks; ++i) {
            int numLinks = linksPerParticle[pInd];
            if (i >= numLinks)
                continue;
            lInd_t lInd = getPartLinkId(i,pInd);
            lInd_t ctInd;
            if (localPartIndA[lInd] == pInd)
                ctInd = sideIndEndA[lInd];
            else {
                ctInd = sideIndEndB[lInd];
            }
            newOrder[orderInd] = ctInd;
            orderInd++;
        }
    }
        */

#endif
        Iterator::syncAll();

        reorderSideData(newOrder);

        //nvtxRangePop();
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

        auto linksPtr = links.ptr;
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(ind_t i, UserData &lData) {

            lInd_t  outInd = i + oldCount;
            auto &l = linksPtr[i];

            this->processInitLink(l, lData, outInd);

        }, numAdded, *this);
        Iterator::syncAll();

        generateOwnedNonOwned(this->owners, this->ownedIndexes, this->nonOwnedIndexes, this->numLocal);
    }
};


#endif //LINKDATAHANDLER_H
