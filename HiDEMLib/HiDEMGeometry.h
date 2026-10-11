//
// Created by Fredrik Robertsen on 7/7/24.
//

#ifndef HIDEMGEOMETRY_H
#define HIDEMGEOMETRY_H
#include <toml/value.hpp>

#include "Generators/ParticleGenerator.h"
#include "Geometry/GeometryCollection.h"
#include "Geometry/InputGeometry.h"
#include "Geometry/Map.h"
#include "Util/ArgParser.h"

#include <Generators/ElementaryParticle.h>
#include <Generators/ElementrayLink.h>

#include "Generators/LinkGenerator.h"

class HiDEMGeometry {
public:
    Real3 lowerLimit;
    Real3 upperLimit;

    lpReal_t maxRadius;
    Real3 offset = 0;
    bool zeroCenter = false;

    std::shared_ptr<ParticleGenerator> pGenPtr;
    LinkGenerator lGen;

    int breakLinkSeed;
    int breakLinkSeedOffset;
    double linkGenScale;

    std::vector<Map<double> *> particleGenerateProbabilities;
    std::vector<std::vector<int> > linkToTypes;
    std::vector<Map<double> *> linkGenerateProbabilities;

    std::vector<std::string> boundingGeometryStrings;

    std::shared_ptr<InputGeometry> fullGeometry;
    std::vector<std::shared_ptr<InputGeometry> > boundingGeometries;
    std::vector<std::shared_ptr<InputGeometry> > collisionGeometries;

    GeometryCollection geomCollection;

    ManVec<ElementaryParticle> generateParticles(Domain ownDomain);

    ManVec<ElementaryLink> generateLinks(ParticleData *pDataPtr);

    void setOffset(Real3 newOffset) {
        offset = newOffset;
    }

    void setup(ArgParser &parser, lpReal_t scale) {
        parseIceParam(parser.table);

        // link generator parameters
        breakLinkSeed = parser.getArg<int>("lattice.seed").value_or(1);
        breakLinkSeedOffset = parser.getArg<int>("lattice.seedOffset").value_or(1);
        linkGenScale = parser.getArg<double>("lattice.linkGenScale").value_or(1.2);

        geomCollection.buildFromConfig(parser);

        buildGeometries();
        setupLimits(parser);
        setupLattice(scale, parser.table);

        // todo dist is possily wrong
        geomCollection.generatedAllAccel(maxRadius);
    }


    void parseIceParam(const toml::value &tomlTable);

    void buildGeometries();

    void setupLattice(double scale, const toml::value &tomlTable);

    void setupLimits(ArgParser &parser);
};


#endif //HIDEMGEOMETRY_H
