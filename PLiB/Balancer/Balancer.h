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

class Balancer {
public:

    enum SplitDim
    {
        X, Y, Z
    };

    Domain getMidpoint(int *indexPtr, int indexCount, Domain bounding, SplitDim splitDir, Real3 prevSize,
        double target, double current, int iter = 0);

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
    bool verbose = false;
    ManVec<double> *xCords;
    ManVec<double> *yCords;
    ManVec<double> *zCords;
    ManVec<double> *weights;
    int numParticles;
    std::vector<Domain> domains;
    MPI_Comm comm = MPI_COMM_WORLD;

    bool xActive = true;
    bool yActive = true;
    bool zActive = true;

    Balancer() {
        //
    };

    Domain getUpper(Domain parent, Domain lower, SplitDim splitDir);

    void setComm(MPI_Comm newComm);
    void setBuffers(ManVec<double> &xCords, ManVec<double> &yCords, ManVec<double> &zCords, ManVec<double> &weights);
    void split(Domain parent, int numDomains, int *indexPtr, int indexCount);
    std::vector<Domain> balance(int numDomains, double halo, Domain bounding);
    double getDomainLoad(Domain &dom, int *indexPtr, int indexCount) ;

    double getDomainLocalLoad(Domain &dom, int *indexPtr, int indexCount);
};


#endif //PHELL_BALANCER_H
