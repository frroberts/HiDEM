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
#include "H5IO/HDF5IO.h"
#include "OutputWriters/CsvWriter.h"
#include "OutputWriters/Hdf5Writer.h"

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

    void dropUnusedLinks(OffloadHashMap<gKey_t, pInd_t> &pMapper) {
        xvtxRangePush("dropUnusedLinks links");
        ManVec<lInd_t> toBeDropped;
        for (size_t i = 0; i < this->numLocal; ++i) {
            ind_t pIndA = pMapper.getValue(this->globalKeyEndA[i]);
            ind_t pIndB = pMapper.getValue(this->globalKeyEndB[i]);
            if (pIndA == pMapper.notFound() || pIndB == pMapper.notFound())
                toBeDropped.push_back(i);
        }
        dropLinks(toBeDropped);
        xvtxRangePop();
    }

    void dropBrokenLinks() {
        xvtxRangePush("dropBrokenLinks links");

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
        newSideOrder.resize(this->numSideData);

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

    void reorderSideDataPadded(ManVec<lInd_t> &newOrder, size_t paddedSize) {
        //
        this->numSideData = paddedSize;
        xvtxRangePush("reorderCT link");

        callForTupleElem(this->getFieldTupleSideInput(), [&](auto vecPTr) {
                    vecPTr->resize(paddedSize);
                });

        callForTupleElem(this->getFieldTupleSideInput(), [&](auto vecPtr) {
            reorderDevVecScratch(*vecPtr, newOrder, scratchSpace);
        });

        this->reindexSideData(newOrder);
        xvtxRangePop();
    }

    void resizeSideData(size_t newSize) {
        this->numSideData = newSize;
        callForTupleElem(this->getFieldTupleSideInput(), [&](auto vecPTr) {
            vecPTr->resize(newSize);
        });
    }

    void dropSideData(ManVec<lInd_t> &droppedSideInd) {
        xvtxRangePush("dropCT link");
        // dropped reorder needs to replace elements in the back of it to work, not just leave to old ones there
        // the reversed lookup for the new order/indexing does not work if its not
        ManVec<lInd_t> newOrder;
        getDropReorder(droppedSideInd, newOrder, (this->numSideData));
        size_t newSize = (this->numSideData) - droppedSideInd.size();
        reorderSideData(newOrder);
        resizeSideData(newSize);
        xvtxRangePop();
    }

    void appendInitLinks(ManVec<ElementaryLink> &links) {
        size_t oldCount = this->numLocal;
        size_t oldSideCount = this->numSideData;
        size_t newCount = this->numLocal + links.size();
        size_t numAdded = links.size();

        if (newCount >= std::numeric_limits<int>::max()) {
            std::cout << "More than int_max links on a single rank, recompile with 64 bit indexes and rerun" << std::endl;
            exit(1);
        }

        this->numLocal = newCount;

        resize(newCount);
        resizeSideData(this->numSideData + (numAdded*2));

        auto linksPtr = links.data();
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(ind_t i, UserData &lData) {

            lInd_t  outInd = i + oldCount;
            lInd_t  outSideInd = i*2 + oldSideCount;
            auto &l = linksPtr[i];

            lData.processInitLink(l, lData, outInd, outSideInd, outSideInd + 1);

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


    template<class... Names, class... Fields>
    void snapshotLinksIndex(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields,
                                ManVec<pInd_t> &indexes, std::optional<std::string> &outLinkType,
                                std::optional<std::string> &outLinkFile, std::optional<std::string> &outWorkingPath,
                                int currentTimestep,
                                std::string prefix = "") {
        RangeV2 r("write links", 2);
        static_assert(sizeof...(Fields) == sizeof...(Names));
        if (outLinkType && outLinkFile && !outLinkFile.value().empty()) {
            // no these cant do inheritance they rely on templates
            if (outLinkType.value() == "CSV") {
                std::string outputStr = outWorkingPath.value_or(".") + "/" + std::to_string(getLocalRank()) + "_" +
                                        prefix + outLinkFile.value() + std::to_string(currentTimestep);
                CsvWriter linkIo;
                linkIo.createFile(outputStr + ".csv");
                linkIo.writeIndexed(indexes, fieldNames, vecPtrToDataPtr(outFields));
                linkIo.closeFile();
            } else if (outLinkType.value() == "HDF5") {
                std::string outputStr =
                        outWorkingPath.value_or(".") + "/" + prefix + outLinkFile.value() + std::to_string(currentTimestep);
                Hdf5Writer linkIo;
                linkIo.createFile(outputStr + ".h5");
                linkIo.writeIndexed(indexes, this->numLocal, fieldNames, vecPtrToDataPtr(outFields));
                linkIo.closeFile();
            } else {
                Logger::writeAtFirst("Unsupported link output type exiting ...");
                exit(1);
            }
        }
    }

    void checkpointData(HDF5IO &h5) {
        h5.openGroup("Links");
        HDF5Span span = h5.generateSpan(this->ownedIndexes.size());

        auto fields = this->getFieldTupleInput();

        int fieldInd = 0;
        callForTupleElem(fields, [&](auto vec) {
            h5.writeDataIndexedSpan(vec->data(), this->numLocal, this->ownedIndexes.data(),
                                    "field_" + std::to_string(fieldInd), span);
            fieldInd++;
        });
        h5.writeSpan(span);
        h5.closeGroup();

        h5.openGroup("LinkSideData");

        ManVec<lInd_t> sideIndexes;
        for (size_t i = 0; i < this->ownedIndexes.size(); ++i) {
            lInd_t lInd = this->ownedIndexes[i];
            lInd_t sideIndA = this->sideIndEndA[lInd];
            lInd_t sideIndB = this->sideIndEndB[lInd];
            sideIndexes.push_back(sideIndA);
            sideIndexes.push_back(sideIndB);
        }
        HDF5Span spanSide = h5.generateSpan(sideIndexes.size());

        auto sideFields = this->getFieldTupleSideInput();
        callForTupleElem(sideFields, [&]<class VecType>(VecType *vec) {
            VecType temp;
            temp.resize(sideIndexes.size());
            for (size_t i = 0; i < sideIndexes.size(); ++i) {
                temp[i] = (*vec)[sideIndexes[i]];
            }
            h5.writeDataSpan(temp.data(), "side_field_" + std::to_string(fieldInd), spanSide);
            fieldInd++;
        });
        h5.writeSpan(spanSide);
        h5.closeGroup();
    }

    void fromCheckpoint(HDF5IO &h5) {
        //

        h5.openGroup("Links");

        std::vector<HDF5Span> spans = h5.readSpan();
        int self = getLocalRank();

        HDF5Span ownSpan;

        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        if (spans.size() == worldSize) {
            // normal read
            ownSpan = spans[self];
        } else {
            std::cout << "restarting with different numer opf ranks is not supproted" << std::endl;
            std::cout << "restart file has " << spans.size() << " ranks, current run uses " << worldSize << " ranks" <<
                    std::endl;
            exit(1);
        }

        int oldCount = this->numLocal;
        int oldSideCount = this->numLocal*2;

        this->resize(oldCount + ownSpan.localCount);

        int fieldInd = 0;
        callForTupleElem(this->getFieldTupleInput(), [&]<typename T>(ManVec<T> *vec) {
            h5.readPtrSpann("field_" + std::to_string(fieldInd), &(vec->data()[oldCount]), ownSpan);

            fieldInd++;
        });
        h5.closeGroup();

        h5.openGroup("LinkSideData");
        std::vector<HDF5Span> sideSpans = h5.readSpan();
        auto ownSideSpan = sideSpans[self];

        this->resizeSideData(oldSideCount + ownSideSpan.localCount);

        callForTupleElem(this->getFieldTupleSideInput(), [&]<typename T>(ManVec<T> *vec) {
            h5.readPtrSpann("side_field_" + std::to_string(fieldInd), &(vec->data()[oldSideCount]), ownSideSpan);
            fieldInd++;
        });

        for (int i = oldSideCount; i < oldSideCount + (ownSideSpan.localCount/2); i++) {
            this->sideIndEndA[i] = i * 2;
            this->sideIndEndB[i] = (i * 2) + 1;
        }

        h5.closeGroup();

        generateOwnedNonOwned(this->owners, this->ownedIndexes, this->nonOwnedIndexes, this->numLocal);
    }
};


#endif //LINKDATAHANDLER_H
