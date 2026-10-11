//
// Created by Fredrik Robertsen on 6/10/24.
//

/*
 void HiDEMParticleData::toCSV(std::string path)
{
    auto start = std::chrono::steady_clock::now();

    //std::cout << path << std::endl;
    CSVWriter writer(path);
    iterateOwnedParticles(SerialExec(), [&](size_t pInd, HiDEMParticleData &pData) {
        writer.appendLine(pData.globalKeys[pInd],
                          pData.xCord[pInd], pData.yCord[pInd], pData.zCord[pInd],
                          pData.xTheta[pInd], pData.yTheta[pInd], pData.zTheta[pInd], pData.radius[pInd],
                          pData.owners[pInd], pData.iteratedNeighbours[pInd], pData.iteratedLinks[pInd],
                          pData.iteratedOrder[pInd], pData.type[pInd]);

    }, *this);

    auto stop = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = stop-start;
    //std::cout << "output time: " << elapsed.count() << "s\n";

}

 */

#ifndef PHELL_CSVWRITER_H
#define PHELL_CSVWRITER_H

#include <string>
#include <ManVec.h>

class CsvWriter {

    std::string sep = ", ";
    std::string buffer;
    int maxBuffer = 32*1024;
    std::ofstream file;
    size_t bytesWritten = 0;


    template<typename ...Fields>
    std::string unpackVals(std::tuple<Fields...> outFields, pInd_t pInd) {
        auto t = getAtIndTuple(pInd, outFields);
        return fmt::format("{} \n", fmt::join(t, ", "));
    }

    template<typename ...Names>
    std::string unpackNames(std::tuple<Names...> fieldNames) {
        return fmt::format("{} \n", fmt::join(fieldNames, ", "));
    }

    void appendLine(std::string &line) {
        if(buffer.length() + line.length() >= maxBuffer){
            bytesWritten += buffer.length();
            file.write(buffer.c_str(), buffer.length());
            buffer = "";
        }
        buffer += line;
    }

public:
    CsvWriter() {
    }

    void createFile(std::string path) {
        file = std::ofstream(path, std::ios::trunc);
        buffer = "";
        buffer.reserve(maxBuffer);
    }

    template<class CordT, typename ...Names, typename ...Fields>
    void
    writerParticlesIndexed(ManVec<CordT> &xCords, ManVec<CordT> &yCords, ManVec<CordT> &zCords, ManVec<pInd_t> &indexes,
                      const std::tuple<Names...> &fieldNames, const std::tuple<Fields...> &outFields) {

        auto fullFields = std::tuple_cat(packVecsDataPtrs(xCords, yCords, zCords), outFields);
        auto fullNames = std::tuple_cat(std::make_tuple("xCords", "yCords", "zCords"), fieldNames);
        writeIndexed(indexes, fieldNames, fullFields);
    }

    template<typename ...Names, typename ...Fields>
    void
    writeIndexed(ManVec<pInd_t> &indexes, const std::tuple<Names...> fieldNames, const std::tuple<Fields...> outFields) {
        std::string header = unpackNames(fieldNames);
        appendLine(header);

        const auto start{std::chrono::steady_clock::now()};

        for(const auto pInd: indexes) {
            std::string line = unpackVals(outFields, pInd);
            appendLine(line);
        }
        bytesWritten += buffer.length();
        file.write(buffer.c_str(), buffer.length());


        const auto end{std::chrono::steady_clock::now()};
        const std::chrono::duration<double> elapsed_seconds{end - start};

        //std::cout << bytesWritten/1000.0/1000.0/elapsed_seconds.count() << "MB/s" << std::endl;
    }

    void closeFile() {
        file.close();
    }
};

#endif //PHELL_CSVWRITER_H
