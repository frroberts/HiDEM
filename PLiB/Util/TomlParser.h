//
// Created by Fredrik Robertsen on 12/03/2022.
//

#ifndef PHELL_TOMLPARSER_H
#define PHELL_TOMLPARSER_H

#include <toml.hpp>
#include <iostream>
#include "Real3.h"

#include <toml.hpp>

#include <optional>


class TomlParser {
    //

    template<class T>
    static double getDouble(toml::value tomlBase, T name){
        const auto vx = toml::find(tomlBase, name);
        double x = vx.is_floating() ? vx.as_floating(std::nothrow) :
                   static_cast<double>(vx.as_integer()); // it throws if vx is neither
        // floating nor integer.
        return x;
    }


public:
    static Real3 getReal3(toml::value &tomlBase, std::string name) {
        try {
            Real3 val;
            if (tomlBase.contains(name)) {

                val.x = getDouble(toml::find(tomlBase, name), 0);
                val.y = getDouble(toml::find(tomlBase, name), 1);
                val.z = getDouble(toml::find(tomlBase, name), 2);
                //val.x = tomlBase[name][0].value<double>().value();
                //val.y = tomlBase[name][1].value<double>().value();
                //val.z = tomlBase[name][2].value<double>().value();
            } else {
                throw std::exception();
            }
            return val;

        }
        catch (std::exception e) {
            std::cout << "failed to find " << name << " in " << tomlBase << std::endl;
            abort();
            return {0};
        }
    }

    static Real3 getReal3Opt(toml::value tomlBase, std::string name, Real3 def) {
        try {
            Real3 val;
            if (tomlBase.contains(name)) {
                val.x = getDouble(toml::find(tomlBase, name), 0);
                val.y = getDouble(toml::find(tomlBase, name), 1);
                val.z = getDouble(toml::find(tomlBase, name), 2);
            } else {
                val = def;
            }
            return val;

        }
        catch (std::exception e) {
            abort();
            return {0};
        }
    }

    template<class T>
    static void getVal(std::string &name, toml::value &tomlBase, std::optional<double> &val) {
        val = getDouble(tomlBase, name);
    }

    template<class T>
    static void getVal(std::string &name, toml::value &tomlBase, std::optional<T> &val) {
        val = toml::find<T>(tomlBase, name);
    }

};

#endif //PHELL_TOMLPARSER_H
