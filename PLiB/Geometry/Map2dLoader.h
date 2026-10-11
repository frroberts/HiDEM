//
// Created by Fredrik Robertsen on 25/10/2020.
//

#ifndef PHELL_MAP2DLOADER_H
#define PHELL_MAP2DLOADER_H

#include <string>
#include <vector>
#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
#include <regex>

#include "Map2d.h"

class Map2dLoader {
    std::vector<std::vector<float>> columns;
    int xPoints;
    int yPoints;
    float xMin;
    float yMin;
    float xMax;
    float yMax;

public:
    size_t readInput(std::string path);

    template<class T>
    Map2d<T> *get(int ind) {
        //
        Map2d<T> *ret = new Map2d<T>(xPoints, yPoints, xMin, yMin, xMax, yMax);

        assert(xPoints*yPoints == columns.at(ind).size());
        for (size_t i = 0; i < columns[0].size(); i++) {
            ret->setPointCord(columns.at(0).at(i), columns.at(1).at(i), columns.at(ind).at(i));
        }
        return ret;
    }



};


#endif //PHELL_MAP2DLOADER_H
