//
// Created by Fredrik Robertse on 6/2/24.
//

#ifndef PHELL_VTKHDF_H
#define PHELL_VTKHDF_H

#include <string>
#include <ManVec.h>


class VtkHdfWriter {
    HDF5IO io;

    template<typename ...Names, typename ...Fields>
    void unpackValsIndexed(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, ManVec<pInd_t> &indexes,
                    size_t fullSize) {
        unpackValsIndexedImpl(fieldNames, outFields, indexes, fullSize, std::make_index_sequence<sizeof...(Fields)>());
    }

    template<std::size_t... ints, typename ...Names, typename ...Fields>
    void unpackValsIndexedImpl(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, ManVec<pInd_t> &indexes,
                        size_t fullSize,
                        std::integer_sequence<std::size_t, ints...>) {
        ((io.writeDataIndexed(std::get<ints>(outFields), fullSize, indexes.data(), indexes.size(),
                            std::get<ints>(fieldNames))), ...);
    }

    template<typename ...Names, typename ...Fields>
    void unpackVals(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, size_t fullSize) {
        unpackValsImpl(fieldNames, outFields, fullSize, std::make_index_sequence<sizeof...(Fields)>());
    }

    template<std::size_t... ints, typename ...Names, typename ...Fields>
    void unpackValsImpl(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, size_t fullSize,
                        std::integer_sequence<std::size_t, ints...>) {
        (io.writeDataSpan(std::get<ints>(outFields),
                            std::get<ints>(fieldNames), io.generateSpan(fullSize)), ...);
    }

public:
    // todo change write to stage
    // add write call
    // the idea being you can call stage and then the io function will make local copies to write out later
    void createFile(std::string path, MPI_Comm comm = MPI_COMM_WORLD) {
        io.createFile(path, comm);
    }

    void closeFile() {
        io.closeFile();
    }


    template<class CordT, typename ...Names, typename ...Fields>
    void writerParticlesIndexed(ManVec<CordT> &xCords, ManVec<CordT> &yCords, ManVec<CordT> &zCords, ManVec<pInd_t> &indexes,
                           std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields) {

        pInd_t numParticlesLocal = indexes.size();
        pInd_t numParticlesGlobal;
        MPI_Allreduce(&numParticlesLocal, &numParticlesGlobal, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

        io.openGroup("VTKHDF");
        io.writeAttributeStr("Type", "UnstructuredGrid");
        std::vector<long long int> version = {2, 2};
        io.writeAttributeArr("Version", version);

        ManVec<long long int> NumberOfPoints;
        NumberOfPoints.push_back(numParticlesGlobal);
        io.writeVectorFirst(NumberOfPoints, "NumberOfPoints");

        ManVec<int> NumberOfConnectivityIds;
        NumberOfConnectivityIds.push_back(0);
        io.writeVectorFirst(NumberOfConnectivityIds, "NumberOfConnectivityIds");

        ManVec<int> NumberOfCells;
        NumberOfCells.push_back(0);
        io.writeVectorFirst(NumberOfCells, "NumberOfCells");

        ManVec<int> Offsets;
        Offsets.push_back(0);
        io.writeVectorFirst(Offsets, "Offsets");

        ManVec<int> Connectivity;
        io.writeVectorFirst(Connectivity, "Connectivity");

        MPI_Barrier(MPI_COMM_WORLD);
        ManVec<unsigned char> Types;
        io.writeVectorFirst(Types, "Types");
        ManVec<CordT> points;
        packPoints(xCords, yCords, zCords, indexes, points);

        io.writeVectorAll2D(points, "Points", 3);

        io.openGroup("PointData");
        unpackValsIndexed(fieldNames, outFields, indexes, xCords.size());
        io.closeGroup();

        io.closeGroup();
    }

    template<class CordT>
    void packPoints(ManVec<CordT> &xCords, ManVec<CordT> &yCords, ManVec<CordT> &zCords, ManVec<pInd_t> &indexes, ManVec<CordT> &points) const {
        points.resize(indexes.size() * 3);
        CordT *pointPtr = points.data();
        CordT *xCordsPtr = xCords.data();
        CordT *yCordsPtr = yCords.data();
        CordT *zCordsPtr = zCords.data();
        pInd_t *indexesPtr = indexes.data();

        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i) {
            int ind = indexesPtr[i];
            pointPtr[(i * 3) + 0] = (xCordsPtr[ind]);
            pointPtr[(i * 3) + 1] = (yCordsPtr[ind]);
            pointPtr[(i * 3) + 2] = (zCordsPtr[ind]);
        }, indexes.size());
        Iterator::syncAll();
    }

    // takes pointers and not manVec references because of how mesh data works ?
    template<class CordT, typename ...Names, typename ...Fields>
    void writeMeshFullSingle(CordT* xCords, CordT* yCords, CordT* zCords, int numTris,
                             std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields) {
        if (getLocalRank() != 0)
            return;

        io.openGroup("VTKHDF");
        io.writeAttributeStr("Type", "UnstructuredGrid");
        std::vector<long long int> version = {2, 2};
        io.writeAttributeArr("Version", version);

        ManVec<long long int> NumberOfPoints;
        NumberOfPoints.push_back(numTris*3);
        io.writeVectorFirst(NumberOfPoints, "NumberOfPoints");

        ManVec<long long int> NumberOfConnectivityIds;
        NumberOfConnectivityIds.push_back(numTris*3);
        io.writeVectorFirst(NumberOfConnectivityIds, "NumberOfConnectivityIds");

        ManVec<long long int> NumberOfCells;
        NumberOfCells.push_back(numTris);
        io.writeVectorFirst(NumberOfCells, "NumberOfCells");

        ManVec<long long int> Offsets;
        for(int i = 0; i < numTris+1; i++) {
            Offsets.push_back((i)*3);
        }
        io.writeVectorFirst(Offsets, "Offsets");

        ManVec<long long int> Connectivity;
        for (int i = 0; i < numTris * 3; i++) {
            Connectivity.push_back(i);
        }
        io.writeVectorFirst(Connectivity, "Connectivity");

        ManVec<unsigned char> Types;
        for (int i = 0; i < numTris; i++) {
            Types.push_back(5);
        }
        io.writeVectorFirst(Types, "Types");


        ManVec<double> points;
        for (int i = 0; i < numTris*3; i++){
            points.push_back(xCords[i]);
            points.push_back(yCords[i]);
            points.push_back(zCords[i]);
        }
        io.writeVectorAll2D(points, "Points", 3);

        io.openGroup("CellData");
        unpackVals(fieldNames, outFields, numTris);
        io.closeGroup();

        io.closeGroup();
    }
};


#endif //PHELL_VTKHDF_H
