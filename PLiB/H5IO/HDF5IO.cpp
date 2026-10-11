//
// Created by Fredrik Robertsen on 6/22/26.
//

#include "HDF5IO.h"
bool HDF5IO::compression = true;
unsigned int HDF5IO::compressionLevel = 4;

HDF5IO::HDF5IO() {
    //
}

size_t HDF5IO::getDataSetSize(std::string name) {
    hid_t target = getTarget();
    auto datasetID = H5Dopen1(target, name.c_str());
    hsize_t size = H5Sget_simple_extent_npoints(H5Dget_space(datasetID));
    return size;
}

HDF5Span HDF5IO::generateSpan(size_t localSize) {
    HDF5Span span;

    span.localCount = localSize;

    span.localOffset = 0;
    MPI_Allreduce(&span.localCount, &span.globalCount, 1, MPI_LONG_LONG_INT, MPI_SUM, comm);
    MPI_Exscan(&span.localCount, &span.localOffset, 1, MPI_LONG_LONG_INT, MPI_SUM, comm);

    return span;
}

std::vector<HDF5Span> HDF5IO::readSpan(std::string prefix) {
    std::vector<long long int> offsets;
    std::vector<long long int> sizes;
    std::vector<HDF5Span> spans;

    if (prefix.length() != 0)
        prefix = prefix + "_";
    sizes = readAttributeArr<long long int>(std::string(prefix + "spann_sizes"));
    offsets = readAttributeArr<long long int>(std::string(prefix + "spann_offsets"));
    long long int globalCount = 0;
    for (size_t i = 0; i < sizes.size(); i++) {
        globalCount += sizes[i];
    }

    for (size_t i = 0; i < sizes.size(); i++) {
        HDF5Span s;
        s.localCount = sizes[i];
        s.localOffset = offsets[i];
        s.globalCount = globalCount;
        spans.push_back(s);
    }
    return spans;
}

void HDF5IO::writeSpan(HDF5Span &span, std::string prefix) {
    int worldSize;
    MPI_Comm_size(comm, &worldSize);
    std::vector<ind_t> offsets;
    std::vector<ind_t> sizes;
    offsets.resize(worldSize);
    sizes.resize(worldSize);
    //
    MPI_Allgather(&span.localCount, 1, MPI_LONG_LONG_INT, sizes.data(), 1, MPI_LONG_LONG_INT, comm);
    MPI_Allgather(&span.localOffset, 1, MPI_LONG_LONG_INT, offsets.data(), 1, MPI_LONG_LONG_INT, comm);


    if (prefix.length() != 0)
        prefix = prefix + "_";
    writeAttributeArr(std::string(prefix + "spann_sizes"), sizes);
    writeAttributeArr(std::string(prefix + "spann_offsets"), offsets);
}

void HDF5IO::writeAttributeStr(std::string name, std::string values) {
    // todo clean
    hid_t target = getTarget();

    auto attr_space = H5Screate(H5S_SCALAR);
    auto attr_type = H5Tcopy(H5T_C_S1);
    checkH5Error(H5Tset_size(attr_type, values.size()));
    checkH5Error(H5Tset_strpad(attr_type, H5T_STR_NULLTERM));
    checkH5Error(H5Tset_cset(attr_type, H5T_CSET_ASCII));

    auto attr = H5Acreate2(target, name.c_str(), attr_type, attr_space, H5P_DEFAULT, H5P_DEFAULT);

    checkH5Error(H5Awrite(attr, attr_type, values.data()));

    checkH5Error(H5Tclose(attr_type));
    checkH5Error(H5Sclose(attr_space));
    checkH5Error(H5Aclose(attr));
}

void HDF5IO::openGroup(std::string groupName) {
    //
    hid_t group;
    hid_t target = getTarget();

    checkH5Error(H5Eset_auto1(NULL, NULL));
    auto status = H5Gget_objinfo(target, groupName.c_str(), 0, NULL);
    checkH5Error(H5Eset_auto1(NULL, NULL));
    if (status >= 0) {
        group = H5Gopen(target, groupName.c_str(), H5P_DEFAULT);
    } else {
        group = H5Gcreate(target, groupName.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    }
    groupStack.push(group);
}

void HDF5IO::closeGroup() {
    checkH5Error(H5Gclose(groupStack.top()));

    groupStack.pop();
}

void HDF5IO::createFile(std::string path, MPI_Comm targetCommunicator) {
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

void HDF5IO::openFile(std::string path) {
    // Set up file access property list with parallel I/O access
    auto propertyList = H5Pcreate(H5P_FILE_ACCESS);
#ifndef SERIAL_HDF5
    checkH5Error(H5Pset_fapl_mpio(propertyList, comm, MPI_INFO_NULL));
#endif

    // Create a new file collectively and release property list identifier.
    fileID = H5Fopen(path.c_str(), H5P_DEFAULT, propertyList);
    checkH5Error(H5Pclose(propertyList));
}

void HDF5IO::closeFile() {
    checkH5Error(H5Fclose(fileID));
}
