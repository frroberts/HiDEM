//
// Created by Fredrik Robertsen on 24/10/2020.
//

#ifndef PHELL_FCCPARTICLEGENERATOR_H
#define PHELL_FCCPARTICLEGENERATOR_H

#include <random>
#include "Geometry/Map2d.h"

#include "Domain.h"
#include "Util/Util.h"
#include "Logger/Logger.h"
#include "Geometry/InputGeometry.h"
#include "ParticleGenerator.h"


class FCCParticleGenerator : public ParticleGenerator  {
    Real3 gLow;
    Real3 gHigh;

    double spacing;
    double radius;

    gKey_t getKeyXYZ(int x, int y, int z, int nXPoints, int nYPoints, int nZPoints ) {
        // offset by 100, so i can sanity check keys as LUMI seems to generate 0 keys
        gKey_t key = x + (y * nXPoints) + (z * nXPoints * nYPoints) + 100;
        //key = temp;
        return key;
    }

public:

    virtual void
    generate(Domain &dom, std::vector<std::shared_ptr<InputGeometry>> geometries, const std::vector<Map<double> *> &generateProbabilities, Real3 offset,
             ManVec<ElementaryParticle> &particles) {
        Event geomGenerateEvent;
        geomGenerateEvent.start();
        int nXPoints = ceil((gHigh.x - gLow.x) / spacing);
        int nYPoints = ceil((gHigh.y - gLow.y) / spacing);
        int nZPoints = ceil((gHigh.z - gLow.z) / spacing);
        int self = getLocalRank();

        double halfSpacing = spacing / 2.0;


        int lowerXInt = std::max(((std::max(dom.lower.x, gLow.x) - gLow.x) / spacing) - 1, 0.0);
        int lowerYInt = std::max(((std::max(dom.lower.y, gLow.y) - gLow.y) / spacing) - 1, 0.0);
        int lowerZInt = std::max(((std::max(dom.lower.z, gLow.z) - gLow.z) / spacing) - 1, 0.0);
        int upperXInt = ((std::min(gHigh.x, dom.upper.x) - gLow.x) / spacing) + 1;
        int upperYInt = ((std::min(gHigh.y, dom.upper.y) - gLow.y) / spacing) + 1;
        int upperZInt = ((std::min(gHigh.z, dom.upper.z) - gLow.z) / spacing) + 1;

#pragma omp parallel
        {
            // seed this better
            std::random_device r;
            std::mt19937 randomEngine(r());
            std::uniform_real_distribution<double> uniformDist(0, 1);

            std::vector<ElementaryParticle> particlesLocal;

#pragma omp for
            for (int k = lowerZInt; k < upperZInt; ++k) {
                for (int j = lowerYInt; j < upperYInt; ++j) {
                    for (int i = lowerXInt; i < upperXInt; ++i) {
                        this->createParticle({gLow.x + (i * spacing), gLow.y + (j * spacing), gLow.z + (k * spacing)}, self,
                                       particlesLocal, dom, geometries,
                                       generateProbabilities, offset, radius, uniformDist(randomEngine));
                        this->createParticle({gLow.x + (i * spacing), gLow.y + (j * spacing) + halfSpacing,
                                        gLow.z + (k * spacing) + halfSpacing}, self, particlesLocal, dom, geometries,
                                       generateProbabilities, offset, radius, uniformDist(randomEngine));
                        this->createParticle({gLow.x + (i * spacing) + halfSpacing, gLow.y + (j * spacing),
                                        gLow.z + (k * spacing) + halfSpacing}, self, particlesLocal, dom, geometries,
                                       generateProbabilities, offset, radius, uniformDist(randomEngine));
                        this->createParticle({gLow.x + (i * spacing) + halfSpacing, gLow.y + (j * spacing) + halfSpacing,
                                        gLow.z + (k * spacing)}, self, particlesLocal, dom, geometries,
                                       generateProbabilities, offset, radius, uniformDist(randomEngine));
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

    FCCParticleGenerator() = delete;

    FCCParticleGenerator(Real3 globalLow, Real3 globalHigh, double spacing, double radius) : gLow(globalLow), gHigh(globalHigh), spacing(spacing), radius(radius) {};
    //void setExtremes(Real3 globalLow, Real3 globalHigh);
};


#endif //PHELL_FCCPARTICLEGENERATOR_H
