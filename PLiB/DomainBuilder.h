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

class DomainBuilder {
public:

#define maxBB 1000

    /*
    static void generateDomains(int nx, int ny, double haloSize, Domain &bounding, World &world) {
        if (world.size != (nx * ny)) {
            Logger::writeAtFirst("number of static domains (nx*ny) != number of ranks\n");
            exit(0);
        }

        Logger::writeAtFirst("generating " + std::to_string(world.size) + " number of non balanced domains\n");

        Real3 size = bounding.upper - bounding.lower;
        size.x /= nx;
        size.y /= ny;

        int i = 0;
        std::vector<std::vector<Domain>> domains;
        for (int x = 0; x < nx; ++x) {
            domains.push_back(std::vector<Domain>());
            for (int y = 0; y < ny; ++y) {
                //
                Domain d;
                d.lower.x = bounding.lower.x + (size.x * x);
                d.lower.y = bounding.lower.y + (size.y * y);
                d.lower.z = bounding.lower.z;

                d.upper.x = d.lower.x + (size.x);
                d.upper.y = d.lower.y + (size.y);
                d.upper.z = d.lower.z + (size.z);


                // set box to some large number if at the edges
                if (x == 0)
                    d.lower.x += -maxBB;
                if (y == 0)
                    d.lower.y += -maxBB;

                if (x == nx - 1)
                    d.upper.x += maxBB;
                if (y == ny - 1)
                    d.upper.y += maxBB;

                d.lowerHalo = d.lower - Real3{haloSize};
                d.upperHalo = d.upper + Real3{haloSize};
                d.owner = i;
                domains.back().push_back(d);
                i++;
            }
        }
        for (int x = 0; x < nx; ++x) {
            for (int y = 0; y < ny; ++y) {
                //domains[x][y].print();
                world.domains.push_back(domains[x][y]);
            }
        }
    }

    static void generateDomainsGeom(Map2d<double> bed, Map2d<double> surface, double haloSize,
                                    double resolution, double tolerance, Domain &bounding, World &world) {
        //
        world.domains.resize(world.size);
        std::vector<Domain> generatedDomains;
        if (world.self == 0) {

            ManVec<double> xCord;
            ManVec<double> yCord;
            ManVec<double> zCord;
            ManVec<double> weights;

            Real3 size = bounding.upper - bounding.lower;
            Real3 incr = size / resolution;

            for (double x = bounding.lower.x; x < bounding.upper.x; x += incr.x) {
                for (double y = bounding.lower.y; y < bounding.upper.y; y += incr.y) {
                    //
                    double diff = surface.interp(x, y) - bed.interp(x, y);
                    xCord.push_back(x);
                    yCord.push_back(y);
                    zCord.push_back(bounding.lower.z);
                    weights.push_back(diff);
                }
            }

            bounding.lower.x += -maxBB;
            bounding.lower.y += -maxBB;
            bounding.lower.z += -maxBB;

            bounding.upper.x += maxBB;
            bounding.upper.y += maxBB;
            bounding.upper.z += maxBB;

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
     */

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

            bounding.lower.x += -maxBB;
            bounding.lower.y += -maxBB;
            bounding.lower.z += -maxBB;

            bounding.upper.x += maxBB;
            bounding.upper.y += maxBB;
            bounding.upper.z += maxBB;

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


    static void generateBalancedDomains(ManVec<double> &xCord, ManVec<double> &yCord, ManVec<double> &zCord, ManVec<double> &weights, double haloSize, double tolerance, Domain &bounding, World &world) {
        //
        RangeV2 r("domain generate", 3);

        Logger::writeAtFirst("re-balancing \n");

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


        double xMax_global;
        double yMax_global;
        double zMax_global;
        double xMin_global;
        double yMin_global;
        double zMin_global;
        MPI_Allreduce(&xMax, &xMax_global, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        MPI_Allreduce(&yMax, &yMax_global, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        MPI_Allreduce(&zMax, &zMax_global, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);

        MPI_Allreduce(&xMin, &xMin_global, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
        MPI_Allreduce(&yMin, &yMin_global, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
        MPI_Allreduce(&zMin, &zMin_global, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);

        bounding.lower = {xMin_global, yMin_global, zMin_global};
        bounding.upper = {xMax_global, yMax_global, zMax_global};

        bounding.lower.x += -10.0;
        bounding.lower.y += -10.0;
        bounding.lower.z += -10.0;

        bounding.upper.x += 10.0;
        bounding.upper.y += 10.0;
        bounding.upper.z += 10.0;

        Balancer geomBalancer;
        geomBalancer.zActive = false;
        geomBalancer.tolerance = tolerance;
        geomBalancer.verbose = false;
        geomBalancer.setBuffers(xCord, yCord, zCord, weights);
        world.domains = geomBalancer.balance(world.size, haloSize, bounding);
    }

};


#endif //PHELL_DOMAINBUILDER_H
