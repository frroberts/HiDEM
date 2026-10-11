////
// Created by Fredrik Robertsen on 21/02/2022.
//

#ifndef PHELL_MESHLOADER_H
#define PHELL_MESHLOADER_H

#include <array>
#include <chrono>
#include <regex>
#include <fstream>
#include <iostream>

#include "../Util/Real3.h"
#include "Triangle.h"

class MeshLoader {
public:
    static std::vector<Triangle> loadObj(std::string path) {

        std::vector<Real3> vertices;
        std::vector<Real3> vertexNormals;
        std::vector<Real3> vertexTexture;
        //std::vector<glm::vec<3, glm::vec<3, int>>> faces;
        std::vector<Triangle> faces;

        //Mesh m;

        std::chrono::time_point<std::chrono::system_clock> start, end;
        start = std::chrono::system_clock::now();
        // normal is third element
        const static std::string fpMatch = "-?[\\d.]+(?:e-?\\d+)?";

        const static std::regex vertexRegex("v\\s+(" + fpMatch + ")\\s+(" + fpMatch + ")\\s+(" + fpMatch + ")");
        const static std::regex vertexNormalRegex("vn\\s+(" + fpMatch + ")\\s+(" + fpMatch + ")\\s+(" + fpMatch + ")");
        const static std::regex texCordRegex("vt\\s+(" + fpMatch + ")\\s+(" + fpMatch + ")\\s+(" + fpMatch + ")");
        const static std::regex faceRegex(R"(f\s+([0-9]+)/([0-9]+)/([0-9]+)\s+([0-9]+)/([0-9]+)/([0-9]+)\s+([0-9]+)/([0-9]+)/([0-9]+))"); // needs to support n//n as well
        const static std::regex faceRegexSingle(R"(f\s+([0-9]+)\s+([0-9]+)\s+([0-9]+))");

        std::ifstream ifs;
        ifs.open(path, std::ifstream::in);
        //if (ifs.good())
        //	std::cout << "file opened" << std::endl;
        while (ifs.good()) {
            std::string line;
            std::getline(ifs, line);

            // show contents of marked subexpressions within each match
            std::smatch matches;
            if (std::regex_search(line, matches, vertexRegex)) {
                Real3 v;
                v.x = atof(matches[1].str().c_str());
                v.y = atof(matches[2].str().c_str());
                v.z = atof(matches[3].str().c_str());
                vertices.push_back(v);
            } else if (std::regex_search(line, matches, vertexNormalRegex)) {
                Real3 vn;
                vn.x = atof(matches[1].str().c_str());
                vn.y = atof(matches[2].str().c_str());
                vn.z = atof(matches[3].str().c_str());
                vertexNormals.push_back(vn);
            } else if (std::regex_search(line, matches, texCordRegex)) {
                Real3 vt;
                vt.x = atof(matches[1].str().c_str());
                vt.y = atof(matches[2].str().c_str());
                vt.z = atof(matches[3].str().c_str());
                vertexTexture.push_back(vt);
            } else if (std::regex_search(line, matches, faceRegex)) {
                // vertex normal texture
                faces.push_back({
                                        vertices[atoi(matches[1].str().c_str()) - 1],
                                        vertices[atoi(matches[4].str().c_str()) - 1],
                                        vertices[atoi(matches[7].str().c_str()) - 1],
                                });

                faces.back().normal = vertexNormals[atoi(matches[3].str().c_str()) - 1];
            } else if (std::regex_search(line, matches, faceRegexSingle)) {
                // vertex normal texture
                faces.push_back({
                                        vertices[atoi(matches[1].str().c_str()) - 1],
                                        vertices[atoi(matches[2].str().c_str()) - 1],
                                        vertices[atoi(matches[3].str().c_str()) - 1],
                                });

            }
        }


        end = std::chrono::system_clock::now();

        std::chrono::duration<double> elapsed_seconds = end - start;
        std::cout << "load time: " << elapsed_seconds.count() << "s " << faces.size() << " triangles\n";
        return faces;
    }

};


#endif //PHELL_MESHLOADER_H
