//
// Created by Fredrik Robertsen on 12/22/25.
//

#ifndef PHELL_HCPPARTICLEGENERATOR_H
#define PHELL_HCPPARTICLEGENERATOR_H

#include <random>
#include "Geometry/Map2d.h"

#include "Domain.h"
#include "Util/Util.h"
#include "Logger/Logger.h"
#include "Geometry/InputGeometry.h"
#include "ParticleGenerator.h"

class HCPParticleGenerator  : public ParticleGenerator
{
    Real3 gLow;
    Real3 gHigh;
    Real3 origin;

    double radius;
public:
    //
    virtual void generate(Domain &dom, std::vector<std::shared_ptr<InputGeometry>> geometries,
        const std::vector<Map<double> *> &generateProbabilities, Real3 offset, ManVec<ElementaryParticle> &particles)
    {

        double sqrt3 = sqrt(3.0);
        double sqrt6 = sqrt(6.0);
        double c13 = 1.0/3.0;

        double xSpacing = 2 * radius;
        double ySpacing = sqrt3 * radius;
        double zSpacing = ((2.0 * sqrt6) / 3.0) * radius;

        // calculate an offset to start from to ensure we get a particle at origin
        Real3 originDistance = origin-gLow;

        Real3 originOffset = {0};
        if (originDistance.x != 0)
        {
            originOffset.x = std::fmod(origin.x, xSpacing) ;
        }
        if (originDistance.y != 0)
        {
            originOffset.y = std::fmod(origin.y, ySpacing) ;
        }
        if (originDistance.z != 0)
        {
            originOffset.z = std::fmod(origin.z, zSpacing) ;
        }

        gLow = gLow - originOffset;

        Event geomGenerateEvent;
        geomGenerateEvent.start();



        int self = getLocalRank();


        int lowerXInt = std::max(((std::max(dom.lower.x, gLow.x) - gLow.x) / xSpacing) - 1, 0.0);
        int lowerYInt = std::max(((std::max(dom.lower.y, gLow.y) - gLow.y) / ySpacing) - 1, 0.0);
        int lowerZInt = std::max(((std::max(dom.lower.z, gLow.z) - gLow.z) / zSpacing) - 1, 0.0);
        int upperXInt = ((std::min(gHigh.x, dom.upper.x) - gLow.x) / xSpacing) + 1;
        int upperYInt = ((std::min(gHigh.y, dom.upper.y) - gLow.y) / ySpacing) + 1;
        int upperZInt = ((std::min(gHigh.z, dom.upper.z) - gLow.z) / zSpacing) + 1;

#pragma omp parallel
        {
            // seed this better
            std::random_device r;
            std::mt19937 randomEngine(r());
            // could be float...
            std::uniform_real_distribution<double> uniformDist(0, 1);

            ManVec<ElementaryParticle> particlesLocal;

#pragma omp for
            for (int k = lowerZInt; k < upperZInt; ++k) {
                for (int j = lowerYInt; j < upperYInt; ++j) {
                    for (int i = lowerXInt; i < upperXInt; ++i) {
                        float x = (2 * i + ((j + k) % 2)) * radius;
                        float y = (sqrt3 * (j + (c13 * (k % 2)))) * radius;;
                        float z = (((2.0 * sqrt6) / 3.0) * k) * radius;;

                        this->createParticle({gLow.x + x, gLow.y + y, gLow.z + z}, self, particlesLocal,
                                             dom, geometries, generateProbabilities, offset, radius,
                                             uniformDist(randomEngine));
                    }
                }
            }
#pragma omp critical
            {
                //
                for(const auto &p: particlesLocal){
                    particles.push_back(p);
                }
            };
        }

        this->setParticleKeys(particles);


        geomGenerateEvent.end();
        Logger::writeAtFirst("Particles generated: " + Logger::getStats(particles.size(), "points") + "\n");
        Logger::writeAtFirst("Particle generate time " + Logger::getLimits(geomGenerateEvent.getLastDuration(), "s") + "\n");
    }

    HCPParticleGenerator() = delete;

    HCPParticleGenerator(Real3 globalLow, Real3 globalHigh, double radius, Real3 origin) :
        gLow(globalLow), gHigh(globalHigh), radius(radius), origin(origin) {};
    //void setExtremes(Real3 globalLow, Real3 globalHigh);
};


#endif //PHELL_HCPPARTICLEGENERATOR_H