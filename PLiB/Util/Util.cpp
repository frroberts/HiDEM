//
// Created by Fredrik Robertsen on 12/11/2020.
//

#include "Util.h"

#include <mpi.h>

int getLocalRank()
{
    int self;
    self = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &self);

    return self;
}

