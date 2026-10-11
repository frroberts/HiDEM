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
#ifdef GPU
    // get the number of devices
    int devices;
    int devicesGlobalMin;
    devGetDeviceCount(&devices);
    MPI_Allreduce(&devices, &devicesGlobalMin, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    // reduce dev count, if min is not local count complain and exit
    if((devices != devicesGlobalMin)){
        std::cout << "different number of devices found per node" << std::endl;
        exit(1);
    }

    // split MPI_COMM_WORLD into shared memory communicator
    MPI_Comm shardMemComm;
    MPI_Info info;
    MPI_Info_create(&info);
    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, info, &shardMemComm);

    // get the id of the rank within the shared memory communicator
    // this effectively gives the id of the rank on its compute node
    int myRankOnNode;
    int nodeSize;
    MPI_Comm_rank(shardMemComm, &myRankOnNode);
    MPI_Comm_size(shardMemComm, &nodeSize);

    // set the device based on the id of the rank on the node
    // % number of devices to allow for oversubscription
    devSetDevice(myRankOnNode%devices);

    // printing of device selection
    devGetDevice(&selectedDevice);
    char hostname[128];
    int resLen;
    MPI_Get_processor_name(hostname, &resLen);

    char pcieId[13];
    devDeviceGetPCIBusId(pcieId, 13, selectedDevice);

    char * allIds;
    allIds = new char[13*nodeSize];

    MPI_Allgather(pcieId, 13, MPI_CHAR, allIds, 13, MPI_CHAR, shardMemComm);

    std::unordered_map<std::string, int> procsPerDev;
    for(int i = 0; i < nodeSize;i++){
        auto id = std::string(allIds+(i*13));
        if (procsPerDev.find(id) == procsPerDev.end()){
            procsPerDev[id] = 0;
        }
        procsPerDev[id]++;
    }
    if(myRankOnNode == 0) {
        for(auto id: procsPerDev){
            if(id.second != 1){
                std::cout << "Device " << id.first << " used by " << id.second << " ranks" << std::endl;
            }
        }
    }
    delete[] allIds;

    //std::cout << "rank " << self << " is rank " << myRankOnNode << " on host " << std::string(hostname) << " and was assigned device " << selectedDevice << std::endl;
#endif
    }
    std::vector<Domain> domains;

    int self;
    int size;
    int selectedDevice = -1;

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