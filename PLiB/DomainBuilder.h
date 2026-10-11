//
// Created by Fredrik Robertsen on 04/04/2022.
//

#ifndef PHELL_DOMAINBUILDER_H
#define PHELL_DOMAINBUILDER_H

#include <random>

#include <ManVec.h>

#include "Util/Real3.h"
#include "World.h"
#include "Logger/Logger.h"
#include "Geometry/InputGeometry.h"
#include "Balancer/Balancer.h"

//how many times halo scale should be added as a buffer on the outside of the outside domains
#define BBSCALE 10

class DomainBuilder {
public:

    static void generateDomainsSampled(std::shared_ptr<InputGeometry> geom, double haloSize, int numSamples, double tolerance,
                                       Domain &bounding, World &world) {
        //
        world.domains.resize(world.size);
        std::vector<Domain> generatedDomains;

        std::mt19937 gen(1); // Standard mersenne_twister_engine seeded with rd()
        std::uniform_real_distribution<> xDis(bounding.lower.x, bounding.upper.x);
        std::uniform_real_distribution<> yDis(bounding.lower.y, bounding.upper.y);
        std::uniform_real_distribution<> zDis(bounding.lower.z, bounding.upper.z);

        if (world.self == 0) {

            ManVec<double> xCord;
            ManVec<double> yCord;
            ManVec<double> zCord;
            ManVec<double> weights;

            while(weights.size() < numSamples){
            // for (int i = 0; i < numSamples; ++i) {
                Real3 pos{xDis(gen), yDis(gen), zDis(gen)};

                if (geom->withinGeometry(pos, 0)) {
                    xCord.push_back(pos.x);
                    yCord.push_back(pos.y);
                    zCord.push_back(pos.z);
                    weights.push_back(1);
                }
            }

            bounding.lower.x += -(haloSize*BBSCALE);
            bounding.lower.y += -(haloSize*BBSCALE);
            bounding.lower.z += -(haloSize*BBSCALE);

            bounding.upper.x += (haloSize*BBSCALE);
            bounding.upper.y += (haloSize*BBSCALE);
            bounding.upper.z += (haloSize*BBSCALE);

            bounding.setHalo(haloSize);
            world.domains.resize(world.size);
            //
            Balancer geomBalancer;
            geomBalancer.zActive = false;
            geomBalancer.tolerance = tolerance;
            geomBalancer.verbose = false;
            geomBalancer.setComm(MPI_COMM_SELF);
            geomBalancer.setBuffers(xCord, yCord, zCord, weights);
            world.domains = geomBalancer.balance(world.size, haloSize, bounding);
        }

        MPI_Bcast(world.domains.data(), sizeof(Domain) * world.size, MPI_BYTE, 0, MPI_COMM_WORLD);
    }


    static void generateBalancedDomains(ManVec<double> &xCord, ManVec<double> &yCord, ManVec<double> &zCord, ManVec<double> &weights,
        double haloSize, double tolerance, Domain bounding, World &world) {
        //
        RangeV2 r("domain generate", 3);

        Logger::writeAtFirst("re-balancing \n");

        /*
        double xMax = std::numeric_limits<double>::min();
        double yMax = std::numeric_limits<double>::min();
        double zMax = std::numeric_limits<double>::min();
        double xMin = std::numeric_limits<double>::max();
        double yMin = std::numeric_limits<double>::max();
        double zMin = std::numeric_limits<double>::max();

        ParallelReduce<double> reducer;
        reducer.minMax(xMin, xMax, xCord);
        reducer.minMax(yMin, yMax, yCord);
        reducer.minMax(zMin, zMax, zCord);
        */

        auto [xMin, xMax] = stx::minmax_element(EXECPOLICY xCord.begin(), xCord.end());
        auto [yMin, yMax] = stx::minmax_element(EXECPOLICY yCord.begin(), yCord.end());
        auto [zMin, zMax] = stx::minmax_element(EXECPOLICY zCord.begin(), zCord.end());

        double xMax_local = *xMax;
        double yMax_local = *yMax;
        double zMax_local = *zMax;
        double xMin_local = *xMin;
        double yMin_local = *yMin;
        double zMin_local = *zMin;

        double xMax_global;
        double yMax_global;
        double zMax_global;
        double xMin_global;
        double yMin_global;
        double zMin_global;
        MPI_Allreduce(&xMax_local, &xMax_global, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        MPI_Allreduce(&yMax_local, &yMax_global, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        MPI_Allreduce(&zMax_local, &zMax_global, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);

        MPI_Allreduce(&xMin_local, &xMin_global, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
        MPI_Allreduce(&yMin_local, &yMin_global, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
        MPI_Allreduce(&zMin_local, &zMin_global, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);

        bounding.lower = {xMin_global, yMin_global, zMin_global};
        bounding.upper = {xMax_global, yMax_global, zMax_global};

        bounding.lower.x += -(haloSize*BBSCALE);
        bounding.lower.y += -(haloSize*BBSCALE);
        bounding.lower.z += -(haloSize*BBSCALE);

        bounding.upper.x += (haloSize*BBSCALE);
        bounding.upper.y += (haloSize*BBSCALE);
        bounding.upper.z += (haloSize*BBSCALE);

        Balancer geomBalancer;
        geomBalancer.zActive = false;
        geomBalancer.tolerance = tolerance;
        geomBalancer.verbose = false;
        geomBalancer.setBuffers(xCord, yCord, zCord, weights);
        world.domains = geomBalancer.balance(world.size, haloSize, bounding);
    }

};


#endif //PHELL_DOMAINBUILDER_H
