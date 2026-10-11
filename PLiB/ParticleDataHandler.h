//
// Created by Fredrik Robertsen on 7/7/24.
//

#ifndef PARTICLEDATAHANDLER_H
#define PARTICLEDATAHANDLER_H

#include "Util/DropReorder.h"
#include "Generators/ElementaryParticle.h"
#include "H5IO/HDF5IO.h"
#include "Logger/Logger.h"
#include "LoggerV2/RangeCollection.h"
#include "OutputWriters/CsvWriter.h"
#include "OutputWriters/Hdf5Writer.h"
#include "OutputWriters/VtkHdfWriter.h"
#include "Util/OwnedListGen.h"

// any autocomplete software will have a freakout about this file,
// just trust that the template it inherits from provides the missing variables

template<class UserData>
class ParticleDataHandler : public UserData {
    ManVec<uint8_t, ScratchAllocator<uint8_t>> scratchSpace;
    ManVec<pInd_t> reorderOrder;
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
        xvtxRangePush("reorder particle");
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

        stx::sort(EXECPOLICY this->ownedIndexes.begin(), this->ownedIndexes.end());
        stx::sort(EXECPOLICY this->nonOwnedIndexes.begin(), this->nonOwnedIndexes.end());
        Iterator::syncAll();

        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr) {
            reorderDevVecScratch(*vecPTr, newOrder, scratchSpace);
        });
        xvtxRangePop();
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
        xvtxRangePush("dropNonOwned particle");
        // this is just a memcpy

        ManVec<pInd_t> reorderOrder;

        reorderOrder.resize(this->numLocal, false);

        Iterator::copy(reorderOrder.data(), this->ownedIndexes.data(),
                       this->ownedIndexes.size() * sizeof(pInd_t));
        Iterator::copy(reorderOrder.data() + this->ownedIndexes.size(),
                       this->nonOwnedIndexes.data(), this->nonOwnedIndexes.size() * sizeof(pInd_t));
        Iterator::syncAll();

        size_t newSize = this->ownedIndexes.size();

        callForTupleElem(this->getFieldTupleInput(), [&](auto vecPTr)
        {
            reorderDevVecScratch(*vecPTr, reorderOrder, scratchSpace);
        });

        resize(newSize);

        // no non owned indexes left
        this->nonOwnedIndexes.clear();
        // owned indexes are now 0-newSize
        Iterator::iterate(OffloadExec(), [] DEVCALLABLE (int ind, ParticleDataHandler<UserData> &pData) {
            pData.ownedIndexes[ind] = ind;
        }, newSize, *this);
        Iterator::syncAll();
        xvtxRangePop();

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


    template<class... Names, class... Fields>
    void snapshotParticlesIndex(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields,
                                ManVec<pInd_t> &indexes, std::optional<std::string> &outParticleType,
                                std::optional<std::string> &outParticleFile, std::optional<std::string> &outWorkingPath,
                                int currentTimestep,
                                std::string prefix = "") {
        RangeV2 r("write particles", 2);
        static_assert(sizeof...(Fields) == sizeof...(Names));

        if (outParticleType && outParticleFile && !outParticleFile.value().empty()) {
            std::string outputStr = outWorkingPath.value_or(".") + "/" + prefix + outParticleFile.value() +
                                    std::to_string(currentTimestep);
            // no these cant do inheritance they rely on templates
            if (outParticleType.value() == "CSV") {
                outputStr = outWorkingPath.value_or(".") + "/" + std::to_string(getLocalRank()) + "_" +
                            prefix + outParticleFile.value() + std::to_string(currentTimestep);
                CsvWriter partIo;
                partIo.createFile(outputStr + ".csv");
                partIo.writerParticlesIndexed(this->xCord, this->yCord, this->zCord, indexes, fieldNames,
                                              vecPtrToDataPtr(outFields));
                partIo.closeFile();
            } else if (outParticleType.value() == "HDF5") {
                Hdf5Writer partIo;
                partIo.createFile(outputStr + ".h5");
                partIo.writerParticlesIndexed(this->xCord, this->yCord, this->zCord, indexes, fieldNames,
                                              vecPtrToDataPtr(outFields));
                partIo.closeFile();
            } else if (outParticleType.value() == "VTKHDF") {
                VtkHdfWriter partIo;
                partIo.createFile(outputStr + ".vtkhdf");
                partIo.writerParticlesIndexed(this->xCord, this->yCord, this->zCord, indexes, fieldNames,
                                              vecPtrToDataPtr(outFields));
                partIo.closeFile();
                //
            } else {
                Logger::writeAtFirst("Unsupported particle output type exiting ...");
                exit(1);
            }
        }
    }

    void checkpointData(HDF5IO &h5) {
        h5.openGroup("Particles");
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
    }

    void fromCheckpoint(HDF5IO &h5) {

        h5.openGroup("Particles");

        std::vector<HDF5Span> spans = h5.readSpan("");
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

        this->resize(oldCount + ownSpan.localCount);

        auto fields = this->getFieldTupleInput();
        int fieldInd = 0;
        callForTupleElem(fields, [&]<typename T>(ManVec<T> *vec) {
            h5.readPtrSpann("field_" + std::to_string(fieldInd), &(vec->data()[oldCount]), ownSpan);

            fieldInd++;
        });
        h5.closeGroup();

        generateOwnedNonOwned(this->owners, this->ownedIndexes, this->nonOwnedIndexes, this->numLocal);
    }
};


#endif //PARTICLEDATAHANDLER_H
