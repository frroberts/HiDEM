//
// Created by Fredrik Robertsen on 7/7/24.
//

#include "HiDEMGeometry.h"

#include <iostream>
#include <toml.hpp>

#include "Generators/BlockParticleGenerator.h"
#include "Generators/FCCParticleGenerator.h"
#include "Generators/HCPParticleGenerator.h"
#include "Generators/FileParticleGenerator.h"
#include "Geometry/MapLoader.h"
#include "Util/ArgParser.h"


ManVec<ElementaryParticle> HiDEMGeometry::generateParticles(Domain ownDomain) {
    ManVec<ElementaryParticle> particles;

    pGenPtr->generate(ownDomain, boundingGeometries, particleGenerateProbabilities, offset, particles);

    return particles;
}

ManVec<ElementaryLink> HiDEMGeometry::generateLinks(ParticleData *pDataPtr) {
        int seed = breakLinkSeed + (getLocalRank() * breakLinkSeedOffset);

        return lGen->generateLinksBreakProbMulti(maxRadius, *pDataPtr, linkGenerateProbabilities, seed);
    }

void HiDEMGeometry::parseIceParam(const toml::value &tomlTable) {
    // make different ice paramters
    const auto iceParameters = toml::find<std::vector<toml::table> >(tomlTable, "iceParameters");

    int ind = 0;
    for (toml::value tomlBase: iceParameters) {
        std::string boundingString = toml::find<std::string>(tomlBase,
                                                             "boundingGeometry");
        // tomlBase["boundingGeometry"].as_string()->get();
        if (tomlBase.contains("breakLinkProbability")) {
            std::cout << "breakLinkProbability is deprecated, use linkGenerateProbability instead which is the inverse"
                    << std::endl;
            exit(0);
        }

        // make these optional
        auto linkGenProb = MapLoader::MapFromToml(tomlBase, "linkGenerateProbability", 1.0);
        auto partGenProb = MapLoader::MapFromToml(tomlBase, "particleGenerateProbability", 1.0);

        linkGenerateProbabilities.push_back(linkGenProb);
        particleGenerateProbabilities.push_back(partGenProb);

        auto tomlLinkTo = toml::find<std::vector<int> >(tomlBase, "linkWith");
        std::vector<int> linkWith;

        for (int val: tomlLinkTo) {
            linkToPairs.push_back({ind, val});
        }

        boundingGeometryStrings.push_back(boundingString);

        ind++;
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
        Real3 originCoordinate = TomlParser::getReal3Opt(latticeBase, "originCoordinate", lowerLimit);
        Real3 fccRotation = TomlParser::getReal3Opt(latticeBase, "fccRotation", {0,0,0});

        maxRadius = radius;

        pGenPtr = std::make_shared<FCCParticleGenerator>(lowerLimit, upperLimit, spacing,
                                                         radius, originCoordinate, fccRotation);
    } else if (pGenType == "HCP") {
        double radius = toml::find<double>(latticeBase, "particleRadius") * scale;
        Real3 originCoordinate = TomlParser::getReal3Opt(latticeBase, "originCoordinate", lowerLimit);

        maxRadius = radius;

        pGenPtr = std::make_shared<HCPParticleGenerator>(lowerLimit, upperLimit, radius, originCoordinate);
    } else if (pGenType == "Block") {
        auto blockGen = std::make_shared<BlocParticleGenerator>(
            lowerLimit, upperLimit, scale, toml::find<double>(latticeBase, "blockSize") * scale);
        std::string path = toml::find<std::string>(latticeBase, "path");

        blockGen->loadUnitCell(path);
        maxRadius = blockGen->maxRad * scale;
        pGenPtr = blockGen;
    } else if (pGenType == "File") {
        //
        std::string path = toml::find<std::string>(latticeBase, "path");
        std::cout << path << std::endl;
        std::string xCord = toml::find_or<std::string>(latticeBase, "xCords", "xCord");
        std::string yCord = toml::find_or<std::string>(latticeBase, "yCords", "yCord");
        std::string zCord = toml::find_or<std::string>(latticeBase, "zCords", "zCord");
        std::string radius = toml::find_or<std::string>(latticeBase, "radius", "radius");

        auto fileGen = std::make_shared<FileParticleGenerator>(path, xCord, yCord, zCord, radius);
        maxRadius = toml::find<double>(latticeBase, "particleRadius") * scale;

        pGenPtr = fileGen;
    } else {
        std::cout << "please specify lattice type" << std::endl;
        exit(1);
    }

    lGen = new LinkGenerator();
    lGen->setDistScale(toml::find<double>(latticeBase, "linkGenScale"));
    lGen->setLinkTo(linkToPairs);

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
        Logger::writeAtFirst("constraining xInputLower\n");
    }
    if (yInputLower) {
        lowerLimit.y = yInputLower.value();
        Logger::writeAtFirst("constraining yInputLower\n");
    }
    if (zInputLower) {
        lowerLimit.z = zInputLower.value();
        Logger::writeAtFirst("constraining zInputLower\n");
    }
    if (xInputUpper) {
        upperLimit.x = xInputUpper.value();
        Logger::writeAtFirst("constraining xInputUpper\n");
    }
    if (yInputUpper) {
        upperLimit.y = yInputUpper.value();
        Logger::writeAtFirst("constraining yInputUpper\n");
    }
    if (zInputUpper) {
        upperLimit.z = zInputUpper.value();
        Logger::writeAtFirst("constraining zInputUpper\n");
    }
    zeroCenter = parser.getArg<bool>("lattice.zeroCenter").value_or(false);

    // todo zero center here ?
    Real3 center = (lowerLimit + upperLimit) / Real3(2.0);
    if (zeroCenter) {
        //
        Logger::writeAtFirst(fmt::format("zero center offset {},{},{}\n", center.x, center.y, center.z));
        // std::cout << " -- -- -- center " << center.x << " " << center.y << " " << center.z << " " << std::endl;
        center.z = 0;
        geomCollection.setOffset(center);
        // setOffset(center);

        lowerLimit = lowerLimit - center;
        upperLimit = upperLimit - center;

        for (auto prob: linkGenerateProbabilities)
        {
            prob->setOffset(center.x, center.y);
        }
        for (auto prob: particleGenerateProbabilities)
        {
            prob->setOffset(center.x, center.y);
        }
    }
}
