//
// Created by Fredrik Robertsen on 03/02/2021.
//

#include "Balancer.h"

#include <queue>
#include <mpi.h>
#include <cmath>
#include <random>

#include "../Logger/Logger.h"
#include "../Util/Util.h"

#include <Iterator.h>

#include "../PLiB/Global.h"

#include "AtomicOps.h"


#ifdef GPU
#include <thrust/gather.h>
#include <thrust/execution_policy.h>
#include <thrust/shuffle.h>
#include <thrust/random.h>
#include <thrust/sequence.h>




__global__ void domainReduceLocal(hpReal_t *xCords,hpReal_t *yCords,hpReal_t *zCords, double *weights, size_t maxElem, double *loads, BalancerDomain *domains, int numDomains) {
    //
    __shared__ double domainLoads[128];
    size_t i = blockIdx.x * blockDim.x + threadIdx.x;

    if (threadIdx.x < 128) {
        domainLoads[threadIdx.x] = 0.0;
    }
    __syncthreads();

    if (i >= maxElem) {
        return;
    }

    Real3 cord = {xCords[i], yCords[i], zCords[i]};
    for (int j = 0; j < numDomains; j++) {
        if (domains[j].done) {
            continue;
        }
        if (domains[j].currentDomain.within(cord)) {
            Atomic::atomicAddVal(domainLoads[j], weights[i]);
        }
    }

    __syncthreads();
    for (int j = threadIdx.x; j < numDomains; j+= blockDim.x) {
        Atomic::atomicAddVal(loads[j], domainLoads[j]);

    }
}

void Balancer::getLocalLoadBulk(ManVec<BalancerDomain> &currentDomains) {
    dim3 block = {1024};
    dim3 grid = {1 + (numParticles / block.x)};
    for (size_t i = 0; i < currentDomains.size(); i+= 128) {
        int domainsInIter = currentDomains.size()-i;
        if (domainsInIter > 128) {
            domainsInIter = 128;
        }

        getErrorDev((domainReduceLocal<<<grid, block>>>(xCords->data(), yCords->data(), zCords->data(),weights->data(), xCords->size(),
                                           localLoads.data()+i, currentDomains.data()+i, domainsInIter)));
        Iterator::syncAll();

    }
}
#else
void Balancer::getLocalLoadBulk(ManVec<BalancerDomain> &currentDomains) {
    //
#pragma omp parallel
    {
        std::vector<double> threadLoads(currentDomains.size(), 0.0);
#pragma omp for
        for (int i = 0; i < numParticles; ++i) {
            //
            Real3 cord = {(*xCords)[i], (*yCords)[i], (*zCords)[i]};
            for (int j = 0; j < currentDomains.size(); j++) {
                if (currentDomains[j].done)
                    continue;
                if (currentDomains[j].currentDomain.within(cord)) {
                    threadLoads[j] += (*weights)[i];
                }
            }
        }
#pragma omp critical
        {
            for (int j = 0; j < currentDomains.size(); j++) {
                localLoads[j] += threadLoads[j];
            }
        }
    }
}
#endif


void Balancer::setComm(MPI_Comm newComm) {
    comm = newComm;
}

void Balancer::setBuffers(ManVec<hpReal_t> &xCords, ManVec<hpReal_t> &yCords, ManVec<hpReal_t> &zCords, ManVec<double> &weights)
{
    if (xCords.size() > static_cast<size_t>(samplePoints) && sampled)
    {
        numParticles = samplePoints;

        xCordsSampled.resize(numParticles);
        yCordsSampled.resize(numParticles);
        zCordsSampled.resize(numParticles);
        weightsSampled.resize(numParticles);

        ManVec<pInd_t> indexes;
        indexes.resize(xCords.size());

#ifdef GPU
        thrust::sequence(EXECPOLICY indexes.begin(), indexes.end());
        thrust::default_random_engine rng(123);
        thrust::shuffle(EXECPOLICY indexes.begin(), indexes.end(), rng);

        thrust::gather(EXECPOLICY indexes.begin(), indexes.begin() + numParticles, xCords.begin(), xCordsSampled.begin());
        thrust::gather(EXECPOLICY indexes.begin(), indexes.begin() + numParticles, yCords.begin(), yCordsSampled.begin());
        thrust::gather(EXECPOLICY indexes.begin(), indexes.begin() + numParticles, zCords.begin(), zCordsSampled.begin());
        thrust::gather(EXECPOLICY indexes.begin(), indexes.begin() + numParticles, weights.begin(), weightsSampled.begin());
#else
        std::iota(indexes.begin(), indexes.end(), 0);
        std::mt19937 g(123);

        std::shuffle(indexes.begin(), indexes.end(), g);

        Iterator::iterate(OffloadExec(), [&] DEVCALLABLE (int i) {
            //
            int ind = indexes[i];
            xCordsSampled[i] = xCords[ind];
            yCordsSampled[i] = yCords[ind];
            zCordsSampled[i] = zCords[ind];
            weightsSampled[i] = weights[ind];

        }, numParticles);
#endif

        this->xCords = &xCordsSampled;
        this->yCords = &yCordsSampled;
        this->zCords = &zCordsSampled;
        this->weights = &weightsSampled;

    } else
    {
        this->xCords = &xCords;
        this->yCords = &yCords;
        this->zCords = &zCords;
        this->weights = &weights;
        numParticles = xCords.size();
    }
    sums.resize(numParticles);

}

void Balancer::split(ManVec<BalancerDomain> &currentDomains, ManVec<BalancerDomain> &nextDomains)
{

    auto domainPtr = currentDomains.data();
    for (auto &current: currentDomains)
    {
        current.parentDomains = current.numChildren;
        current.parent = current.currentDomain;
        double lowerBlock = std::floor(current.numChildren / 2.0);

        double ratio = lowerBlock / current.numChildren;

        // double fullLoad = parent.d.load;
        current.targetLoad = current.currentDomain.load * ratio;

        auto size = current.currentDomain.getSize();

        current.dim = SplitDim::Y;
        if (zActive)
        {
            if (size.x > size.y && size.x > size.z)
            {
                current.dim = SplitDim::X;
            }
            else if (size.z > size.x && size.z > size.y)
            {
                current.dim = SplitDim::Z;
            }
        }
        else
        {
            if (size.x > size.y)
            {
                current.dim = SplitDim::X;
            }
        }

        current.domainSize = current.currentDomain.getSize();
        current.parent = current.currentDomain;
        current.numChildren = lowerBlock;
        current.done = false;
    }

    localLoads.resize(currentDomains.size());
    globalLoads.resize(currentDomains.size());
    auto localLoadPtr = localLoads.data();

    for (int iter = 0; iter < maxDepth; iter++) {

        Iterator::iterate(OffloadExec(),[=] DEVCALLABLE (int i) {
            auto &current = domainPtr[i];
            localLoadPtr[i] = 0;

            if (current.done)
                return;

            double sign = 1;

            if (current.currentDomain.load > (current.targetLoad)) {
                // go down in size
                sign = -1;
            } else if (current.currentDomain.load < (current.targetLoad)) {
                // go up in size
                sign = 1;
            }

            Real3 cnst = {0};
            switch (current.dim) {
                case SplitDim::X:
                    cnst.x = sign;
                    break;
                case SplitDim::Y:
                    cnst.y = sign;
                    break;
                case SplitDim::Z:
                    cnst.z = sign;
                    break;
            }
            current.domainSize = current.domainSize / 2.0;
            current.currentDomain.upper = current.currentDomain.upper + (cnst * current.domainSize);

        }, currentDomains.size());
        Iterator::syncAll();

        getLocalLoadBulk(currentDomains);

        Iterator::syncAll();

        MPI_Allreduce(localLoads.data(), globalLoads.data(), currentDomains.size(), MPI_DOUBLE, MPI_SUM, comm);

        auto globalLoadsPtr = globalLoads.data();
        Iterator::iterate(OffloadExec(), [=, this] DEVCALLABLE (int i) {
            auto &current = domainPtr[i];

            if (!current.done)
                current.currentDomain.load = globalLoadsPtr[i];

            if ((current.currentDomain.load <= current.targetLoad + current.targetLoad * tolerance) &&
                (current.currentDomain.load >= current.targetLoad - current.targetLoad * tolerance)) {
                current.done = true;
            }
        }, currentDomains.size());

        Iterator::syncAll();

        bool allDone = ParStd::transform_reduce(EXECPOLICY currentDomains.begin(), currentDomains.end(), true,
                                             []DEVCALLABLE (bool a, bool b) {
                                                 return a && b;
                                             }, [=] DEVCALLABLE (BalancerDomain b) {
                                                 return b.done;
                                             });
        Iterator::syncAll();

        if (allDone)
            break;
        Iterator::syncAll();
    }

    for (auto &current : currentDomains)
    {

        BalancerDomain upperPart;
        upperPart.numChildren = current.parentDomains - current.numChildren;

        upperPart.currentDomain = current.parent;
        switch (current.dim)
        {
        case SplitDim::X:
            upperPart.currentDomain.lower.x = current.currentDomain.upper.x;
            break;
        case SplitDim::Y:
            upperPart.currentDomain.lower.y = current.currentDomain.upper.y;
            break;
        case SplitDim::Z:
            upperPart.currentDomain.lower.z = current.currentDomain.upper.z;
            break;
        }

        upperPart.currentDomain.load = current.parent.load - current.currentDomain.load;


        if (upperPart.numChildren == 1) {
            domains.push_back(upperPart.currentDomain);
        } else {
            nextDomains.push_back(upperPart);
        }
        if (current.numChildren == 1) {
            domains.push_back(current.currentDomain);
        } else {
            nextDomains.push_back(current);
        }
    }
}

std::vector<Domain> Balancer::balance(int numDomains, double halo, Domain bounding) {

    domains.clear();

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

    ManVec<BalancerDomain> currentDomains;

    // no need to do any balancer shenanigans if we only run one domain
    if (numDomains > 1) {
        currentDomains.push_back(BalancerDomain(bounding, numDomains));
    } else {
        domains.push_back(bounding);
    }
    while (!currentDomains.empty())
    {
        ManVec<BalancerDomain> nextDomains;

        split(currentDomains, nextDomains);
        std::swap(nextDomains, currentDomains);
    }

    std::vector<double> localLoads;
    std::vector<double> allLoads;
    allLoads.resize(numDomains*numDomains);
    for (auto &d: domains) {
        localLoads.push_back(d.load);
    }
    // odd behaviour without barrier
    MPI_Barrier(comm);
    MPI_Allgather(localLoads.data(), numDomains, MPI_DOUBLE, allLoads.data(), numDomains, MPI_DOUBLE, comm);


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



