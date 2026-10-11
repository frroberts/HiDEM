//
// Created by robertse on 29/09/23.
//

#ifndef PHELL_BLOCKPARTICLEGENERATOR_H
#define PHELL_BLOCKPARTICLEGENERATOR_H

#include <Util/Real3.h>
#include <ManVec.h>
#include "ParticleGenerator.h"
#include "Util/Util.h"

struct ElemParticle{
    Real3 cord;
    double radius;
    int id;
};

class BlocParticleGenerator : public ParticleGenerator {
public:
    Real3 gLow;
    Real3 gHigh;
    double scale;
    Real3 blockSize;

    std::vector<ElemParticle> unitCell;

    double maxRad = 0;

    BlocParticleGenerator(Real3 gLow, Real3 gHigh, double scale, Real3 blockSize): gLow{gLow}, gHigh{gHigh}, scale{scale}, blockSize{blockSize} {

    }

        template <typename T>
    void splitString(const std::string &input, const std::regex &splitExpr, T fun)
    {
        for(auto iter = std::sregex_token_iterator(input.begin(), input.end(), splitExpr, -1);
            iter != std::sregex_token_iterator();
            iter++)
        fun(iter->str());
    }


    void loadUnitCell(std::string path) {

        std::ifstream inFile;
        inFile.open(path);

        std::string line;

        std::regex splitExpr = std::regex(R"(\s+)");
        auto start = std::chrono::steady_clock::now();
        size_t readLines = 0;
        std::vector<std::vector<double>> columns;
        int fieldCount = 5;
        columns.resize(fieldCount);

        if (inFile.is_open()) {
            while (getline(inFile, line)) {
                int column = 0;

                splitString(line, splitExpr, [&](std::string element) {
                    if (element.length() == 0) {
                        return;
                    }
                    columns[column].push_back(std::stof(element));
                    column++;
                });
                if (column != fieldCount) {
                    std::cout << "Malformed input, on row " << readLines << std::endl;
                    exit(1);
                }
                readLines++;
            }
        } else {
            std::cout << "open failed" << std::endl;
        }
        //
        for (int i = 0; i < readLines; ++i) {
            ElemParticle p;
            p.cord = {columns[1][i],columns[2][i],columns[3][i]};
            p.radius = columns[4][i];
            p.id = i;
            maxRad = std::max(maxRad, p.radius);

            unitCell.push_back(p);
        }

    }


    virtual void
    generate(Domain &dom, std::vector<std::shared_ptr<InputGeometry>> geometries, const std::vector<Map<double> *> &generateProbabilities, Real3 offset,
             ManVec<ElementaryParticle> &particles) {
        //
        Event geomGenerateEvent;
        geomGenerateEvent.start();

        std::cout << "Gen Domain: {" << gLow.x << "," << gLow.y << "," << gLow.z << "}" << std::endl;
        std::cout << "Gen Domain: {" << gHigh.x << "," << gHigh.y << "," << gHigh.z << "}" << std::endl;
        std::cout << "Block size: {" << blockSize.x << "," << blockSize.y << "," << blockSize.z << "}" << std::endl;
        std::cout << "---------------------------------------" << "\n";

        int self = getLocalRank();

        for(auto &p: unitCell){
            p.radius *= scale;
            p.cord = p.cord * Real3{scale};
        }

        // todo rethink currently iterates whole global domain on all nodes

        int lowerXInt = floor((gLow.x - gLow.x) / blockSize.x) - 1;;
        int lowerYInt = floor((gLow.y - gLow.y) / blockSize.y) - 1;;
        int lowerZInt = floor((gLow.z - gLow.z) / blockSize.z) - 1;;
        int upperXInt = ceil((gHigh.x - gLow.x) / blockSize.x) + 1;
        int upperYInt = ceil((gHigh.y - gLow.y) / blockSize.y) + 1;
        int upperZInt = ceil((gHigh.z - gLow.z) / blockSize.z) + 1;

        Domain global;
        global.lower = gLow;
        global.upper = gHigh;
        global.setHalo(0);

        // seed this better
        std::random_device r;
        std::mt19937 randomEngine(r());
        std::uniform_real_distribution<double> uniformDist(0, 1);
        std::vector<int> genIndexes;


        for (int k = lowerZInt; k < upperZInt; ++k) {
            for (int j = lowerYInt; j < upperYInt; ++j) {
                for (int i = lowerXInt; i < upperXInt; ++i) {

                    ManVec<ElementaryParticle> particlesLocal;
                    for (auto &p: unitCell) {
                        //
                        Real3 cord = p.cord + (blockSize * Real3{double(i), double(j), double(k)}) + offset;
                        if(global.within(cord)) {
                            this->createParticle(cord, self, particlesLocal, dom, geometries,
                                       generateProbabilities, offset, p.radius, uniformDist(randomEngine));
                        }
                    }
                    for (const auto &p: particlesLocal) {
                        particles.push_back(p);
                    }
                }
            }
        }
        this->setParticleKeys(particles);

        geomGenerateEvent.end();
        Logger::writeAtFirst("Particles generated: " + Logger::getStats(particles.size(), "points") + "\n");
        Logger::writeAtFirst("Particle generate time " + Logger::getLimits(geomGenerateEvent.getLastDuration(), "s") + "\n");
    }
};

#endif //PHELL_BLOCKPARTICLEGENERATOR_H
