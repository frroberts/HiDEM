//
// Created by Fredrik Robertsen on 03/04/2022.
//

#ifndef PHELL_HIDEMH5CHECKPOINT_H
#define PHELL_HIDEMH5CHECKPOINT_H


#include <Domain.h>
#include <H5IO/H5Checkpoint.h>
#include <Util/Util.h>
#include <filesystem>

class HiDEMH5Checkpoint : public H5Checkpoint {
public:
    HiDEMH5Checkpoint() {
    }

    ~HiDEMH5Checkpoint() {
    }


    template <typename PDataT, typename LDataT>
    void writeCheckpoint(int timestep, Domain bounding, PDataT &pData,
                                            LDataT &lData,
                                            bool timestepInFile, bool keepOld, std::string workingPathStr,
                                            std::string checkpointDirectory) {
        MPI_Barrier(MPI_COMM_WORLD);
        std::string suffixStr = "";
        if (timestepInFile) {
            suffixStr = "_" + std::to_string(timestep);
        }

        std::filesystem::path targetPath = workingPathStr + "/" + checkpointDirectory + "/" + "checkpoint" + suffixStr + ".h5";
        if (std::filesystem::exists(targetPath) && keepOld) {
            std::filesystem::rename(targetPath, workingPathStr + "/" + checkpointDirectory + "/" + "checkpoint" + suffixStr + "_old.h5");
        }

        MPI_Barrier(MPI_COMM_WORLD);

        newCheckpooint(workingPathStr + "/" + checkpointDirectory + "/" + "checkpoint" + suffixStr + ".h5");
        writeTimestep(timestep);
        writeDomains(bounding);

        pData.checkpointData(h5);
        lData.checkpointData(h5);

        h5.closeFile();
    }

};


#endif //PHELL_HIDEMH5CHECKPOINT_H
