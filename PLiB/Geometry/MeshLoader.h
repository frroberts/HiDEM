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
#include "../Logger/Logger.h"
#include "../Util/Util.h"

class MeshLoader {
public:
    static Real3 strToReal3(std::string endStr) {
        Real3 v;
        v.x = std::stof( endStr.substr(0, endStr.find_first_of(" ")));
        endStr = endStr.substr(endStr.find_first_of(" ")+1);
        v.y = std::stof( endStr.substr(0, endStr.find_first_of(" ")));
        endStr = endStr.substr(endStr.find_first_of(" ")+1);
        v.z = std::stof( endStr.substr(0, endStr.find_first_of(" ")));

        return v;
    }

    static std::vector<Triangle> loadObj(std::string path) {

        std::vector<Real3> vertices;
        std::vector<Real3> vertexNormals;
        std::vector<Real3> vertexTexture;
        std::vector<Triangle> faces;

        std::chrono::time_point<std::chrono::system_clock> start, end;
        start = std::chrono::system_clock::now();

        std::ifstream ifs;
        ifs.open(path, std::ifstream::in);
        if (!ifs.good())
        	std::cout << path << " file open failed" << std::endl;

        while (ifs.good()) {
            std::string line;
            std::getline(ifs, line);

            // really want to do string_view here, but
            if (line.length() < 2)
                continue;

            auto strStart = std::string(line.data(),2);

            if (strStart.find("v ") != std::string::npos) {
                auto endStr = std::string(line.data()+2,line.size()-2);
                Real3 v = strToReal3(endStr);
                vertices.push_back(v);
            } else if (strStart.find("vn") != std::string::npos) {
                auto endStr = std::string(line.data()+3,line.size()-3);
                Real3 vn = strToReal3(endStr);
                vertexNormals.push_back(vn);
            } else if (strStart.find("vt") != std::string::npos) {
                auto endStr = std::string(line.data()+3,line.size()-3);
                Real3 vt = strToReal3(endStr);
                vertexTexture.push_back(vt);
            } else if (strStart.find("f ") != std::string::npos) {
                auto endStr = std::string(line.data()+2,line.size()-2);

                int numSlash = std::reduce(endStr.begin(), endStr.end(), 0, [](int count, char c) -> int {
                    return count + (c == '/');
                });

                std::array<Real3, 3> vtxes;
                // std::array<Real3, 3> vtxsNormals;
                int i = 0;

                splitStringDelims(endStr, " ", [&](std::string token) {
                    int vtxId = std::stoi(token.substr(0, token.find_first_of('/')));
                    vtxes[i] = vertices[vtxId-1];
                    if (numSlash != 0) {
                        int normalId = std::stoi(token.substr(token.find_last_not_of('/'), token.size() - 1));
                        // vtxsNormals[i] = vertexNormals[normalId-1];
                    }
                    i++;
                });
                if (i != 3) {
                    std::cout << "malformed tri " << endStr << std::endl;
                }
                Triangle tri{ vtxes[0], vtxes[1], vtxes[2] };
                faces.push_back(tri);
            }

        }


        end = std::chrono::system_clock::now();

        std::chrono::duration<double> elapsed_seconds = end - start;
        Logger::writeAtFirst(path +" Mesh load time at rank 0: " + std::to_string(elapsed_seconds.count()) + "s " + std::to_string(faces.size()) + " triangles\n");

        return faces;
    }

};


#endif //PHELL_MESHLOADER_H
