//
// Created by Fredrik Robertsen on 03/02/2021.
//

#include "Balancer.h"

#include <tuple>
#include <queue>
#include <mpi.h>
#include <cmath>
#include <random>
//#include <fmt/core.h>
//#include <fmt/ranges.h>

#include "../Logger/Logger.h"
#include "../Util/Util.h"

#include <Iterator.h>
#include <ThrustHelper.h>


Domain Balancer::getMidpoint(int *indexPtr, int indexCount, Domain bounding, SplitDim splitDir, Real3 prevSize,
    double target, double current, int iter) {
    //  logic of what size we use is wrong

    double sign = 1;

    if ((current <= target + target * tolerance) && (current >= target - target * tolerance)) {
        if (verbose)
            Logger::writeAtFirst( getIndent(iter) + std::to_string(current) + " load within tolerance returning, "+std::to_string(iter)+" steps\n");
        bounding.load = current;
        return bounding;
    }

    if (iter > maxDepth) {
        if (verbose)
            Logger::writeAtFirst( getIndent(iter) + "at max iter giving up \n");
        bounding.load = current;
        return bounding;
    }

    if (current > (target)) {
        // go down in size
        sign = -1;
        if (verbose)
            Logger::writeAtFirst( getIndent(iter) + " target " + std::to_string(target) + " " +  std::to_string(current) + " down in size ");

    } else if (current < (target)) {
        // go up in size
        sign = 1;
        if (verbose)
            Logger::writeAtFirst( getIndent(iter) + " target " + std::to_string(target) + " " +  std::to_string(current) + " up in size ");
    }

    Real3 size = prevSize;

    switch (splitDir) {
        case SplitDim::X:
            size.x /= 2.0;
            if (verbose)
                Logger::writeAtFirst( "X old upper " + std::to_string(bounding.upper.x));
            bounding.upper.x += sign * size.x;
            if (verbose)
                Logger::writeAtFirst( " new upper " + std::to_string(bounding.upper.x) + "\n");
            break;
        case SplitDim::Y:
            size.y /= 2.0;
            if (verbose)
                Logger::writeAtFirst( "Y old upper " + std::to_string(bounding.upper.y));
            bounding.upper.y += sign * size.y;
            if (verbose)
                Logger::writeAtFirst( " new upper " + std::to_string(bounding.upper.y) + "\n");
            break;
        case SplitDim::Z:
            size.z /= 2.0;
            if (verbose)
                Logger::writeAtFirst( "Z old upper " + std::to_string(bounding.upper.z));
            bounding.upper.z += sign * size.z;
            if (verbose)
                Logger::writeAtFirst( " new upper " + std::to_string(bounding.upper.z) + "\n");
            break;
    }

    double next = getDomainLoad(bounding, indexPtr, indexCount);

    return getMidpoint(indexPtr, indexCount, bounding, splitDir, size, target, next, ++iter);
}


void Balancer::setComm(MPI_Comm newComm) {
    comm = newComm;
}

void Balancer::setBuffers(ManVec<double> &xCords, ManVec<double> &yCords, ManVec<double> &zCords, ManVec<double> &weights) {
    this->xCords = &xCords;
    this->yCords = &yCords;
    this->zCords = &zCords;
    this->weights = &weights;
    numParticles = xCords.size();
}

Domain Balancer::getUpper(Domain parent, Domain lower, SplitDim splitDir) {
    //
    Domain upperPart = parent;
    switch (splitDir) {
        case SplitDim::X:
            upperPart.lower.x = lower.upper.x;
            break;
        case SplitDim::Y:
            upperPart.lower.y = lower.upper.y;
            break;
        case SplitDim::Z:
            upperPart.lower.z = lower.upper.z;
            break;
    }
    return upperPart;
}



void Balancer::split(Domain parent, int numDomains, int *indexPtr, int indexCount) {
    if (numDomains == 1) {
        domains.push_back(parent);
        return;
    }

    double lowerBlock = std::floor(numDomains / 2.0);
    double upperBlock = std::ceil(numDomains / 2.0);
    double ratio = lowerBlock / numDomains;

    int64_t globalLoad = parent.load;
    int64_t halfLoad = globalLoad * ratio;

    auto size = parent.getSize();

    SplitDim splitDir = SplitDim::Y;
    if (zActive)
    {
        if (size.x > size.y && size.x > size.z)
        {
            splitDir = SplitDim::X;
        }
        else if (size.z > size.x && size.z > size.y)
        {
            splitDir = SplitDim::Z;
        }
    } else
    {
        if (size.x > size.y)
        {
            splitDir = SplitDim::X;
        }
    }

    Domain lowerPart = getMidpoint(indexPtr, indexCount, parent, splitDir, parent.getSize(), halfLoad, globalLoad);
    Domain upperPart = getUpper(parent, lowerPart, splitDir);
    upperPart.load = globalLoad - lowerPart.load;

    double *xPtr = xCords->data();
    double *yPtr = yCords->data();
    double *zPtr = zCords->data();

    auto point = stx::stable_partition( EXECPOLICY
        indexPtr, indexPtr+indexCount,
        [=] DEVCALLABLE (int i)
        {
            double x = xPtr[i];
            double y = yPtr[i];
            double z = zPtr[i];

            bool in = lowerPart.within(Real3{x, y, z});
            return in;
        });
    int splitCount = point-indexPtr;

    split(lowerPart, static_cast<int>(lowerBlock), indexPtr, splitCount);
    split(upperPart, static_cast<int>(upperBlock), indexPtr+splitCount, indexCount-splitCount);
}

std::vector<Domain> Balancer::balance(int numDomains, double halo, Domain bounding) {
    //
    // possible optimization here would be to shuffle the coordinates and weights over ranks so that lower down there would be more GPUs active
    numParticles = xCords->size();
    double localLoad = stx::reduce(EXECPOLICY weights->begin(), weights->end(), 0, std::plus<double>());
    MPI_Allreduce(&localLoad, &bounding.load, 1, MPI_DOUBLE, MPI_SUM, comm);
    // bounding.load = numParticles;
    ManVec<int> indexes;
    indexes.resize(numParticles);
    int *indexPtr = indexes.data();
    Iterator::iterate(OffloadExec(), [=] DEVCALLABLE (int i) {
        indexPtr[i] = i;
    }, numParticles);
    Iterator::syncAll();

    split(bounding, numDomains, indexes.data(), indexes.size());

    std::vector<double> localLoads;
    std::vector<double> allLoads;
    allLoads.resize(numDomains*numDomains);
    for (auto &d: domains) {
        localLoads.push_back(d.load);
    }
    // odd behaviour without barrier
    MPI_Barrier(comm);
    MPI_Allgather(&localLoads[0], numDomains, MPI_DOUBLE, &allLoads[0], numDomains, MPI_DOUBLE, comm);


    double maxLoad = std::numeric_limits<double>::lowest();
    double minLoad = std::numeric_limits<double>::max();
    std::vector<int> remainingOwners;
    for (int j = 0; j < numDomains; ++j) {
        remainingOwners.push_back(j);
    }

    int i = 0;
    for (auto &d: domains) {
        d.setHalo(halo);
        maxLoad = std::max(maxLoad, d.load);
        minLoad = std::min(minLoad, d.load);
        int bestOwner = -1;
        double bestCount = -1;
        for(const auto &avail: remainingOwners)
        {
            if (bestCount < allLoads[(avail*numDomains)+i]){
                bestOwner = avail;
                bestCount = allLoads[(avail*numDomains)+i];
            }
        }
        d.owner = bestOwner;
        // d.owner = i;
        remainingOwners.erase(std::remove_if(remainingOwners.begin(),
                                             remainingOwners.end(),
                                             [&](auto x) { return x == bestOwner; }), remainingOwners.end());
        i++;
    }

    Logger::writeAtFirst("balance quality: "+std::to_string(maxLoad / minLoad)+" \n");

    return domains;
}

double Balancer::getDomainLoad(Domain &dom, int *indexPtr, int indexCount)  {
    double localLoad = 0;
    double globalLoad = 0;

    localLoad = getDomainLocalLoad(dom, indexPtr, indexCount);

    MPI_Allreduce(&localLoad, &globalLoad, 1, MPI_DOUBLE, MPI_SUM, comm);
    return globalLoad;
}

double Balancer::getDomainLocalLoad(Domain &dom, int *indexPtr, int indexCount) {
    double localLoad = 0;
    double *xPtr = xCords->data();
    double *yPtr = yCords->data();
    double *zPtr = zCords->data();
    double *wPtr = weights->data();

    // std::transform_reduce

    localLoad = ParStd::transform_reduce(EXECPOLICY indexPtr, indexPtr + indexCount, 0, [] DEVCALLABLE (int a, int b) -> int
        {return a+b;}, [=] DEVCALLABLE  (int ind) -> int
    {
        double x = xPtr[ind];
        double y = yPtr[ind];
        double z = zPtr[ind];

        return dom.within(Real3{x,y,z}) * wPtr[ind]; // should incorporate weight
    });

    return localLoad;
}




