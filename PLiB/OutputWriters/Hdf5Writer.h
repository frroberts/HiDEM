//
// Created by Fredrik Robertsen on 6/10/24.
//

#ifndef PHELL_HDF5WRITER_H
#define PHELL_HDF5WRITER_H

#include <string>
#include <ManVec.h>

class Hdf5Writer {
    HDF5IO w;

    template<typename ...Names, typename ...Fields>
    void unpackVals(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, ManVec<pInd_t> &indexes,
                    size_t fullSize) {
        unpackValsImpl(fieldNames, outFields, indexes, fullSize, std::make_index_sequence<sizeof...(Fields)>());
    }

    template<std::size_t... ints, typename ...Names, typename ...Fields>
    void unpackValsImpl(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, ManVec<pInd_t> &indexes,
                        size_t fullSize,
                        std::integer_sequence<std::size_t, ints...>) {
        ((w.writeDataIndexed(std::get<ints>(outFields), fullSize, indexes.data(), indexes.size(),
                              std::get<ints>(fieldNames))), ...);
    }

public:
    Hdf5Writer() = default;

    void createFile(std::string path) {
        w.createFile(path);

    }

    template<class CordT, typename ...Names, typename ...Fields>
    void
    writerParticlesIndexed(ManVec<CordT> &xCords, ManVec<CordT> &yCords, ManVec<CordT> &zCords, ManVec<pInd_t> &indexes,
                      std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields) {

        w.writeDataIndexed(xCords.data(), xCords.size(), indexes.data(), indexes.size(), "xCord");
        w.writeDataIndexed(yCords.data(), yCords.size(), indexes.data(), indexes.size(), "yCord");
        w.writeDataIndexed(zCords.data(), zCords.size(), indexes.data(), indexes.size(), "zCord");

        unpackVals(fieldNames, outFields, indexes, xCords.size());
    }

    template<typename ...Names, typename ...Fields>
    void
    writeIndexed(ManVec<pInd_t> &indexes, size_t fullSize, const std::tuple<Names...> &fieldNames, const std::tuple<Fields...> &outFields) {

        unpackVals(fieldNames, outFields, indexes, fullSize);
    }

    void closeFile() {
        w.closeFile();
    }
};

#endif //PHELL_HDF5WRITER_H
