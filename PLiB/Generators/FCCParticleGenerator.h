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

#include "Util/Mat4.h"


class FCCParticleGenerator : public ParticleGenerator  {
    Real3 gLow;
    Real3 gHigh;
    Real3 origin;

    double spacing;
    double radius;
    Real3 rotation = {0,0,0};

    ManVec<Real3> createUnitCell(float unitCellSize, Real3 offset = {0}) {

        ManVec<Real3> unitCell;


        int lowerXInt = 0;
        int lowerYInt = 0;
        int lowerZInt = 0;
        int upperXInt = unitCellSize / spacing; // does this need a +1 ?
        int upperYInt = unitCellSize / spacing; // does this need a +1 ?
        int upperZInt = unitCellSize / spacing; // does this need a +1 ?

        lpReal_t cellSize = spacing;
        lpReal_t halfCell = cellSize / 2.f;

        for (int k = lowerZInt; k < upperZInt; ++k) {
            for (int j = lowerYInt; j < upperYInt; ++j) {
                for (int i = lowerXInt; i < upperXInt; ++i) {
                    Real3 c0{(i * spacing), (j * spacing), (k * spacing)};
                    Real3 c1{(i * spacing), (j * spacing) + halfCell, (k * spacing) + halfCell};
                    Real3 c2{(i * spacing) + halfCell, (j * spacing), (k * spacing) + halfCell};
                    Real3 c3{(i * spacing) + halfCell, (j * spacing) + halfCell, (k * spacing)};

                    c0 = c0 + offset;
                    c1 = c1 + offset;
                    c2 = c2 + offset;
                    c3 = c3 + offset;

                    unitCell.push_back(c0);
                    unitCell.push_back(c1);
                    unitCell.push_back(c2);
                    unitCell.push_back(c3);

                }
            }
        }

        return unitCell;
    }

public:

    virtual void generate(Domain &dom, std::vector<std::shared_ptr<InputGeometry>> geometries, const std::vector<Map<double> *> &generateProbabilities,

        Real3 offset, ManVec<ElementaryParticle> &particles) {
        Event geomGenerateEvent;
        geomGenerateEvent.start();

        lpReal_t cellSize = spacing;

        lpReal_t unitCellSize = cellSize*10;

        Real3 originDistance = origin-gLow;
        Real3 originOffset = {0};
        if (originDistance.x != 0)
        {
            originOffset.x = std::fmod(origin.x, spacing) ;
        }
        if (originDistance.y != 0)
        {
            originOffset.y = std::fmod(origin.y, spacing) ;
        }
        if (originDistance.z != 0)
        {
            originOffset.z = std::fmod(origin.z, spacing) ;
        }

        ManVec<Real3> unitCell = createUnitCell(unitCellSize, originOffset);

        Real3 localLower{
            std::max(dom.lower.x, gLow.x),
            std::max(dom.lower.y, gLow.y),
            std::max(dom.lower.z, gLow.z)};
        Real3 localUpper{
            std::min(gHigh.x, dom.upper.x),
            std::min(gHigh.y, dom.upper.y),
            std::min(gHigh.z, dom.upper.z)};

        std::vector<Real3> corners = {
            {localLower.x, localLower.y, localLower.z},
            {localUpper.x, localLower.y, localLower.z},
            {localLower.x, localUpper.y, localLower.z},
            {localUpper.x, localUpper.y, localLower.z},
            {localLower.x, localLower.y, localUpper.z},
            {localUpper.x, localLower.y, localUpper.z},
            {localLower.x, localUpper.y, localUpper.z},
            {localUpper.x, localUpper.y, localUpper.z}
        };

        int self = getLocalRank();

        Mat4 geometryMat;

        geometryMat.fromEulerAnglesXYZ(rotation.x, rotation.y, rotation.z);

        auto invGeometry = geometryMat.toInvert();

        for (auto &v: corners) {
            v = invGeometry.mulReal3(v);
        }
        auto geometryLowerT = corners[0];
        auto geometryUpperT = corners[0];

        for (auto v: corners) {
            geometryUpperT = {std::max(geometryUpperT.x, v.x), std::max(geometryUpperT.y, v.y), std::max(geometryUpperT.z, v.z)};;//glm::max(geometryUpperT, v);
            geometryLowerT = {std::min(geometryLowerT.x, v.x), std::min(geometryLowerT.y, v.y), std::min(geometryLowerT.z, v.z)};
        }

        Domain localLimits;
        localLimits.lower = Real3(localLower.x, localLower.y, localLower.z);
        localLimits.upper = Real3(localUpper.x, localUpper.y, localUpper.z);
        localLimits.lower = localLimits.lower - Real3{1*unitCellSize};
        localLimits.upper = localLimits.upper + Real3{1*unitCellSize};

        int lowerXInt = std::floor((geometryLowerT.x) / unitCellSize);
        int lowerYInt = std::floor((geometryLowerT.y) / unitCellSize);
        int lowerZInt = std::floor((geometryLowerT.z) / unitCellSize);
        int upperXInt = std::ceil((geometryUpperT.x) / unitCellSize);
        int upperYInt = std::ceil((geometryUpperT.y) / unitCellSize);
        int upperZInt = std::ceil((geometryUpperT.z) / unitCellSize);

        ManVec<Real3> cellLowers;

        for (int k = lowerZInt; k < upperZInt; ++k) {
            for (int j = lowerYInt; j < upperYInt; ++j) {
                for (int i = lowerXInt; i < upperXInt; ++i) {
                    Real3 cellLower{i * unitCellSize, j * unitCellSize, k * unitCellSize};
                    Real3 cellUpper{(i + 1) * unitCellSize, (j + 1) * unitCellSize, (k + 1) * unitCellSize};
                    cellLower = geometryMat.mulReal3(cellLower);
                    cellUpper = geometryMat.mulReal3(cellUpper);

                    if (!localLimits.within({cellLower.x, cellLower.y, cellLower.z}) &&
                        !localLimits.within({cellUpper.x, cellUpper.y, cellUpper.z})) {
                        continue;
                    }
                    cellLowers.push_back(Real3{i * unitCellSize, j * unitCellSize, k * unitCellSize});
                }
            }
        }

#pragma omp parallel
        {
            // seed this better
            std::random_device r;
            std::mt19937 randomEngine(r());
            // could be float...
            std::uniform_real_distribution<double> uniformDist(0, 1);

            ManVec<ElementaryParticle> particlesLocal;

#pragma omp for
            for (const auto &cellLower: cellLowers) {
                Real3 cellUpper = cellLower + Real3{unitCellSize}; // {(i+1)*unitCellSize,(j+1)*unitCellSize,(k+1)*unitCellSize};
                Real3 cellLowerT = geometryMat.mulReal3(cellLower);
                Real3 cellUpperT = geometryMat.mulReal3(cellUpper);

                if (!localLimits.within({cellLowerT.x, cellLowerT.y, cellLowerT.z}) &&
                    !localLimits.within({cellUpperT.x, cellUpperT.y, cellUpperT.z})) {
                    continue;
                }

                for (const auto &c: unitCell) {
                    Real3 c0{c.x, c.y, c.z};
                    c0 = c0 + cellLower;
                    c0 = geometryMat.mulReal3(c0);

                    this->createParticle(Real3{c0.x, c0.y, c0.z},
                                         self, particlesLocal, dom, geometries,
                                         generateProbabilities, offset, radius, uniformDist(randomEngine));
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

    FCCParticleGenerator(Real3 globalLow, Real3 globalHigh, double spacing, double radius, Real3 origin, Real3 rotation) :
        gLow(globalLow), gHigh(globalHigh), spacing(spacing), radius(radius), origin(origin), rotation(rotation) {};
    //void setExtremes(Real3 globalLow, Real3 globalHigh);
};


#endif //PHELL_FCCPARTICLEGENERATOR_H
