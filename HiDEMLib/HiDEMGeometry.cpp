//
// Created by Fredrik Robertsen on 7/7/24.
//

#include "HiDEMGeometry.h"

#include <iostream>
#include <toml.hpp>

#include "Generators/BlockParticleGenerator.h"
#include "Generators/FCCParticleGenerator.h"
#include "Geometry/MapLoader.h"
#include "Util/ArgParser.h"


ManVec<ElementaryParticle> HiDEMGeometry::generateParticles(Domain ownDomain) {
    ManVec<ElementaryParticle> particles;
    pGenPtr->generate(ownDomain, boundingGeometries, particleGenerateProbabilities, offset, particles);

    return particles;
}

ManVec<ElementaryLink> HiDEMGeometry::generateLinks(ParticleData *pDataPtr) {
        int seed = breakLinkSeed + (getLocalRank() * breakLinkSeedOffset);
        return lGen.generateLinksBreakProbMulti(linkGenScale, maxRadius, *pDataPtr, linkGenerateProbabilities,
                                                 seed, linkToTypes);
    }

void HiDEMGeometry::parseIceParam(const toml::value &tomlTable) {
    // make different ice paramters
    const auto iceParameters = toml::find<std::vector<toml::table> >(tomlTable, "iceParameters");

    for (toml::value tomlBase: iceParameters) {
        std::string boundingString = toml::find<std::string>(tomlBase,
                                                             "boundingGeometry");
        // tomlBase["boundingGeometry"].as_string()->get();
        if (tomlBase.contains("breakLinkProbability")) {
            std::cout << "breakLinkProbability is deprecated, use linkGenerateProbability instead which is the inverse"
                    << std::endl;
            exit(0);
        }
        // double breakProbability = toml::find<double>(tomlBase, "breakLinkProbability");// tomlBase["breakLinkProbability"].as_floating_point()->get();

        // make these optional
        auto linkGenProb = MapLoader::MapFromToml(tomlBase, "linkGenerateProbability", 1.0);
        auto partGenProb = MapLoader::MapFromToml(tomlBase, "particleGenerateProbability", 1.0);

        linkGenerateProbabilities.push_back(linkGenProb);
        particleGenerateProbabilities.push_back(partGenProb);

        auto tomlLinkTo = toml::find<std::vector<int> >(tomlBase, "linkWith");
        std::vector<int> linkWith;

        for (int val: tomlLinkTo) {
            linkWith.push_back(val); // tomlBase["linkWith"][j].value<int>().value());
        }

        boundingGeometryStrings.push_back(boundingString);
        linkToTypes.push_back(linkWith);
    }
}

void HiDEMGeometry::buildGeometries() {
    std::string allString = "";

    for (auto s: boundingGeometryStrings) {
        //
        allString += " (" + s + ") ";
        boundingGeometries.push_back(geomCollection.generateCompound(s));
        if (boundingGeometries.size() != boundingGeometryStrings.size())
            allString += " OR ";
    }
    // geomBuilder.collisionMeshData->toVTKFull("full.vtu");
    fullGeometry = geomCollection.generateCompound(allString);
}


void HiDEMGeometry::setupLattice(double scale, const toml::value &tomlTable) {
    const auto latticeBase = toml::find<toml::value>(tomlTable, "lattice");
    std::string pGenType = toml::find<std::string>(latticeBase, "type");

    if (pGenType == "FCC") {
        double spacing = toml::find<double>(latticeBase, "fccSpacing") * scale;
        double radius = toml::find<double>(latticeBase, "particleRadius") * scale;
        maxRadius = radius;

        pGenPtr = std::make_shared<FCCParticleGenerator>(lowerLimit, upperLimit, spacing,
                                                         radius);
    } else if (pGenType == "Block") {
        auto blockGen = std::make_shared<BlocParticleGenerator>(
            lowerLimit, upperLimit, scale, toml::find<double>(latticeBase, "blockSize") * scale);
        std::string path = toml::find<std::string>(latticeBase, "path");

        blockGen->loadUnitCell(path);
        maxRadius = blockGen->maxRad * scale;
        pGenPtr = blockGen;
    } else {
        std::cout << "please specify lattice type" << std::endl;
        exit(1);
    }
}

// standalone if returning reals, and taking either full geometry or its limits
void HiDEMGeometry::setupLimits(ArgParser &parser) {
    auto limits = fullGeometry->getLimits();
    lowerLimit = std::get<0>(limits);
    upperLimit = std::get<1>(limits);

    auto xInputUpper = parser.getArg<double>("world.xInputUpper");
    auto yInputUpper = parser.getArg<double>("world.yInputUpper");
    auto zInputUpper = parser.getArg<double>("world.zInputUpper");

    auto xInputLower = parser.getArg<double>("world.xInputLower");
    auto yInputLower = parser.getArg<double>("world.yInputLower");
    auto zInputLower = parser.getArg<double>("world.zInputLower");

    if (xInputLower) {
        lowerLimit.x = xInputLower.value();
        Logger::writeAtFirst("constraining xInputLower");
    }
    if (yInputLower) {
        lowerLimit.y = yInputLower.value();
        Logger::writeAtFirst("constraining yInputLower");
    }
    if (zInputLower) {
        lowerLimit.z = zInputLower.value();
        Logger::writeAtFirst("constraining zInputLower");
    }
    if (xInputUpper) {
        upperLimit.x = xInputUpper.value();
        Logger::writeAtFirst("constraining xInputUpper");
    }
    if (yInputUpper) {
        upperLimit.y = yInputUpper.value();
        Logger::writeAtFirst("constraining yInputUpper");
    }
    if (zInputUpper) {
        upperLimit.z = zInputUpper.value();
        Logger::writeAtFirst("constraining zInputUpper");
    }
    zeroCenter = parser.getArg<bool>("general.zeroCenter").value_or(false);

    // todo zero center here ?
    Real3 center = (lowerLimit + upperLimit) / Real3(2.0);
    if (zeroCenter) {
        //
        std::cout << "center " << center.x << " " << center.y << " " << center.z << " " << std::endl;
        center.z = 0;
        setOffset(center);

        lowerLimit = lowerLimit - center;
        upperLimit = upperLimit - center;
    }
}
