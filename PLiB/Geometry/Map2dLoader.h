//
// Created by Fredrik Robertsen on 25/10/2020.
//

#ifndef PHELL_MAP2DLOADER_H
#define PHELL_MAP2DLOADER_H

#include <string>
#include <vector>
#include <array>
#include <fstream>
#include <iostream>
#include <regex>

#include "Map2d.h"

class Map2dLoader {
    std::vector<std::vector<double>> columns;
    int xPoints;
    int yPoints;
    double xMin;
    double yMin;
    double xMax;
    double yMax;

public:
    size_t readInput(std::string path);

    template<class T>
    Map2d<T> *get(int ind) {
        //
        Map2d<T> *ret = new Map2d<T>(xPoints, yPoints, xMin, yMin, xMax, yMax);

        //int i = 100;
        //std::cout << columns.at(0).at(i) << " " << columns.at(1).at(i) << " " << columns.at(ind).at(i) << std::endl;
// #pragma omp parallel for
        for (int i = 0; i < columns[0].size(); i++) {
            ret->setPointCord(columns.at(0).at(i), columns.at(1).at(i), columns.at(ind).at(i));
        }
        return ret;
    }


    template<typename T>
    void splitString(const std::string &input, const std::regex &splitExpr, T fun)
    {
        auto begin = std::sregex_iterator(input.begin(), input.end(), splitExpr);
        auto end = std::sregex_iterator();

        for (std::sregex_iterator i = begin; i != end; ++i) {
            std::string str = i->str();
            fun(str);
        }
    }

    template<typename T>
    void splitStringDelims(const std::string &input, std::string delims, T fun) {
        std::size_t prev = 0, pos;
        while ((pos = input.find_first_of(delims, prev)) != std::string::npos) {
            if (pos > prev)
                fun(input.substr(prev, pos - prev));
            prev = pos + 1;
        }
        if (prev < input.length())
            fun(input.substr(prev, std::string::npos));
    }
};


#endif //PHELL_MAP2DLOADER_H
