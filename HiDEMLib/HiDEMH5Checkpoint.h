//
// Created by Fredrik Robertsen on 03/04/2022.
//

#ifndef PHELL_HIDEMH5CHECKPOINT_H
#define PHELL_HIDEMH5CHECKPOINT_H


#include <Domain.h>
#include <H5IO/H5Checkpoint.h>
#include <Util/Util.h>

class HiDEMH5Checkpoint : public H5Checkpoint {
public:
    HiDEMH5Checkpoint() {
    }

    ~HiDEMH5Checkpoint() {
    }


    template <typename PDataT, typename LDataT>
    void writeCheckpoint(int timestep, Domain bounding, PDataT &pData,
                                            LDataT &lData,
                                            bool timestepInFile, std::string workingPathStr,
                                            std::string checkpointDirectory) {
        MPI_Barrier(MPI_COMM_WORLD);
        std::string suffixStr = "";
        if (timestepInFile) {
            suffixStr = "_" + std::to_string(timestep);
        }
        newCheckpooint(workingPathStr + "/" + checkpointDirectory + "/" + "checkpoint" + suffixStr + ".h5");
        writeTimestep(timestep);
        writeDomains(bounding);
        writeParticleDataTup(pData);
        writeLinkData(lData);

        h5.closeFile();
    }

    template <typename LDataT>
    void writeLinkData(LDataT &lData) {
        h5.openGroup("Links");
        HDF5Span span = h5.generateSpan(lData.ownedIndexes.size());

        auto fields = lData.getFieldTupleInput();

        int fieldInd = 0;
        callForTupleElem(fields, [&](auto vec) {
            h5.writeDataIndexedSpan(vec->data(), lData.numLocal, lData.ownedIndexes.data(),
                                    "field_" + std::to_string(fieldInd), span);
            fieldInd++;
        });
        h5.writeSpan(span);
        h5.closeGroup();

        h5.openGroup("LinkSideData");

        ManVec<lInd_t> sideIndexes;
        for (int i = 0; i < lData.ownedIndexes.size(); ++i) {
            lInd_t lInd = lData.ownedIndexes[i];
            lInd_t sideIndA = lData.sideIndEndA[lInd];
            lInd_t sideIndB = lData.sideIndEndB[lInd];
            sideIndexes.push_back(sideIndA);
            sideIndexes.push_back(sideIndB);
        }
        HDF5Span spanSide = h5.generateSpan(sideIndexes.size());

        auto sideFields = lData.getFieldTupleSideInput();
        callForTupleElem(sideFields, [&]<class VecType>(VecType *vec) {
            VecType temp;
            temp.resize(sideIndexes.size());
            for (int i = 0; i < sideIndexes.size(); ++i) {
                temp[i] = (*vec)[sideIndexes[i]];
            }
            h5.writeDataSpan(temp.data(), "side_field_" + std::to_string(fieldInd), spanSide);
            fieldInd++;
        });
        h5.writeSpan(spanSide);
        h5.closeGroup();
    }

    template <typename LDataT>
    void readLinkData(LDataT &lData) {
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

        int oldCount = lData.numLocal;
        int oldSideCount = lData.numLocal*2;

        lData.resize(oldCount + ownSpan.localCount);

        int fieldInd = 0;
        callForTupleElem(lData.getFieldTupleInput(), [&]<typename T>(ManVec<T> *vec) {
            h5.readPtrSpann("field_" + std::to_string(fieldInd), &(vec->data()[oldCount]), ownSpan);

            fieldInd++;
        });
        h5.closeGroup();

        h5.openGroup("LinkSideData");
        std::vector<HDF5Span> sideSpans = h5.readSpan();
        auto ownSideSpan = sideSpans[self];

        lData.resizeSideData(oldSideCount + ownSideSpan.localCount);

        callForTupleElem(lData.getFieldTupleSideInput(), [&]<typename T>(ManVec<T> *vec) {
            h5.readPtrSpann("side_field_" + std::to_string(fieldInd), &(vec->data()[oldSideCount]), ownSideSpan);
            fieldInd++;
        });

        for (int i = oldSideCount; i < oldSideCount + (ownSideSpan.localCount/2); i++) {
            lData.sideIndEndA[i] = i * 2;
            lData.sideIndEndB[i] = (i * 2) + 1;
        }

        h5.closeGroup();

        generateOwnedNonOwned(lData.owners, lData.ownedIndexes, lData.nonOwnedIndexes, lData.numLocal);
    }

    template <typename PDataT>
    void writeParticleDataTup(PDataT &pData) {
        h5.openGroup("Particles");
        HDF5Span span = h5.generateSpan(pData.ownedIndexes.size());

        auto fields = pData.getFieldTupleInput();

        int fieldInd = 0;
        callForTupleElem(fields, [&](auto vec) {
            h5.writeDataIndexedSpan(vec->data(), pData.numLocal, pData.ownedIndexes.data(),
                                    "field_" + std::to_string(fieldInd), span);
            fieldInd++;
        });

        h5.writeSpan(span);
        h5.closeGroup();
    }

    template <typename PDataT>
    void readParticleDataTup(PDataT &pData) {
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

        int oldCount = pData.numLocal;

        pData.resize(oldCount + ownSpan.localCount);

        auto fields = pData.getFieldTupleInput();
        int fieldInd = 0;
        callForTupleElem(fields, [&]<typename T>(ManVec<T> *vec) {
            h5.readPtrSpann("field_" + std::to_string(fieldInd), &(vec->data()[oldCount]), ownSpan);

            fieldInd++;
        });
        h5.closeGroup();

        generateOwnedNonOwned(pData.owners, pData.ownedIndexes, pData.nonOwnedIndexes, pData.numLocal);
    }
};


#endif //PHELL_HIDEMH5CHECKPOINT_H
