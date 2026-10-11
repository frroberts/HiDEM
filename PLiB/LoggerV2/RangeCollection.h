//
// Created by Fredrik Robertsen on 7/13/24.
//

#ifndef RANGECOLLECTION_H
#define RANGECOLLECTION_H

#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include <list>
#include <regex>

#include "RangeDurationCollection.h"
#include "RecordedRange.h"
#include "../Util/Util.h"



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
        RankDurationCollection col;
        col.reduceDurations(recorded, maxLevel);

        if (getLocalRank() == 0) {
            col.printTree();
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

#endif //RANGECOLLECTION_H
