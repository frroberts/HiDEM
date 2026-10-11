//
// Created by Fredrik Robertsen on 25/12/2020.
//

#ifndef PHELL_WORLD_H
#define PHELL_WORLD_H

#include <vector>
#include "Domain.h"
#include "Util/Util.h"

#include <mpi.h>

#ifdef GPU
#include "DevPortabilityLayer.h"
#endif

class World
{
private:
    /* data */
public:
    World()
    {
        self = 0;
        MPI_Comm_rank(MPI_COMM_WORLD, &self);
        MPI_Comm_size(MPI_COMM_WORLD, &size);

    }
    std::vector<Domain> domains;

    int self;
    int size;

    Domain getOwnDomain() const{
        for(auto d: domains){
            if(d.owner == self)
                return d;
        }
        Domain d;
        d.owner = -1;
        return d;
    }

};


#endif