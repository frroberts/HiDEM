//
// Created by Fredrik Robertsen on 6/24/24.
//

#ifndef PHELL_MPIHELPERS_H
#define PHELL_MPIHELPERS_H

#include <mpi.h>

template<class T>
MPI_Datatype getMPIType() {
    if (std::is_same<T, double>::value) {
        return MPI_DOUBLE;
    } else if (std::is_same<T, float>::value) {
        return MPI_FLOAT;
    } else if (std::is_same<T, int>::value) {
        return MPI_INT;
    } else if (std::is_same<T, long long int>::value) {
        return MPI_INT64_T;
    } else if (std::is_same<T, unsigned int>::value) {
        return MPI_UINT32_T;
    } else if (std::is_same<T, unsigned long long int>::value) {
        return MPI_UINT64_T;
    } else if (std::is_same<T, unsigned char>::value) {
        return MPI_CHAR;
    } else if (std::is_same<T, uint8_t>::value) {
        return MPI_UINT8_T;
    } else if (std::is_same<T, bool>::value) {
        return MPI_UINT8_T;
    } else if (std::is_same<T, signed char>::value) {
        return MPI_CHAR;
    } else {
        std::cerr << "unsupported mpi type" << std::endl;
        exit(0);
    }

}


#endif //PHELL_MPIHELPERS_H
