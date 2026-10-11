//
// Created by Fredrik Robertsen on 10/9/23.
//

#ifndef PHELL_PARTICLEGENERATOR_H
#define PHELL_PARTICLEGENERATOR_H

#include <ManVec.h>
#include <mpi.h>
#include <Util/Util.h>
#include "ElementaryParticle.h"
#include <Domain.h>
#include <Geometry/InputGeometry.h>
#include <Geometry/Map.h>

class ParticleGenerator {
public:
    virtual void
    generate(Domain &dom, std::vector<std::shared_ptr<InputGeometry>> geometries, const std::vector<Map<double> *>&generateProbabilities, Real3 offset,
             ManVec<ElementaryParticle>&particles) = 0;

    void setParticleKeys(ManVec<ElementaryParticle>&particles) {
        //
        int localCount = particles.size();
        int localOffset = 0;

        MPI_Exscan(&localCount, &localOffset, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        // std::cout << getLocalRank() << " " << localOffset << std::endl;;

        gKey_t k = 0;
        for (auto&p: particles) {
            p.globalKey = k + localOffset;
            k++;
        }
    }

    void createParticle(Real3 cord, int owner, ManVec<ElementaryParticle>&particles,
                        Domain&dom, const std::vector<std::shared_ptr<InputGeometry>>&geometries,
                        const std::vector<Map<double> *>&generateProbabilities, Real3 offset, double radius, double prob = 1) {
        int geomType = 0;
        // get new random probability here
        for (const auto &geom: geometries) {
            if (geom->withinGeometry(cord, radius) && dom.within(cord)) {
                double denAtCord = generateProbabilities[geomType]->getVal(cord.x, cord.y, cord.z);
                // if generated probability is less than ice generate probability for the current type then skip it
                if (denAtCord >= prob) {
                    // what is the operator here
                    ElementaryParticle tp;
                    tp.initConstruct(cord, owner, radius);
                    tp.type = geomType;
                    particles.push_back(tp);
                    return;
                }
            }
            geomType++;
        }
    }
};

#endif //PHELL_PARTICLEGENERATOR_H
