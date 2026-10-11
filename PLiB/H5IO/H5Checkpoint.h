//
// Created by Fredrik Robertsen on 21/11/2021.
//

#ifndef PHELL_H5CHECKPOINT_H
#define PHELL_H5CHECKPOINT_H

#include <unordered_set>

#include <mpi.h>

#include "HDF5IO.h"
#include "../Domain.h"


class H5Checkpoint {

public:
    HDF5IO h5;


    H5Checkpoint() {


    }

    ~H5Checkpoint() {
    }

    void newCheckpooint(std::string path){
        h5.createFile(path);
    }

    void openCheckpooint(std::string path) {
        h5.openFile(path);
    }

    void closeFile(){
        h5.closeFile();
    }

    void writeTimestep(int timeStep){
        //
        h5.writeAttribute("timeStep", timeStep);
    }

    int readTimestep(){
        return h5.readAttribute<int>("timeStep");
    }

    void writeDomains(Domain &bounding){
        std::vector<Domain> allDomains;

        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        allDomains.resize(worldSize);

        MPI_Allgather(&bounding, sizeof(Domain), MPI_BYTE, allDomains.data(), sizeof(Domain), MPI_BYTE, MPI_COMM_WORLD);

        std::vector<double> domainLowerX;
        std::vector<double> domainLowerY;
        std::vector<double> domainLowerZ;
        std::vector<double> domainUpperX;
        std::vector<double> domainUpperY;
        std::vector<double> domainUpperZ;

        std::vector<double> domainLowerHaloX;
        std::vector<double> domainLowerHaloY;
        std::vector<double> domainLowerHaloZ;
        std::vector<double> domainUpperHaloX;
        std::vector<double> domainUpperHaloY;
        std::vector<double> domainUpperHaloZ;

        std::vector<int> owner;

        for(auto &d: allDomains){
            domainUpperX.push_back(d.upper.x);
            domainUpperY.push_back(d.upper.y);
            domainUpperZ.push_back(d.upper.z);
            domainLowerX.push_back(d.lower.x);
            domainLowerY.push_back(d.lower.y);
            domainLowerZ.push_back(d.lower.z);

            domainUpperHaloX.push_back(d.upperHalo.x);
            domainUpperHaloY.push_back(d.upperHalo.y);
            domainUpperHaloZ.push_back(d.upperHalo.z);
            domainLowerHaloX.push_back(d.lowerHalo.x);
            domainLowerHaloY.push_back(d.lowerHalo.y);
            domainLowerHaloZ.push_back(d.lowerHalo.z);

            owner.push_back(d.owner);
        }

        h5.openGroup("Domains");
        h5.writeAttributeArr("domainUpperX", domainUpperX);
        h5.writeAttributeArr("domainUpperY", domainUpperY);
        h5.writeAttributeArr("domainUpperZ", domainUpperZ);
        h5.writeAttributeArr("domainLowerX", domainLowerX);
        h5.writeAttributeArr("domainLowerY", domainLowerY);
        h5.writeAttributeArr("domainLowerZ", domainLowerZ);

        h5.writeAttributeArr("domainUpperHaloX", domainUpperHaloX);
        h5.writeAttributeArr("domainUpperHaloY", domainUpperHaloY);
        h5.writeAttributeArr("domainUpperHaloZ", domainUpperHaloZ);
        h5.writeAttributeArr("domainLowerHaloX", domainLowerHaloX);
        h5.writeAttributeArr("domainLowerHaloY", domainLowerHaloY);
        h5.writeAttributeArr("domainLowerHaloZ", domainLowerHaloZ);

        h5.writeAttributeArr("domainOwner", owner);
        h5.closeGroup();
    }

    std::vector<Domain> readDomains(){
        //
        std::vector<double> domainLowerX;
        std::vector<double> domainLowerY;
        std::vector<double> domainLowerZ;
        std::vector<double> domainUpperX;
        std::vector<double> domainUpperY;
        std::vector<double> domainUpperZ;

        std::vector<double> domainLowerHaloX;
        std::vector<double> domainLowerHaloY;
        std::vector<double> domainLowerHaloZ;
        std::vector<double> domainUpperHaloX;
        std::vector<double> domainUpperHaloY;
        std::vector<double> domainUpperHaloZ;
        std::vector<int> owner;

        h5.openGroup("Domains");
        domainUpperX = h5.readAttributeArr<double>("domainUpperX");
        domainUpperY = h5.readAttributeArr<double>("domainUpperY");
        domainUpperZ = h5.readAttributeArr<double>("domainUpperZ");
        domainLowerX = h5.readAttributeArr<double>("domainLowerX");
        domainLowerY = h5.readAttributeArr<double>("domainLowerY");
        domainLowerZ = h5.readAttributeArr<double>("domainLowerZ");

        domainUpperHaloX = h5.readAttributeArr<double>("domainUpperHaloX");
        domainUpperHaloY = h5.readAttributeArr<double>("domainUpperHaloY");
        domainUpperHaloZ = h5.readAttributeArr<double>("domainUpperHaloZ");
        domainLowerHaloX = h5.readAttributeArr<double>("domainLowerHaloX");
        domainLowerHaloY = h5.readAttributeArr<double>("domainLowerHaloY");
        domainLowerHaloZ = h5.readAttributeArr<double>("domainLowerHaloZ");

        owner = h5.readAttributeArr<int>("domainOwner");
        h5.closeGroup();

        std::vector<Domain> allDomains;
        allDomains.resize(owner.size());

        for (size_t i = 0; i < owner.size(); ++i) {
            Domain d;
            d.lower.x = domainLowerX[i];
            d.lower.y = domainLowerY[i];
            d.lower.z = domainLowerZ[i];
            d.upper.x = domainUpperX[i];
            d.upper.y = domainUpperY[i];
            d.upper.z = domainUpperZ[i];

            d.lowerHalo.x = domainLowerHaloX[i];
            d.lowerHalo.y = domainLowerHaloY[i];
            d.lowerHalo.z = domainLowerHaloZ[i];
            d.upperHalo.x = domainUpperHaloX[i];
            d.upperHalo.y = domainUpperHaloY[i];
            d.upperHalo.z = domainUpperHaloZ[i];
            d.owner = owner[i];
            allDomains[i] = d;
        }

        return allDomains;
    }

};

#endif //PHELL_H5CHECKPOINT_H
