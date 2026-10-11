//
// Created by Fredrik Robertsen on 7/13/24.
//

#ifndef RANGECOLLECTION_H
#define RANGECOLLECTION_H


//
// Created by Fredrik Robertsen on 6/30/24.
//

#ifndef LOGGERV3_H
#define LOGGERV3_H
#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>

#include <mpi.h>
#include <fmt/format.h>

#include <list>
#include <regex>


class RecordedRange {
    //
public:
    std::chrono::duration<double> cumDuration = std::chrono::duration<double>::zero();
    std::chrono::duration<double> minDuration = std::chrono::duration<double>::max();
    std::chrono::duration<double> maxDuration = std::chrono::duration<double>::min();
    int count = 0;
    int level;
    int createdOrder;
};


class RangeCollection {
    static thread_local std::vector<std::string> pathStack;
    static std::mutex writeMtx;
    // for active ranges
    // include range stack

    //static std::unordered_map<>
    static std::unordered_map<std::string, RecordedRange> recorded;

public:
    static std::string getPath() {
        //if(pathStack.empty())
        // pathStack.push_back(std::to_string(gettid()));
        //pathStack.push_back("");
        std::string fullPath = "";
        for (const auto &e: pathStack) {
            if (!fullPath.empty())
                fullPath += +"|";
            fullPath += e;
        }
        if (!fullPath.empty())
            fullPath += "|";
        return fullPath;
    }

    static void pushPath(std::string &s) {
        pathStack.push_back(s);
    }

    static void popPath() {
        pathStack.pop_back();
    }

    static void zeroHistory() {
        recorded.clear();
    }

    static void recordEvent(std::string path, std::chrono::duration<double> duration, int level) {
        writeMtx.lock();
        if (recorded.find(path) == recorded.end()) {
            auto newRange = RecordedRange();
            newRange.createdOrder = recorded.size();
            recorded[path] = newRange;
        }
        recorded[path].count++;
        recorded[path].cumDuration += duration;
        recorded[path].maxDuration = std::max(recorded[path].maxDuration, duration);
        recorded[path].minDuration = std::min(recorded[path].minDuration, duration);
        recorded[path].level = level;
        writeMtx.unlock();
    }


    static void print(int maxLevel = 1) {
        // assumes all ranks take the same ranges
        int numRanks;
        MPI_Comm_size(MPI_COMM_WORLD, &numRanks);
        std::vector<std::string> paths;
        for (auto r: recorded) {
            if(r.second.level <= maxLevel)
                paths.push_back(r.first);
        }
        std::sort(paths.begin(), paths.end());

        int self;
        self = 0;
        MPI_Comm_rank(MPI_COMM_WORLD, &self);

        if(self == 0)
            std::cout << fmt::format("{:<10} {:<32} {:>10}  {:>9} {:5} {:>9} {:5} {:>9} {:5} {:>9} {:5}\n",
                                 "", "name", "average", "min", "rank", "max", "rank", "minMin", "rank", "maxMax",
                                 "rank");
/*
        int prevIndent = 0;
        std::string prevPath = "";
        std::string nextPath = "";
        std::string nextPathBase = "";
        std::list<std::string> base;
*/

        // take paths, create string views of the splits, start from the right, build each row
        std::vector<std::vector<std::string_view >> split;
        for (int i = 0; i < paths.size(); ++i) {
            std::vector<std::string_view > splitPaths;
            
        }
        for (int i = 0; i < paths.size(); ++i) {
            auto p = paths[i];
            int lastSplit = p.find_last_of('|');
            const auto last = p.substr(lastSplit + 1);
            const auto currentPath = p.substr(0, lastSplit);
            int indentLevel = 0;
            for(const auto &c: p) {
                if (c == '|')
                    indentLevel++;
            }

            /*
                      if (i < paths.size() - 1) {
                          auto p = paths[i+1];
                          nextPath = p.substr(0, p.find_last_of('|') );
                          nextPathBase = nextPath.substr(0, nextPath.find_last_of('|') );
                      }

              */
            std::string tabStr = "";
            for (int j = 0; j < indentLevel; ++j) {
                tabStr += "-";
            }
/*
            //tabStr += " " + p+ " ";
            for(auto t: base){
                tabStr += t;
            }

            if(prevPath == currentPath && nextPath == currentPath){
                tabStr += "├─";
            } else if(prevPath != currentPath && nextPath == currentPath){
                tabStr += "├┐";
                base.push_back("│");
            } else if(prevPath == currentPath && nextPath != currentPath && currentPath == nextPathBase) {
                tabStr += "└┬";
                base.push_back(" ");
            } else if(prevPath == currentPath && nextPath != currentPath) {
                tabStr += "└─";
                while(base.back() != std::string("│")){
                    base.pop_back();
                }
            } else if(prevPath != currentPath && nextPath != currentPath && i == paths.size() - 1){
                tabStr += "└─";
                while(base.back() != std::string("│")){
                    base.pop_back();
                }
            } else if(prevPath != currentPath && nextPath != currentPath){
                tabStr += "├─";
            }
*/

            // prevPath = currentPath;

            auto val = recorded[p];

            int myRank;
            int worldSize;
            MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
            MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
            std::vector<RecordedRange> data;
            data.resize(worldSize);
            MPI_Gather(&val, sizeof(RecordedRange), MPI_BYTE, data.data(), sizeof(RecordedRange), MPI_BYTE, 0,
                       MPI_COMM_WORLD);

            if (self == 0) {
                auto sumWall = std::chrono::duration<double>::zero();
                auto minWall = std::chrono::duration<double>::max();
                int minWallAt = 0;
                auto maxWall = std::chrono::duration<double>::min();
                int maxWallAt = 0;

                auto minMin = std::chrono::duration<double>::max();
                int minMinAt = 0;
                auto maxMax = std::chrono::duration<double>::min();
                int maxMaxAt = 0;

                int i = 0;
                int countSum = 0;
                for (auto r: data) {
                    countSum += r.count;
                    sumWall += r.cumDuration;
                    if (r.cumDuration < minWall) {
                        minWall = r.cumDuration;
                            minWallAt = i;
                    }
                    if (r.cumDuration > maxWall) {
                        maxWall = r.cumDuration;
                        maxWallAt = i;
                    }
                    if (r.minDuration < minMin) {
                        minMin = r.minDuration;
                        minMinAt = i;
                    }
                    if (r.maxDuration > maxMax) {
                        maxMax = r.maxDuration;
                        maxMaxAt = i;
                    }
                    i++;
                }

                std::cout << fmt::format(
                    "{: <10} {:<32} {:>9.4g}s {:>9.4g}s @{:<3} {:>9.4g}s @{:<3} {:>9.4g}s @{:<3} {:>9.4g}s @{:<3}\n",
                    tabStr, last, sumWall.count()/(double)numRanks, minWall.count(), minWallAt,
                    maxWall.count(), maxWallAt, minMin.count(), minMinAt, maxMax.count(), maxMaxAt);
            }
        }
        if (self == 0) {
            std::cout << std::flush << std::endl;
        }
    }
};


class RangeV2 {
public:
    std::string path;
    std::chrono::time_point<std::chrono::steady_clock> start;
    bool active = true;
    int level = 1;

    RangeV2(std::string name, int logLevel = 1) {
        path = RangeCollection::getPath() + name;
        start = std::chrono::steady_clock::now();
        level = logLevel;
        RangeCollection::pushPath(name);
    }

    ~RangeV2() {
        if (active) {
            end();
        }
    }

    void end() {
        std::chrono::duration<double> duration = std::chrono::steady_clock::now() - start;
        RangeCollection::recordEvent(path, duration, level);
        RangeCollection::popPath();
        active = false;
    }
};

#endif //LOGGERV3_H



#endif //RANGECOLLECTION_H
