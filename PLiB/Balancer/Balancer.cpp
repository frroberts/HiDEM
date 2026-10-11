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

Domain Balancer::getMidpoint(Domain bounding, SplitDim splitDir, Real3 prevSize, double target, double current, int iter) {
    //  logic of what size we use is wrong
    double sign = 1;

    if ((current <= target + target * tolerance) && (current >= target - target * tolerance)) {
        if (verbose)
            Logger::writeAtFirst( getIndent(iter) + std::to_string(current) + " load within tolerance returning..\n");
        return bounding;
    }

    if (iter > maxDepth) {
        if (verbose)
            Logger::writeAtFirst( getIndent(iter) + "at max iter giving up \n");
        return bounding;
    }

    if (current > (target)) {
        // go down in size
        sign = -1;
        if (verbose)
            Logger::writeAtFirst( getIndent(iter) + std::to_string(current) + " down in size ");

    } else if (current < (target)) {
        // go up in size
        sign = 1;
        if (verbose)
            Logger::writeAtFirst( getIndent(iter) + std::to_string(current) + " up in size ");
    }

    Real3 size = prevSize;

    switch (splitDir) {
        case SplitDim::X:
            size.x /= 2;
            if (verbose)
                Logger::writeAtFirst( "X old upper " + std::to_string(bounding.upper.x));
            bounding.upper.x += sign * size.x;
            if (verbose)
                Logger::writeAtFirst( " new upper " + std::to_string(bounding.upper.x) + "\n");
            break;
        case SplitDim::Y:
            size.y /= 2;
            if (verbose)
                Logger::writeAtFirst( "Y old upper " + std::to_string(bounding.upper.y));
            bounding.upper.y += sign * size.y;
            if (verbose)
                Logger::writeAtFirst( " new upper " + std::to_string(bounding.upper.y) + "\n");
            break;
        case SplitDim::Z:
            size.z /= 2;
            if (verbose)
                Logger::writeAtFirst( "Z old upper " + std::to_string(bounding.upper.z));
            bounding.upper.z += sign * size.z;
            if (verbose)
                Logger::writeAtFirst( " new upper " + std::to_string(bounding.upper.z) + "\n");
            break;
    }

    double next = getDomainLoad(bounding, numParticles);
    //std::cout << target << " " << next << " " << current << std::endl;
    //if(abs(next-target) > abs(current-target))
    //    return bounding;
    return getMidpoint(bounding, splitDir, size, target, next, ++iter);

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



void Balancer::split(Domain parent, int numDomains) {
    if (numDomains == 1) {
        domains.push_back(parent);
        return;
    }

    double lowerBlock = std::floor(numDomains / 2.0);
    double upperBlock = std::ceil(numDomains / 2.0);
    double ratio = lowerBlock / numDomains;

    int64_t globalLoad = getDomainLoad(parent, numParticles);
    int64_t halfLoad = globalLoad * ratio;

    auto lowerPartX = getMidpoint(parent, SplitDim::X, parent.getSize(), halfLoad, getDomainLoad(parent, numParticles));
    auto upperPartX = getUpper(parent, lowerPartX, SplitDim::X);

    auto lowerPartY = getMidpoint(parent, SplitDim::Y, parent.getSize(), halfLoad, getDomainLoad(parent, numParticles));
    auto upperPartY = getUpper(parent, lowerPartY, SplitDim::Y);

    auto lowerPartZ = getMidpoint(parent, SplitDim::Z, parent.getSize(), halfLoad, getDomainLoad(parent, numParticles));
    auto upperPartZ = getUpper(parent, lowerPartZ, SplitDim::Z);

    double xSurf = lowerPartX.getSurface() + upperPartX.getSurface();
    double ySurf = lowerPartY.getSurface() + upperPartY.getSurface();
    double zSurf = lowerPartZ.getSurface() + upperPartZ.getSurface();

    Domain upperPart;
    Domain lowerPart;

    if(xSurf < ySurf && xSurf < zSurf){
        upperPart = upperPartX;
        lowerPart = lowerPartX;
    } else if(zSurf < xSurf && zSurf < ySurf && zActive){
        upperPart = upperPartZ;
        lowerPart = lowerPartZ;
    } else { // (ySurf < xSurf && ySurf < zSurf)
        upperPart = upperPartY;
        lowerPart = lowerPartY;
    }

    split(lowerPart, static_cast<int>(lowerBlock));
    split(upperPart, static_cast<int>(upperBlock));
}

std::vector<Domain> Balancer::balance(int numDomains, double halo, Domain bounding) {
    //
    numParticles = xCords->size();

    split(bounding, numDomains);

    std::vector<double> localLoads;
    std::vector<double> allLoads;
    allLoads.resize(numDomains*numDomains);
    for (auto &d: domains) {
        localLoads.push_back(getDomainLocalLoad(d, numParticles));
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
        double load = getDomainLoad(d, numParticles);
        maxLoad = std::max(maxLoad, load);
        minLoad = std::min(minLoad, load);
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
        d.load = load;
    }

    /*
    if(getLocalRank() == 0) {
        for (int i = 0; i < numDomains; ++i) {
            for (int j = 0; j < numDomains; ++j) {
                if(domains[i].owner == j)
                    std::cout << "[";
                std::cout << allLoads[(i * numDomains) + j];
                if(domains[i].owner == j)
                    std::cout << "]";
                std::cout << " ";
            }
            std::cout << std::endl;
        }

        for (auto d: domains) {
            std::cout << "newPoint([" << d.upper.x << "," << d.upper.y << "," << d.upper.z << "],"
                      << "[" << d.lower.x << "," << d.lower.y << "," << d.lower.z << "]," << d.owner << ")"
                      << std::endl;

            std::cout << "newPoint([" << d.upperHalo.x << "," << d.upperHalo.y << "," << d.upperHalo.z << "],"
                      << "[" << d.lowerHalo.x << "," << d.lowerHalo.y << "," << d.lowerHalo.z << "]," << d.owner << ")"
                      << std::endl;
        }
    }
     */

    //Logger::writeAtFirst("Domain loads : "+std::to_string(loads)+"\n");
    Logger::writeAtFirst("balance quality: "+std::to_string(maxLoad / minLoad)+" \n");
//    Logger::writeAtFirst(fmt::format("Domain loads : {}\n", loads));
//    Logger::writeAtFirst(fmt::format("balance quality: {} \n", maxLoad / minLoad));
    return domains;
}

int64_t Balancer::getDomainLoad(Domain &dom, int numParticles)  {
    int64_t localLoad = 0;
    int64_t globalLoad = 0;

    localLoad = getDomainLocalLoad(dom, numParticles);

    /*
#pragma omp parallel for reduction(+:localLoad)
    for (int i = 0; i < numParticles; ++i) {
        if (dom.within({(*xCords)[i], (*yCords)[i], (*zCords)[i]})) {
            localLoad += (*weights)[i];
        }
    }
     */
    MPI_Allreduce(&localLoad, &globalLoad, 1, MPI_INT64_T, MPI_SUM, comm);
    return globalLoad;
}

int64_t Balancer::getDomainLocalLoad(const Domain &dom, int numParticles) {
    int64_t localLoad = 0;
    double *xPtr = xCords->ptr;
    double *yPtr = yCords->ptr;
    double *zPtr = zCords->ptr;
    localLoad = reducer.reduce<int>(numParticles, []DEVCALLABLE(int a, int b) { return a + b; }, [=]DEVCALLABLE(int i) {
        double x = xPtr[i];
        double y = yPtr[i];
        double z = zPtr[i];
        bool xIn;
        if (dom.lower.x == dom.upper.x && x == dom.lower.x)
            xIn = true;
        else
            xIn = (x >= dom.lower.x && x < dom.upper.x);

        bool yIn;
        if (dom.lower.y == dom.upper.y && y == dom.lower.y)
            yIn = true;
        else
            yIn = (y >= dom.lower.y && y < dom.upper.y);

        bool zIn;
        if (dom.lower.z == dom.upper.z && z == dom.lower.z)
            zIn = true;
        else
            zIn = (z >= dom.lower.z && z < dom.upper.z);

        bool within = xIn && yIn && zIn;

        if (within) {
            return 1;
        }
        return 0;
    });
    return localLoad;
}




