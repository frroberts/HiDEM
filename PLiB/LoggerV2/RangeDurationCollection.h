//
// Created by Fredrik Robertsen on 5/4/26.
//

#ifndef PHELL_RANGE_DURATIONCOLLECTION_H
#define PHELL_RANGE_DURATIONCOLLECTION_H

#include <chrono>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>

#include <mpi.h>
#include <fmt/format.h>

#include <numeric>

#include "../Util/Util.h"
#include "RecordedRange.h"

class RankDurationCollection {

    struct EventNode {
        std::string name;
        std::map<std::string, EventNode*> children;
        double minVal, maxVal, mean, stddev, median, p25, p75, total;
        int minRank, maxRank, numRanks, missingRanks;

        void print(const std::string &prefix, bool last = true) {
            if (!name.empty()) {

                std::cout << fmt::format(
                    "{:<36} {:>9.4g}s {:>9.4g}s @{:<3} {:>9.4g}s @{:<3} {:>9.4g}s {:>9.4g}s {:>9.4g}s {:>5} \n",
                    prefix+(last ? "└─ " : "├─ ")+name,
                    mean,
                    minVal, minRank,
                    maxVal, maxRank, p25, median, p75, numRanks);
            }

            std::string childPrefix = std::string(prefix) + (name.empty() ? "" : (last ? "  " : "│ "));

            auto it = children.begin();
            while (it != children.end()) {
                bool isLast = (std::next(it) == children.end());
                it->second->print(childPrefix, isLast);
                ++it;
            }
        }
    };

    struct RankDurationCommStruct {
        char name[512];
        double duration;
        int rank;

        RankDurationCommStruct(const std::string &name, double duration) {
            // todo check string copy logic
            int strSize = name.length();
            if (strSize >= 511) {
                strSize = 511;
            }
            strncpy(this->name, name.c_str(), strSize);
            this->name[strSize] = '\0';
            this->duration = duration;
            MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        }
        RankDurationCommStruct() = default;
    };

    std::unordered_map<std::string, std::vector<RankDurationCommStruct>> collectedRanges;
    std::unordered_map<std::string, EventNode> treeNodes;

    std::vector<std::string> splitPath(const std::string &s, const char delim) {
        std::vector<std::string> parts;
        size_t start = 0;
        size_t pos;
        while ((pos = s.find(delim, start)) != std::string::npos) {
            parts.push_back(s.substr(start, pos - start));
            start = pos + 1;
        }
        parts.push_back(s.substr(start));
        return parts;
    }

public:
    RankDurationCollection() {
    }

    void printTree() {
        std::cout << fmt::format("{:<36} {:>9} {:>9} {:5} {:>9} {:5} {:>9} {:>9} {:>9} {:5}\n",
                     "name", "average", "min", "rank", "max", "rank", "p25", "median", "p75", "ranks");
        std::string base = "";
        treeNodes[""].print(base);
    }

    void reduceDurations(std::unordered_map<std::string, RecordedRange> &recorded, int maxLevel)
    {
        std::vector<RankDurationCommStruct> ranges;

        for (auto r: recorded) {
            if(r.second.level <= maxLevel) {
                ranges.push_back({r.first,r.second.cumDuration.count()});
            }
        }
        std::sort(ranges.begin(), ranges.end(), [](auto &a, auto &b) {
            return std::string(a.name) < std::string(b.name);
        });

        int self;
        MPI_Comm_rank(MPI_COMM_WORLD, &self);
        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

        std::vector<int> countsPerRank;
        countsPerRank.resize(worldSize);
        int ownSize = ranges.size();

        MPI_Gather(&ownSize, 1, MPI_INT, countsPerRank.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

        int totalCont = std::reduce(countsPerRank.begin(), countsPerRank.end(), 0, std::plus<int>());
        std::vector<int> bytesPerRank;
        std::transform(countsPerRank.begin(), countsPerRank.end(), std::back_inserter(bytesPerRank),
                       [](int val) {
                           return val * sizeof(RankDurationCommStruct);
                       });

        std::vector<int> displacements;
        std::exclusive_scan(bytesPerRank.begin(), bytesPerRank.end(), std::back_inserter(displacements), 0, std::plus<int>());

        std::vector<RankDurationCommStruct> allRanges;
        allRanges.resize(totalCont);

        MPI_Gatherv(ranges.data(), ranges.size()*sizeof(RankDurationCommStruct), MPI_BYTE, allRanges.data(),
                    bytesPerRank.data(), displacements.data(), MPI_BYTE, 0, MPI_COMM_WORLD);

        for (const auto &r: allRanges) {
            collectedRanges[r.name].push_back(r);
        }

        treeNodes.clear();

        for (auto& [path, entries] : collectedRanges) {

            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                return a.duration < b.duration;
            });

            const int n = (int)entries.size();
            auto percentile = [&](double p) {
                return entries[std::clamp((int)std::ceil(p * n) - 1, 0, n - 1)].duration;
            };

            double total = std::accumulate(entries.begin(), entries.end(), 0.0,
                               [](double acc, const auto& e) { return acc + e.duration; });
            double mean = total / n;
            double var  = std::accumulate(entries.begin(), entries.end(), 0.0,
                               [&](double acc, const auto& e) { return acc + (e.duration - mean) * (e.duration - mean); });

            treeNodes[path] = EventNode{
                .name        = splitPath(path, '|').back(),
                .minVal         = entries.front().duration,
                .maxVal         = entries.back().duration,
                .mean        = mean,
                .stddev      = n > 1 ? std::sqrt(var / (n - 1)) : 0.0,
                .median      = n % 2 ? entries[n/2].duration : (entries[n/2-1].duration + entries[n/2].duration) / 2.0,
                .p25         = percentile(0.25),
                .p75         = percentile(0.75),
                .total       = total,
                .minRank     = entries.front().rank,
                .maxRank     = entries.back().rank,
                .numRanks    = n,
                .missingRanks = worldSize - n,
            };
        }

        // hashmap not changed after this, pointers are static
        // build the tree based on tree nodes
        for (auto p: treeNodes) {
            if (p.first == "")
                continue;
            auto parts = splitPath(p.first, '|');
            std::string currentPath = "";
            EventNode* cur = &treeNodes[currentPath];
            for (const auto &part : parts) {
                if (!currentPath.empty())
                    currentPath += "|";
                currentPath += part;
                auto it = cur->children.find(part);
                if (it == cur->children.end()) {
                    cur->children.insert(std::make_pair(part, &treeNodes[currentPath]));
                }
                cur = cur->children.find(part)->second;
            }
        }
    }
};

#endif //PHELL_RANGE_DURATIONCOLLECTION_H