//
// Created by Fredrik Robertsen on 22/02/2021.
//

#ifndef PHELL_LOGGER_H
#define PHELL_LOGGER_H

#include <string>
#include <chrono>
#include <mpi.h>

#include <mpi.h>
#include <string>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <numeric>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <iostream>
#include <fstream>
#include <numeric>

class Range{
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    std::chrono::time_point<std::chrono::high_resolution_clock> end;
    int tid;
};


class Logger
{
private:
    /* data */
    int myRank;
    int worldSize;
    std::chrono::time_point<std::chrono::high_resolution_clock> syncTime;

public:

    void init()
    {
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
    }

    void syncClock()
    {
        //
        MPI_Barrier(MPI_COMM_WORLD);
        syncTime = std::chrono::high_resolution_clock::now();
    }

    void writeAt(int rank, std::string message)
    {
        //
    }

    static void writeAtFirst(std::string message, bool sync = false)
    {
        //
        if(sync)
            MPI_Barrier(MPI_COMM_WORLD);
        int myRank;
        MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
        if(myRank == 0) {
            //
            std::cout << fmt::format("{}", message) << std::flush;
        }
    }

    void write(std::string message)
    {
        //
        auto time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = time-Logger::syncTime;
        std::cout << diff.count() << " | " << message << std::endl;
    }

    template <typename T>
    static T getMaxValWorld(T val)
    {
        int myRank;
        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
        std::vector<T> data;
        data.resize(worldSize);
        MPI_Gather(&val, 1*sizeof(val), MPI_BYTE, data.data(), 1*sizeof(val), MPI_BYTE, 0, MPI_COMM_WORLD);

        auto max = std::max_element(data.begin(), data.end());
        return *max;
    }

    template <typename T>
    static T getMinValWorld(T val)
    {
        int myRank;
        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
        std::vector<T> data;
        data.resize(worldSize);
        MPI_Gather(&val, 1*sizeof(val), MPI_BYTE, data.data(), 1*sizeof(val), MPI_BYTE, 0, MPI_COMM_WORLD);

        auto min = std::min_element(data.begin(), data.end());
        return *min;
    }

    template <typename T>
    static std::string getLimits(T val, std::string unit)
    {
        int myRank;
        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
        std::vector<T> data;
        data.resize(worldSize);
        MPI_Gather(&val, 1*sizeof(val), MPI_BYTE, data.data(), 1*sizeof(val), MPI_BYTE, 0, MPI_COMM_WORLD);

        if(myRank == 0) {
            auto max = std::max_element(data.begin(), data.end());
            auto min = std::min_element(data.begin(), data.end());
            return fmt::format("max: {} {} @ rank: {} | min: {} {} @ rank: {}",
                                     *max, unit, std::distance(data.begin(), max), *min, unit, std::distance(data.begin(), min));
        }
        return "";
    }
    template <typename T>
    static std::string getStats(T val, std::string unit)
    {
        int myRank;
        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
        std::vector<T> data;
        data.resize(worldSize);
        MPI_Gather(&val, 1*sizeof(val), MPI_BYTE, data.data(), 1*sizeof(val), MPI_BYTE, 0, MPI_COMM_WORLD);

        if(myRank == 0) {
            auto max = std::max_element(data.begin(), data.end());
            T maxVal = *max;
            int maxPos = std::distance(data.begin(), max);
            auto min = std::min_element(data.begin(), data.end());
            T minVal = *min;
            int minPos = std::distance(data.begin(), min);
            auto sum = std::accumulate(data.begin(), data.end(), T(0));
            std::nth_element(data.begin(), data.begin() + data.size()/2, data.end());
            return fmt::format("max: {} {} @ rank: {} | min: {} {} @ rank: {} | sum : {} | mean : {} | median : {}" ,
                                     maxVal, unit, maxPos, minVal, unit, minPos, sum, (sum)/worldSize, data[data.size()/2]);
        }
        return "";
    }

    static auto reduceStats(double val) {

        int myRank;
        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
        std::vector<double> data;
        data.resize(worldSize);
        MPI_Gather(&val, 1, MPI_DOUBLE, data.data(), 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

            auto max = std::max_element(data.begin(), data.end());
            double maxVal = *max;
            int maxPos = std::distance(data.begin(), max);
            auto min = std::min_element(data.begin(), data.end());
            double minVal = *min;
            int minPos = std::distance(data.begin(), min);
            double sum = std::accumulate(data.begin(), data.end(), 0.0);
            std::nth_element(data.begin(), data.begin() + data.size()/2, data.end());
            return std::make_tuple(maxVal, maxPos, minVal, minPos, sum, (sum)/worldSize, data[data.size()/2]);
    }

    static std::string getStats(double val, std::string unit)
    {
        int myRank;
        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        MPI_Comm_rank(MPI_COMM_WORLD, &myRank);

        auto [maxVal, maxPos, minVal, minPos, sum, mean, median] = reduceStats(val);

        if(myRank == 0) {
            return fmt::format("max: {:.4f} {} @ rank: {} | min: {:.4f} {} @ rank: {} | sum : {:.4f} | mean : {:.4f} | median : {:.4f}" ,
                                     maxVal, unit, maxPos, minVal, unit, minPos, sum, mean, median);
        }
        return "";
    }

    template <typename T>
    static std::string getSum(T val, std::string unit)
    {
        int myRank;
        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
        std::vector<T> data;
        data.resize(worldSize);
        MPI_Gather(&val, 1*sizeof(val), MPI_BYTE, data.data(), 1*sizeof(val), MPI_BYTE, 0, MPI_COMM_WORLD);

        if(myRank == 0) {
            auto sum = std::accumulate(data.begin(), data.end(), 0.0);
            return fmt::format("sum: {} ", sum);
        }
        return "";
    }

    template <typename T>
    static std::string getValues(T val, std::string unit)
    {
        int myRank;
        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
        std::vector<T> data;
        data.resize(worldSize);
        MPI_Gather(&val, 1*sizeof(val), MPI_BYTE, data.data(), 1*sizeof(val), MPI_BYTE, 0, MPI_COMM_WORLD);

        if(myRank == 0) {
            return fmt::format("values: {} {}", data, unit);
        }
        return "";
    }

    template <typename T>
    static void writeHist(T val)
    {
        T max;
        T min;
        T average;

    }

    static void timedBarrier(std::string name, MPI_Comm comm = MPI_COMM_WORLD)
    {
        //
        auto start = std::chrono::steady_clock::now();
        MPI_Barrier(comm);
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end-start;
        double selfCount = elapsed_seconds.count();
        Logger::writeAtFirst(name+" "+ Logger::getLimits(selfCount, "s") + "\n");
        //std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";
    }
};



class Event{
    //
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> endTime;
    std::string name;
    std::chrono::duration<double> accumulative;
    std::vector<std::chrono::duration<double>> durations;
public:
    Event() {
        //
        accumulative = std::chrono::duration<double>::zero();
    }
    void start() {
        startTime = std::chrono::high_resolution_clock::now();
    }
    void end() {
        //
        endTime = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration<double>(endTime-startTime);
        accumulative += dur;
        durations.push_back(dur);
    }
    void zeroAccumulative()
    {
        accumulative = std::chrono::duration<double>::zero();
    }
    void clearHistory()
    {
        durations.clear();
    }
    auto getLimitVals(){
        auto max = std::max_element(durations.begin(), durations.end());
        auto min = std::min_element(durations.begin(), durations.end());
        return std::make_tuple(min, max);
    }
    std::string getLimits(){
        //
        return  "";
    }
    void getHistorgram(int numBuckets = 10){
        auto [min, max] = getLimitVals();
        double bucketSize = (max->count() - min->count())/static_cast<double>(numBuckets);
        std::vector<int> buckets;
        buckets.resize(numBuckets+1);
        for(auto dur: durations)
        {
            double val = dur.count();
            val -= min->count();
            val = val/bucketSize;
            buckets[static_cast<int>(val)]++;
        }
        //
        std::string firstLine;
        std::string secondLine;
        for (int i = 0; i < numBuckets+1; ++i) {
            firstLine += fmt::format("{:.{}f} \t",min->count() + (i*bucketSize), 2);
            secondLine += fmt::format("{} \t",buckets[i]);
        }
        std::cout << firstLine << "\n" << secondLine << std::endl;
    }

    void historyToFile(std::string path, std::string name, MPI_Comm comm = MPI_COMM_WORLD){
        //
        std::ofstream file;
        file.open(path, std::ios::out | std::ios::app);
        for(auto d: durations)
        {
            std::string line = fmt::format("{}, {} \n", name, d.count() );
            file.write(line.c_str(), line.length());
        }
        file.close();
        clearHistory();
    }

    double getLastDuration() {
        double duration = std::chrono::duration<double>(endTime-startTime).count();
        return duration;
    }
};


#endif //PHELL_LOGGER_H
