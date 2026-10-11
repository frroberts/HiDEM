//
// Created by Fredrik Robertsen on 03/02/2021.
//

#ifndef PHELL_BALANCER_H
#define PHELL_BALANCER_H

#include <tuple>
#include <queue>
#include <cmath>

#include <mpi.h>

#include <ManVec.h>

#include "../Domain.h"

#include <ParallelReduce.h>
#include "../PLiB/Global.h"



#include "DevPortabilityLayer.h"

enum SplitDim
{
    X, Y, Z
};


struct BalancerDomain
{
    Domain currentDomain;
    int numChildren;
    int parentDomains;
    Domain parent;
    SplitDim dim;
    Real3 domainSize;
    double targetLoad;
    bool done;
};

class Balancer {

public:

    std::string getIndent(int iter)
    {
        std::string indent;
        for (int i = 0; i < iter; ++i) {
            indent+=" ";
        }
        return indent;
    }

    int maxDepth = 32;
    double tolerance = 0.005;
    int samplePoints = 10000000;
    bool sampled = true;
    bool verbose = false;
    ManVec<hpReal_t> *xCords;
    ManVec<hpReal_t> *yCords;
    ManVec<hpReal_t> *zCords;
    ManVec<double> *weights;

    ManVec<hpReal_t> xCordsSampled;
    ManVec<hpReal_t> yCordsSampled;
    ManVec<hpReal_t> zCordsSampled;
    ManVec<double> weightsSampled;

    ManVec<double> sums;
    ManVec<double> localLoads;
    ManVec<double> globalLoads;
    ManVec<int> colors;
    ManVec<int> keys;

    int numParticles;
    std::vector<Domain> domains;
    MPI_Comm comm = MPI_COMM_WORLD;

    bool xActive = true;
    bool yActive = true; 
    bool zActive = true;

    Balancer() {
        //
    };

    void setComm(MPI_Comm newComm);
    void setBuffers(ManVec<hpReal_t> &xCords, ManVec<hpReal_t> &yCords, ManVec<hpReal_t> &zCords, ManVec<double> &weights);

    void getLocalLoadBulk(ManVec<BalancerDomain> &currentDomains);

    void split(ManVec<BalancerDomain> &currentDomains,
               ManVec<BalancerDomain> &nextDomains);//, int *indexPtr, int indexCount);
    std::vector<Domain> balance(int numDomains, double halo, Domain bounding);
};


#endif //PHELL_BALANCER_H
