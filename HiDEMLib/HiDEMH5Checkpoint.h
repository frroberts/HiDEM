//
// Created by Fredrik Robertsen on 03/04/2022.
//

#ifndef PHELL_HIDEMH5CHECKPOINT_H
#define PHELL_HIDEMH5CHECKPOINT_H


#include <Domain.h>
#include <H5IO/H5Checkpoint.h>

#include "HiDEMLinkData.h"
#include "HiDEMParticleData.h"

class HiDEMH5Checkpoint : public H5Checkpoint {

public:


    HiDEMH5Checkpoint() {


    }

    ~HiDEMH5Checkpoint() {
    }


    void writeCheckpoint(int timestep, Domain bounding, HiDEMParticleData &pData, HiDEMLinkData &lData,
                         bool timestepInFile, std::string workingPathStr, std::string checkpointDirectory);

    void writeParticleDataTup(HiDEMParticleData &pData);

    void readParticleDataTup(HiDEMParticleData &pData);

    void writeLinkData(HiDEMLinkData &lData);

    void readLinkData(HiDEMLinkData &lDaata);
};


#endif //PHELL_HIDEMH5CHECKPOINT_H
