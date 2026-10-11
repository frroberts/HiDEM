
// Created by Fredrik Robertsen on 25/10/2020.
//

#include "Map2dLoader.h"
#include "../Logger/Logger.h"


size_t Map2dLoader::readInput(std::string path) {
    //

    Event loadEvent;
    loadEvent.start();
    int fieldCount;

    xMin = std::numeric_limits<double>::max();
    xMax = std::numeric_limits<double>::min();
    yMin = std::numeric_limits<double>::max();
    yMax = std::numeric_limits<double>::min();

    std::unordered_map<double, int> rowCounts;

    std::ifstream inFile;
    inFile.open(path);
    std::string line;

    std::regex splitExpr = std::regex(R"([^\s]+)");
    auto start = std::chrono::steady_clock::now();
    size_t readLines = 0;
    if (inFile.is_open()) {
        getline(inFile, line);
        auto numColBeg =
            std::sregex_iterator(line.begin(), line.end(), splitExpr);
        auto words_end = std::sregex_iterator();
        fieldCount = std::distance(numColBeg, words_end);

        columns.resize(fieldCount);

        inFile.seekg(std::ios::beg);

        while (getline(inFile, line)) {
            int column = 0;

            splitStringDelims(line, " \t", [&](std::string element) {
                if (element.length() == 0) {
                    return;
                }
                columns[column].push_back(std::stof(element));
                column++;
            });
            if (column != fieldCount) {
                std::cout << "Malformed input, on row " << readLines << std::endl;
                exit(1);
            }

            xMin = std::min(xMin, columns[0].back());
            xMax = std::max(xMax, columns[0].back());
            yMin = std::min(yMin, columns[1].back());
            yMax = std::max(yMax, columns[1].back());

            if (rowCounts.find(columns[1].back()) == rowCounts.end()) {
                rowCounts[columns[1].back()] = 0;
            }
            rowCounts[columns[1].back()]++;

            readLines++;
        }
    } else {
        std::cout << "input file open failed" << std::endl;
        return -1;
    }
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    // std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";

    yPoints = rowCounts.size();
    xPoints = rowCounts.begin()->second;

    for (const auto &k: rowCounts) {
        if (k.second != xPoints) {
            std::cout << "loaded geometry invalid " << k.first << " line to short " << xPoints << " " << k.second << std::endl;
            exit(1);
        }
    }
    loadEvent.end();
    Logger::writeAtFirst("Map2d Loaded: " + std::to_string(readLines) + " points loaded");
    Logger::writeAtFirst("Map2d load time" + Logger::getLimits(loadEvent.getLastDuration(), "s") + "\n");
    return readLines;
}

