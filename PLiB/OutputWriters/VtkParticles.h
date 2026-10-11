//
// Created by Fredrik Robertsen on 1/24/24.
//

#ifndef PHELL_VTKPARTICLES_H
#define PHELL_VTKPARTICLES_H

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <filesystem>
#include "Util/Util.h"

#include <mpi.h>
#include <fstream>

#include <fmt/format.h>
#include <ManVec.h>

class VtkParticles{
    size_t offset;
    std::filesystem::path filePath;

    std::string xmlPreamble;
    std::string xmlPostamble;
    std::string xmlPreBin;

    MPI_Offset binStart;

    std::unordered_map<std::string, size_t > blockStarts;
    std::unordered_map<std::string, size_t > blockSizes;

    std::fstream file;
    MPI_File fh;

    template<class T>
    void xmlParticles(ManVec<T>& xCords, ManVec<T> &yCords, ManVec<T> &zCords, ManVec<pInd_t> &indexes) {
        blockStarts["points"] = offset;
        // int for block size, then 3x xCords x sizeof
        size_t localSize = sizeof(int) + (sizeof (T)*3*indexes.size());
        blockSizes["points"] = localSize;

        int globalCount;
        int localCount = indexes.size();
        MPI_Allreduce(&localCount, &globalCount, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        offset += globalCount * 3 * sizeof(T) + sizeof(int);


        xmlPreamble += R"(<UnstructuredGrid>)";
        xmlPreamble += "\n";
        xmlPreamble += fmt::format(R"(<Piece NumberOfPoints="{}" NumberOfCells="0">)", globalCount);
        xmlPreamble += "\n";
        xmlPreamble += R"(<Points>)";
        xmlPreamble += "\n";

        xmlPreamble += fmt::format(R"(<DataArray Name="points" NumberOfComponents="3" type="{}" format="appended" offset="0"/>)", getType<T>());
        xmlPreamble += "\n";

        xmlPreamble += R"(</Points>)";
        xmlPreamble += "\n";

        xmlPreamble += R"(<PointData>)";

        xmlPreBin += R"(</PointData>)";

        xmlPreBin += R"(<Cells>)";
        xmlPreBin += R"(<DataArray type="Int64" Name="connectivity" format="ascii">)";
        xmlPreBin += R"(</DataArray>)";
        xmlPreBin += R"(<DataArray type="Int64" Name="offsets" format="ascii" RangeMin="1e+299" RangeMax="-1e+299">)";
        xmlPreBin += R"(</DataArray>)";
        xmlPreBin += R"(</Cells>)";
        xmlPreBin += R"(</Piece>)";
        xmlPreBin += R"(</UnstructuredGrid>)";
        xmlPreBin += R"(<AppendedData encoding="raw">)";
        xmlPreBin += "\n";
        xmlPreBin += "_";
    }

    template<class T>
    void xmlScalar(std::string name, T* data, ManVec<pInd_t> &indexes){
        blockStarts[name] = offset;
        size_t localSize = (sizeof (T)*indexes.size());
        size_t globalSize;
        MPI_Allreduce(&localSize, &globalSize, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
        // reduce global here for mpiio
        blockSizes[name] = globalSize + sizeof(T);
        xmlPreamble += fmt::format(R"(<DataArray Name="{}" NumberOfComponents="1" type="{}" format="appended" offset="{}"/>)", name, getType<T>(), offset);
        xmlPreamble += "\n";
        offset += globalSize + sizeof(T);
    }

    template<class T>
    void binParticles(ManVec<T> &xCords, ManVec<T> &yCords, ManVec<T> &zCords, ManVec<pInd_t> &indexes){
        //
        std::vector<T> intermixed;
        intermixed.resize(indexes.size()*3);
        Iterator::syncAll();

        for (int i = 0; i < indexes.size(); ++i) {
            pInd_t ind = indexes[i];
            intermixed[(i*3) + 0] = (xCords[ind]);
            intermixed[(i*3) + 1] = (yCords[ind]);
            intermixed[(i*3) + 2] = (zCords[ind]);
        }
        Iterator::syncAll();

        uint32_t localSize = intermixed.size()* sizeof(T);
        uint32_t globalSize;
        MPI_Allreduce(&localSize, &globalSize, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);

        writeLocalChunk(intermixed, binStart, globalSize, localSize);
    }

    template<class T>
    void binScalar(T *val, std::string name, ManVec<pInd_t> &indexes){
        //
        MPI_Offset fileOffset = blockStarts[name] + binStart;
        uint32_t globalSize = blockSizes[name];
        uint32_t localSize = indexes.size()* sizeof(T);
        std::vector<T> selected;
        for (int i = 0; i < indexes.size(); ++i) {
            pInd_t ind = indexes[i];
            selected.push_back(val[ind]);
        }

        writeLocalChunk(selected, fileOffset, globalSize, localSize);
    }

    template<class T>
    void writeLocalChunk(std::vector<T> &val, MPI_Offset &fileOffset, uint32_t &globalSize, uint32_t &localSize) const {
        // copy indexed vals to write buffer

        Iterator::syncAll();

        MPI_File_seek(fh, fileOffset, MPI_SEEK_SET);

        int rank = 0;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        // rank 0 writes block size, the rest seek 4 bytes forward
        if(rank == 0) {
            MPI_File_write(fh, &globalSize, 1, MPI_UINT32_T, MPI_STATUS_IGNORE);
        } else {
            MPI_File_seek(fh, sizeof(uint32_t), MPI_SEEK_CUR);
        }

        // seek to own place
        uint32_t localOffset = 0;
        MPI_Exscan(&localSize, &localOffset, 1, MPI_UINT32_T, MPI_SUM, MPI_COMM_WORLD);
        MPI_File_seek(fh, localOffset, MPI_SEEK_CUR);

        MPI_File_write_all(fh, val.data(), localSize, MPI_BYTE, MPI_STATUS_IGNORE);
    }

    template<typename ...Names, typename ...Fields>
    void unpackBinVals(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, ManVec<pInd_t> &indexes) {
        unpackBinImpl(fieldNames, outFields, indexes, std::make_index_sequence<sizeof...(Fields)>());
    }

    template<std::size_t... ints, typename ...Names, typename ...Fields>
    void unpackBinImpl(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, ManVec<pInd_t> &indexes,
                        std::integer_sequence<std::size_t, ints...>) {
        ((binScalar(std::get<ints>(outFields), std::get<ints>(fieldNames), indexes) ), ...);
    }

    template<typename ...Names, typename ...Fields>
    void unpackXmlVals(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, ManVec<pInd_t> &indexes) {
        unpackXmlImpl(fieldNames, outFields, indexes, std::make_index_sequence<sizeof...(Names)>());
    }

    template<std::size_t... ints, typename ...Names, typename ...Fields>
    void unpackXmlImpl(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, ManVec<pInd_t> &indexes,
                        std::integer_sequence<std::size_t, ints...>) {
        ((xmlScalar(std::get<ints>(fieldNames), std::get<ints>(outFields), indexes) ), ...);
    }

    template<class T>
    std::string getType() {
        if (std::is_same<T, double>::value) {
            return "Float64";
        } else if (std::is_same<T, float>::value) {
            return "Float32";
        } else if (std::is_same<T, int>::value) {
            return "Int32";
        } else if (std::is_same<T, long long int>::value) {
            return "Int64";
        } else if (std::is_same<T, unsigned int>    ::value) {
            return "Uint32";
        } else if (std::is_same<T, unsigned long long int>::value) {
            return "Uint64";
        } else if (std::is_same<T, uint8_t>::value) {
            return "Uint8";
        } else if (std::is_same<T, bool>::value) {
            return "Uint8";
        } else if (std::is_same<T, signed char>::value) {
            return "Int8";
        }
        exit(1);
        return "";
    }

public:
    VtkParticles() {
        offset = 0;
        xmlPreamble = R"(<VTKFile type="UnstructuredGrid" version="1.0" byte_order="LittleEndian">)";
        xmlPreBin = "";
        xmlPostamble = R"(</AppendedData> </VTKFile>)";
    }

    void createFile(std::string path) {
        if(getLocalRank() == 0){
            filePath = std::filesystem::path(path);
            if(std::filesystem::exists(filePath)){
                std::filesystem::remove(filePath);
            }
        }
        MPI_File_open(MPI_COMM_WORLD, filePath.c_str(), MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fh);
    }

    template<class CordT, typename ...Names, typename ...Fields>
    void writerParticlesIndexed(ManVec<CordT> &xCords, ManVec<CordT> &yCords, ManVec<CordT> &zCords, ManVec<pInd_t> &indexes,
                           std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields){

        xmlParticles(xCords, yCords, zCords, indexes);

        unpackXmlVals(fieldNames, outFields, indexes);

        std::string xmlStr = xmlPreamble + xmlPreBin;

        MPI_File_preallocate(fh, xmlStr.size()+xmlPostamble.size()+offset);

        int rank = 0;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if(rank == 0) {
            MPI_File_write(fh, xmlStr.c_str(), xmlStr.size(), MPI_CHAR, MPI_STATUS_IGNORE);
            MPI_File_get_position(fh, &binStart);
        }
        MPI_Bcast(&binStart, sizeof(MPI_Offset), MPI_BYTE, 0, MPI_COMM_WORLD);

        binParticles(xCords, yCords, zCords, indexes);

        unpackBinVals(fieldNames, outFields, indexes);

        if(rank == 0) {
            MPI_File_write(fh, xmlPostamble.c_str(), xmlPostamble.size(), MPI_CHAR, MPI_STATUS_IGNORE);
        }
    }

    void closeFile(){
        MPI_File_close(&fh);

    }

};


#endif //PHELL_VTKPARTICLES_H
