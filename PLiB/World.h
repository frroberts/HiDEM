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

    void domiansToVTKHDF(std::string fileName) {
        //
        HDF5IO io;
        io.createFile(fileName, MPI_COMM_WORLD);

        io.openGroup("VTKHDF");
        io.writeAttributeStr("Type", "UnstructuredGrid");
        std::vector<long long int> version = {2, 2};
        io.writeAttributeArr("Version", version);

        ManVec<hpReal_t> points;
        ManVec<int> connectivity;
        ManVec<unsigned char> types;
        ManVec<int> owner;

        int conn = 0;
        int numCells = 0;

        for (auto d: domains) {

            owner.push_back(d.owner);

            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            connectivity.push_back(conn++);
            types.push_back(11);
            numCells++;

            points.push_back(d.lower.x);
            points.push_back(d.lower.y);
            points.push_back(d.lower.z);

            points.push_back(d.upper.x);
            points.push_back(d.lower.y);
            points.push_back(d.lower.z);

            points.push_back(d.lower.x);
            points.push_back(d.upper.y);
            points.push_back(d.lower.z);

            points.push_back(d.upper.x);
            points.push_back(d.upper.y);
            points.push_back(d.lower.z);

            points.push_back(d.lower.x);
            points.push_back(d.lower.y);
            points.push_back(d.upper.z);

            points.push_back(d.upper.x);
            points.push_back(d.lower.y);
            points.push_back(d.upper.z);

            points.push_back(d.lower.x);
            points.push_back(d.upper.y);
            points.push_back(d.upper.z);

            points.push_back(d.upper.x);
            points.push_back(d.upper.y);
            points.push_back(d.upper.z);
        }

        Iterator::syncAll();

        io.writeVectorAll2D(points, "Points", 3);
        ManVec<long long int> NumberOfPoints;
        NumberOfPoints.push_back(points.size() / 3);
        io.writeVectorFirst(NumberOfPoints, "NumberOfPoints");

        ManVec<int> NumberOfConnectivityIds;
        NumberOfConnectivityIds.push_back(numCells * 8);
        io.writeVectorFirst(NumberOfConnectivityIds, "NumberOfConnectivityIds");

        ManVec<int> NumberOfCells;
        NumberOfCells.push_back(numCells);
        io.writeVectorFirst(NumberOfCells, "NumberOfCells");

        ManVec<int> Offsets;
        for (int i = 0; i < numCells + 1; i++) {
            Offsets.push_back((i) * 8);
        }
        io.writeVectorFirst(Offsets, "Offsets");

        io.writeVectorFirst(connectivity, "Connectivity");
        io.writeVectorFirst(types, "Types");

        io.openGroup("CellData");
        io.writeVectorFirst(owner, "owner");
        io.closeGroup();
        io.closeGroup();

        io.closeFile();
    }

};


#endif