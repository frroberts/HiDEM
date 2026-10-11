//
// Created by Fredrik Robertsen on 25/10/2021.
//

#ifndef PHELL_HDF5IO_H
#define PHELL_HDF5IO_H

#include <vector>
#include <unordered_map>
#include <optional>
#include <numeric>

#include <ManVec.h>
#include <Iterator.h>

#include <stack>

#include "Util/Util.h"

#ifdef ENABLE_HDF5

#include <hdf5.h>


class HDF5Span{
    bool spaceSet = false;
    hid_t spannSpace;
    int rank;

public:
    hsize_t localOffset;
    hsize_t localCount;
    hsize_t globalCount;

    hid_t getSpace(hid_t datasetID){
        if(spaceSet){
            H5Sclose(spannSpace);
        }

        spannSpace = H5Dget_space(datasetID);
        H5Sselect_hyperslab(spannSpace, H5S_SELECT_SET, &localOffset, NULL, &localCount, NULL);
        spaceSet = true;
        return spannSpace;
    }

    ~HDF5Span(){
        if(spaceSet){
            H5Sclose(spannSpace);
        }
    }
};

class HDF5IO {

    void checkH5Error(hid_t err) {
        if(err != 0) {
            H5Eprint2(H5E_DEFAULT, stderr);
            std::cout << err << std::endl;
            throw std::runtime_error("HDF5IO Error");
        }
    }

    hid_t fileID;
    std::stack<hid_t> groupStack;
    MPI_Comm comm = MPI_COMM_WORLD;

    // H5FD_MPIO_COLLECTIVE
    H5FD_mpio_xfer_t transferType = H5FD_MPIO_COLLECTIVE;

    bool compression = true;
    unsigned int compressionLevel = 4;


    auto getGlobalPos(hsize_t &localCount) const {
        hsize_t localOffset = 0;
        hsize_t globalCount;
        MPI_Allreduce(&localCount, &globalCount, 1, MPI_LONG_LONG_INT, MPI_SUM, comm);
        MPI_Exscan(&localCount, &localOffset, 1, MPI_LONG_LONG_INT, MPI_SUM, comm);

        return std::make_tuple(localOffset, globalCount);
    }

    template<class T>
    hid_t getH5Type() {
        if (std::is_same<T, double>::value) {
            return H5T_NATIVE_DOUBLE;
        } else if (std::is_same<T, float>::value) {
            return H5T_NATIVE_FLOAT;
        } else if (std::is_same<T, int>::value) {
            return H5T_NATIVE_INT32;
        } else if (std::is_same<T, long long int>::value) {
            return H5T_NATIVE_INT64;
        } else if (std::is_same<T, unsigned int>::value) {
            return H5T_NATIVE_UINT32;
        } else if (std::is_same<T, unsigned long long int>::value) {
            return H5T_NATIVE_UINT64;
        }else if (std::is_same<T, unsigned char>::value) {
            return H5T_NATIVE_UCHAR;
        } else if (std::is_same<T, uint8_t>::value) {
            return H5T_NATIVE_UINT8;
        } else if (std::is_same<T, bool>::value) {
            return H5T_NATIVE_UINT8;
        } else if (std::is_same<T, signed char>::value) {
            return H5T_NATIVE_INT8;
        }
        throw std::runtime_error("Unsupported h5 type");
        return H5T_NATIVE_INT_g;
    }


    hid_t getTarget() {
        hid_t target;
        if(groupStack.size() != 0)
            target = groupStack.top();
        else
            target = fileID;
        return  target;
    }

    template<class T, int NDimms>
    hid_t setCompression(hsize_t size[NDimms]) {
        hid_t dcpl_id = H5P_DEFAULT;
        // check that the size is more than 0 to avoid errors
        if (compression && size[0] > 0) {
            // if compression is enabled, create a chunked dataset with compression
            dcpl_id = H5Pcreate(H5P_DATASET_CREATE);
            hsize_t blockSize[NDimms];
            for (hsize_t i = 0; i < NDimms; i++) {
                blockSize[i] = size[i];
            }
            blockSize[0] = std::min((hsize_t) ((1024 * 1024 * 4) / sizeof(T)), size[0]);
            checkH5Error(H5Pset_chunk(dcpl_id, NDimms, blockSize));
            unsigned int cd_values[1];
            cd_values[0] = {compressionLevel};
            checkH5Error(H5Pset_filter(dcpl_id, H5Z_FILTER_DEFLATE, H5Z_FLAG_MANDATORY, (size_t) 1, cd_values));
        }
        return dcpl_id;
    }

public:
    HDF5IO(bool compression = false, int compressionLevel = 4) : compression(compression), compressionLevel(compressionLevel) {

    }

    void createFile(std::string path, MPI_Comm targetCommunicator = MPI_COMM_WORLD) {
        comm = targetCommunicator;

        // Set up file access property list with parallel I/O access
        auto propertyList = H5Pcreate(H5P_FILE_ACCESS);
#ifndef SERIAL_HDF5
        checkH5Error(H5Pset_fapl_mpio(propertyList, comm, MPI_INFO_NULL));
#endif

        // Create a new file collectively and release property list identifier.
        fileID = H5Fcreate(path.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, propertyList);
        if (fileID == H5I_INVALID_HID) {
            checkH5Error(fileID);
        }

        checkH5Error(H5Pclose(propertyList));



    }

    void openFile(std::string path) {
        // Set up file access property list with parallel I/O access
        auto propertyList = H5Pcreate(H5P_FILE_ACCESS);
#ifndef SERIAL_HDF5
        checkH5Error(H5Pset_fapl_mpio(propertyList, comm, MPI_INFO_NULL));
#endif

        // Create a new file collectively and release property list identifier.
        fileID = H5Fopen(path.c_str(), H5P_DEFAULT, propertyList);
        checkH5Error(H5Pclose(propertyList));

    }

    void closeFile() {
        H5Fclose(fileID);
    }

    template<class T>
    void writeAttribute(std::string name, T value) {
        //

        auto aid2 = H5Screate(H5S_SCALAR);
        if (aid2 == H5I_INVALID_HID) {
            checkH5Error(aid2);
        }
        auto attr2 = H5Acreate2(fileID, name.c_str(), getH5Type<T>(), aid2, H5P_DEFAULT, H5P_DEFAULT);
        if (attr2 == H5I_INVALID_HID) {
            checkH5Error(attr2);
        }

        checkH5Error(H5Awrite(attr2, getH5Type<T>(), &value));

        checkH5Error(H5Sclose(aid2));
        checkH5Error(H5Aclose(attr2));

    }

    void openGroup(std::string groupName) {
        //
        hid_t group;
        hid_t target = getTarget();

        checkH5Error(H5Eset_auto1(NULL, NULL));
        auto status = H5Gget_objinfo (target, groupName.c_str(), 0, NULL);
        checkH5Error(H5Eset_auto1(NULL, NULL));
        if (status >= 0) {
            group = H5Gopen(target, groupName.c_str(), H5P_DEFAULT);
        } else {
            group = H5Gcreate(target, groupName.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        }
        groupStack.push(group);

    }

    void closeGroup() {
        checkH5Error(H5Gclose(groupStack.top()));

        groupStack.pop();
    }

    template<class T>
    void writeAttributeArr(std::string name, std::vector<T> &values) {
        //
        hid_t target = getTarget();

        auto aid1 = H5Screate(H5S_SIMPLE);
        hsize_t size = values.size();
        checkH5Error(H5Sset_extent_simple(aid1, 1, &size, NULL));

        auto attr1 = H5Acreate2(target, name.c_str(), getH5Type<T>(), aid1, H5P_DEFAULT, H5P_DEFAULT);

        checkH5Error(H5Awrite(attr1, getH5Type<T>(), values.data()));

        checkH5Error(H5Sclose(aid1));
        checkH5Error(H5Aclose(attr1));
    }

    void writeAttributeStr(std::string name, std::string values) {
        // todo clean
        hid_t target = getTarget();

        auto attr_space = H5Screate(H5S_SCALAR);
        auto attr_type = H5Tcopy(H5T_C_S1);
        checkH5Error(H5Tset_size(attr_type, values.size()));
        checkH5Error(H5Tset_strpad(attr_type, H5T_STR_NULLTERM));
        checkH5Error(H5Tset_cset(attr_type, H5T_CSET_ASCII));

        auto attr = H5Acreate2(target, name.c_str(), attr_type, attr_space, H5P_DEFAULT, H5P_DEFAULT);

        checkH5Error(H5Awrite(attr, attr_type, values.data()));

        checkH5Error(H5Sclose(attr_space));
        checkH5Error(H5Aclose(attr));
    }




    void checkH5ErrorDataset(auto datasetID) {
        if (datasetID == H5I_INVALID_HID) {
            checkH5Error(datasetID);
        }
    }

    template<class T>
    void writeDataSpan(T *data, std::string name, HDF5Span span) {
        //
        hid_t target = getTarget();
        hid_t fileSpace;
        hid_t memSpace; /* file and memory dataspace identifiers */

        fileSpace = H5Screate_simple(1, &span.globalCount, NULL);

        hsize_t dims[1];
        dims[0] = span.globalCount;

        hid_t datasetCreationProperty = setCompression<T, 1>(dims);
        // Create the dataset with default properties and close fileSpace.
        auto datasetID = H5Dcreate(target, name.c_str(), getH5Type<T>(), fileSpace,
                                   H5P_DEFAULT, datasetCreationProperty, H5P_DEFAULT);
        checkH5ErrorDataset(datasetID);
        checkH5Error(H5Pclose(datasetCreationProperty));
        checkH5Error(H5Sclose(fileSpace));

        // Each process defines dataset in memory and writes it to the hyperslab in the file.
        
        memSpace = H5Screate_simple(1, &span.localCount, NULL);
        // Select hyperslab in the file.
        fileSpace = span.getSpace(datasetID);

        // Create property list for collective dataset write.
        auto propertyList = H5Pcreate(H5P_DATASET_XFER);
        checkH5Error(H5Pset_dxpl_mpio(propertyList, transferType));

        checkH5Error(H5Dwrite(datasetID, getH5Type<T>(), memSpace, fileSpace,
                               propertyList, data));

        checkH5Error(H5Dclose(datasetID));
        checkH5Error(H5Sclose(memSpace));
        checkH5Error(H5Pclose(propertyList));
    }

    template<class T, class Tind>
    void writeDataIndexedSpan(T *data, hsize_t dataSize, Tind *indexes, std::string name, HDF5Span span) {
        //
        ManVec<T> dataTemp;
        dataTemp.resize(span.localCount);

        Iterator::gather(OffloadExec(), dataTemp.data(), data, indexes, span.localCount);
        Iterator::syncAll();

        writeDataSpan(dataTemp.data(), name, span);
    }

    template<class T, class Tind>
    void writeDataIndexed(T *data, hsize_t dataSize, Tind *indexes, hsize_t numIndexes, std::string name) {
        //
        HDF5Span span = generateSpan(numIndexes);
        writeDataIndexedSpan(data, dataSize, indexes, name, span);
    }

    template<class T>
    void writeVectorFirst(ManVec<T> &data, std::string name) {
        // todo add check that if data is more than 1000 elements start doing parallel writes
        HDF5Span span;
        span.globalCount = data.size();
        span.localOffset = 0;
        if (getLocalRank() == 0) {
            span.localCount = data.size();
        } else {
            span.localCount = 0;
        }
        writeDataSpan(data.data(), name, span);
    }

    template<class T>
    void writeVectorAll2D(ManVec<T> &data, std::string name, hsize_t dim2) {
                //
        hid_t target = getTarget();
        hid_t fileSpace;
        hid_t memSpace;

        hsize_t localCount = data.size();

        auto [localOffset, globalCount] = getGlobalPos(localCount);

        hsize_t globalCount2D[2] = {globalCount/dim2,dim2};
        hsize_t localCount2D[2] = {localCount/dim2,dim2};
        hsize_t localOffset2D[2] = {localOffset/dim2,0};

        fileSpace = H5Screate_simple(2, globalCount2D, NULL);

        hid_t datasetCreationProperty = setCompression<T, 2>(globalCount2D);

        // Create the dataset with default properties and close fileSpace.
        auto datasetID = H5Dcreate(target, name.c_str(), getH5Type<T>(), fileSpace,
                                   H5P_DEFAULT, datasetCreationProperty, H5P_DEFAULT);
        checkH5ErrorDataset(datasetID);
        checkH5Error(H5Pclose(datasetCreationProperty));
        checkH5Error(H5Sclose(fileSpace));

        // Each process defines dataset in memory and writes it to the hyperslab in the file.
        memSpace = H5Screate_simple(1, &localCount, NULL);
        // Select hyperslab in the file.
        fileSpace = H5Dget_space(datasetID);
        checkH5Error(H5Sselect_hyperslab(fileSpace, H5S_SELECT_SET, localOffset2D, NULL, localCount2D, NULL));

        // Create property list for collective dataset write.
        auto propertyList = H5Pcreate(H5P_DATASET_XFER);
        checkH5Error(H5Pset_dxpl_mpio(propertyList, transferType));

        checkH5Error(H5Dwrite(datasetID, getH5Type<T>(), memSpace, fileSpace,propertyList, data.data()));
        checkH5Error(H5Dclose(datasetID));
        checkH5Error(H5Sclose(fileSpace));
        checkH5Error(H5Sclose(memSpace));
        checkH5Error(H5Pclose(propertyList));

    }


    template<class Fun, class... T>
    void writeMultiPred(size_t numLocal, Fun predicate, std::vector<std::string> names, T&... fields){
        //
        // fill index vector
        ManVec<pInd_t > allIndexes;
        allIndexes.resize(numLocal);
        std::iota(allIndexes.begin(), allIndexes.end(), 0);

        // figure out how many we will select
        int selectedCount = std::count_if(allIndexes.begin(), allIndexes.end(), predicate);

        // copy selected
        ManVec<pInd_t > selectedIndexes;
        selectedIndexes.resize(selectedCount);
        std::copy_if(allIndexes.begin(), allIndexes.end(), selectedIndexes.begin(), predicate);

        // write out the passed fields
        int fieldCount = 0;
        // grabbing a ptr and then dereferencing it as manVec has some things deleted
        for(auto field: {&fields...}){
            writeDataIndexed((*field).data(), (*field).size(), selectedIndexes.data(), selectedIndexes.size(), names[fieldCount]);
//            writeVectorIndexed(*field, selectedIndexes, names[fieldCount]);
            fieldCount++;
        }
    }

    template<class T>
    void writeDataFileShape(T *data, std::string name, HDF5Span span) {
        //
        hid_t target = getTarget();
        hid_t fileSpace;
        hid_t memSpace;      /* file and memory dataspace identifiers */

        fileSpace = H5Screate_simple(1, &span.globalCount, NULL);

        // Create the dataset with default properties and close fileSpace.
        auto datasetID = H5Dcreate(target, name.c_str(), getH5Type<T>(), fileSpace,
                                   H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        checkH5Error(H5Sclose(fileSpace));

        // Each process defines dataset in memory and writes it to the hyperslab in the file.

        memSpace = H5Screate_simple(1, &span.localCount, NULL);
        // Select hyperslab in the file.
        fileSpace = span.getSpace(datasetID);

        // Create property list for collective dataset write.
        auto propertyList = H5Pcreate(H5P_DATASET_XFER);
        checkH5Error(H5Pset_dxpl_mpio(propertyList, transferType));

        checkH5Error(H5Dwrite(datasetID, getH5Type<T>(), memSpace, fileSpace, propertyList, data));

        checkH5Error(H5Dclose(datasetID));
        checkH5Error(H5Sclose(memSpace));
        checkH5Error(H5Pclose(propertyList));
    }

    void writeSpan(HDF5Span span, std::string prefix = ""){
        int worldSize;
        MPI_Comm_size(comm, &worldSize);
        std::vector<ind_t> offsets;
        std::vector<ind_t> sizes;
        offsets.resize(worldSize);
        sizes.resize(worldSize);
        //
        MPI_Allgather(&span.localCount, 1, MPI_LONG_LONG_INT, sizes.data(), 1, MPI_LONG_LONG_INT, comm);
        MPI_Allgather(&span.localOffset, 1, MPI_LONG_LONG_INT, offsets.data(), 1, MPI_LONG_LONG_INT, comm);

        
        if(prefix.length() != 0)
            prefix = prefix + "_";
        writeAttributeArr(std::string(prefix+"spann_sizes"), sizes);
        writeAttributeArr(std::string(prefix+"spann_offsets"), offsets);
    
    }

    std::vector<HDF5Span> readSpan(std::string prefix = ""){
        std::vector<long long int> offsets;
        std::vector<long long int> sizes;
        std::vector<HDF5Span> spans;
        
        if(prefix.length() != 0)
            prefix = prefix + "_";
        sizes = readAttributeArr<long long int>(std::string(prefix+"spann_sizes"));
        offsets = readAttributeArr<long long int>(std::string(prefix+"spann_offsets"));
        long long int globalCount = 0;
        for (size_t i = 0; i < sizes.size(); i++)
        {
            globalCount += sizes[i];
        }
        
        for (size_t i = 0; i < sizes.size(); i++)
        {
            HDF5Span s;
            s.localCount = sizes[i];
            s.localOffset = offsets[i];
            s.globalCount = globalCount;
            spans.push_back(s);
        }
        return spans;
    }

    template<class T>
    T readAttribute(std::string name) {
        hid_t target = getTarget();
        auto attr2 = H5Aopen_name(target, name.c_str());
        T value;
        checkH5Error(H5Aread(attr2, getH5Type<T>(), &value));
        checkH5Error(H5Aclose(attr2));

        return value;
    }

    template<class T>
    std::vector<T> readAttributeArr(std::string name) {
        //
        hid_t target = getTarget();
        std::vector<T> values;
        auto attr2 = H5Aopen(target, name.c_str(), H5P_DEFAULT);

        auto space = H5Aget_space (attr2);

        hsize_t size;

        H5Sget_simple_extent_dims(space, &size, NULL);
        checkH5Error(H5Sclose(space));

        values.resize(size);

        checkH5Error(H5Aread(attr2, getH5Type<T>(), values.data()));

        checkH5Error(H5Aclose(attr2));

        return values;

    }

    // read based on attribute of who owns what parts
    template<class T>
    ManVec<T> readVectorSpann(std::string name, HDF5Span spann) {
        //
        hid_t target = getTarget();

        int self;
        MPI_Comm_rank(comm, &self);

        ManVec<T> data;
        auto datasetID = H5Dopen1(target, name.c_str());

        auto fileSpace = H5Dget_space(datasetID);
        checkH5Error(H5Sselect_hyperslab(fileSpace, H5S_SELECT_SET, &spann.localOffset, NULL, &spann.localCount, NULL));

        auto memSpace =  H5Screate_simple(1, &spann.localCount, NULL);;
        data.resize(spann.localCount);

        // Create property list for collective dataset write.
        auto propertyList = H5Pcreate(H5P_DATASET_XFER);
        checkH5Error(H5Pset_dxpl_mpio(propertyList, transferType));

        checkH5Error(H5Dread(datasetID, getH5Type<T>(), memSpace, fileSpace, propertyList, data.data()));

        checkH5Error(H5Dclose(datasetID));
        checkH5Error(H5Sclose(fileSpace));
        checkH5Error(H5Sclose(memSpace));
        checkH5Error(H5Pclose(propertyList));
        return data;
    }

    template<class T>
    void readPtrSpann(std::string name, T* ptr, HDF5Span spann) {
        //
        hid_t target = getTarget();

        int self;
        MPI_Comm_rank(comm, &self);

        auto datasetID = H5Dopen1(target, name.c_str());

        auto fileSpace = H5Dget_space(datasetID);
        checkH5Error(H5Sselect_hyperslab(fileSpace, H5S_SELECT_SET, &spann.localOffset, NULL, &spann.localCount, NULL));
        auto memSpace =  H5Screate_simple(1, &spann.localCount, NULL);;

        // Create property list for collective dataset write.
        auto propertyList = H5Pcreate(H5P_DATASET_XFER);
        checkH5Error(H5Pset_dxpl_mpio(propertyList, transferType));

        checkH5Error(H5Dread(datasetID, getH5Type<T>(), memSpace, fileSpace, propertyList, ptr));

        checkH5Error(H5Dclose(datasetID));
        checkH5Error(H5Sclose(fileSpace));
        checkH5Error(H5Sclose(memSpace));
        checkH5Error(H5Pclose(propertyList));
    }



    HDF5Span generateSpan(size_t localSize){
        HDF5Span span;

        span.localCount = localSize;

        span.localOffset = 0;
        MPI_Allreduce(&span.localCount, &span.globalCount, 1, MPI_LONG_LONG_INT, MPI_SUM, comm);
        MPI_Exscan(&span.localCount, &span.localOffset, 1, MPI_LONG_LONG_INT, MPI_SUM, comm);

        return span;
    }
};

#else
// stubs to enable non hdf5 builds

typedef size_t hsize_t;
typedef size_t hid_t;

class HDF5Span{
    bool spaceSet = false;
    hid_t spannSpace;
    int rank;

public:
    hsize_t localOffset;
    hsize_t localCount;
    hsize_t globalCount;

    hid_t getSpace(hid_t datasetID){
        return 0;
    }

    ~HDF5Span(){
        //
    }
};

class HDF5IO {

public:
    HDF5IO() = default;

    void createFile(std::string path, MPI_Comm targetCommunicator = MPI_COMM_WORLD) {
    }

    void openFile(std::string path) {
    }

    void closeFile() {
    }

    template<class T>
    void writeAttribute(std::string name, T value) {
    }

    void openGroup(std::string groupName) {
    }

    void closeGroup() {
    }

    template<class T>
    void writeAttributeArr(std::string name, std::vector<T> &values) {
    }

    void writeAttributeStr(std::string name, std::string values) {
    }


    template<class T>
    void writeDataSpan(T *data, std::string name, HDF5Span span) {
    }

    template<class T, class Tind>
    void writeDataIndexedSpan(T *data, hsize_t dataSize, Tind *indexes, std::string name, HDF5Span span) {
    }

    template<class T, class Tind>
    void writeDataIndexed(T *data, hsize_t dataSize, Tind *indexes, hsize_t numIndexes, std::string name) {
    }

    template<class T>
    void writeVectorFirst(ManVec<T> &data, std::string name) {
    }

    template<class Fun, class... T>
    void writeMultiPred(size_t numLocal, Fun predicate, std::vector<std::string> names, T&... fields){
    }

    template<class T>
    void writeDataFileShape(T *data, std::string name, HDF5Span span) {
    }

    void writeSpan(HDF5Span span, std::string prefix = ""){
    }

    std::vector<HDF5Span> readSpan(std::string prefix = ""){
        return std::vector<HDF5Span>{};
    }

    template<class T>
    T readAttribute(std::string name) {
        return T{};
    }

    template<class T>
    std::vector<T> readAttributeArr(std::string name) {
        return std::vector<T>{};
    }

    // read based on attribute of who owns what parts
    template<class T>
    ManVec<T> readVectorSpann(std::string name, HDF5Span spann) {
        return ManVec<T>{};
    }

    template<class T>
    void readPtrSpann(std::string name, T* ptr, HDF5Span spann) {
    }


    template<class T>
    void writeVectorAll2D(ManVec<T> &data, std::string name, hsize_t dim2) {
    }

    HDF5Span generateSpan(size_t localSize){
        HDF5Span span;
        return span;
    }
};

#endif

#endif //PHELL_HDF5IO_H
