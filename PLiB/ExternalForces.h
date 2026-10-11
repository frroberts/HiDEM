//
// Created by Fredrik Robertsen on 18/02/23.
//

#ifndef PHELL_EXTERNALFORCES_H
#define PHELL_EXTERNALFORCES_H


#include "Geometry/Map.h"
#include "Geometry/MapLoader.h"
#include <toml.hpp>
#include "Global.h"

/*
 * AMD GPUs can't really do virtual functions at all it seems like
 * Hence the single value force is implemented as a map2d with one element
 * Revisit it AMDs support gets better
 * (cuda support for them is also not straight forward, but doable by creating the object in GPU code
 * using for instance placement new)
 * 02/2023
 */


class ExternalForces : public ManagedParent{
public:
    Map2d<lpReal_t> *xForce;
    Map2d<lpReal_t> *yForce;
    Map2d<lpReal_t> *zForce;
    Map2d<lpReal_t> *uForce;
    Map2d<lpReal_t> *vForce;
    Map2d<lpReal_t> *wForce;

    lpReal_t getXForce(lpReal_t xCord, lpReal_t yCord, lpReal_t zCord){
        return xForce->interp(xCord, yCord);
    }
    lpReal_t getYForce(lpReal_t xCord, lpReal_t yCord, lpReal_t zCord){
        return yForce->interp(xCord, yCord);
    }
    lpReal_t getZForce(lpReal_t xCord, lpReal_t yCord, lpReal_t zCord){
        return zForce->interp(xCord, yCord);
    }
    lpReal_t getUForce(lpReal_t xCord, lpReal_t yCord, lpReal_t zCord){
        return uForce->interp(xCord, yCord);
    }
    lpReal_t getVForce(lpReal_t xCord, lpReal_t yCord, lpReal_t zCord){
        return vForce->interp(xCord, yCord);
    }
    lpReal_t getWForce(lpReal_t xCord, lpReal_t yCord, lpReal_t zCord){
        return wForce->interp(xCord, yCord);
    }

private:
};


class ExternalForceBuilder{
public:
    static
    std::optional<ExternalForces*> build(toml::value configTable){
        if (!configTable.contains("externalForce")) {
            return {};
        }
        ExternalForces *extForce = new ExternalForces();

        extForce->xForce = createForce(configTable, "externalForce.xForce");
        extForce->yForce = createForce(configTable, "externalForce.yForce");
        extForce->zForce = createForce(configTable, "externalForce.zForce");
        extForce->uForce = createForce(configTable, "externalForce.uForce");
        extForce->vForce = createForce(configTable, "externalForce.vForce");
        extForce->wForce = createForce(configTable, "externalForce.wForce");
        return extForce;
    }

    static
    Map2d<lpReal_t> *createForce(toml::value configTable, std::string key) {
        //
        bool found = false;
        toml::value tomlMap;
        if (auto pos = key.find("."); pos == std::string::npos) {
            tomlMap = toml::find(configTable, key);
            found = true;
        } else {
            //
            std::string first = key.substr(0, pos);
            std::string second = key.substr(pos + 1, key.length());

            if (configTable.contains(first)) {
                toml::value tomlFirst = toml::find(configTable, first);
                if (tomlFirst.contains(second)) {
                    tomlMap = toml::find(tomlFirst, second);
                    found = true;
                }
            }
        }

        // figure out what map we are building
        if (found && tomlMap.is_table()) {
            // todo check what type it is, current is only csv2d
            // todo clean up memory
            Map2dLoader loader;
            loader.readInput(toml::find<std::string>(tomlMap, "path"));
            auto ptr = loader.get<lpReal_t>(toml::find<int>(tomlMap, "fieldID"));
            return ptr;

        } else {
            // get as T
            Map2d<lpReal_t> *ptr;

            ptr = new Map2d<lpReal_t>(1, 1, 0, 0, 1, 1);

            if (!found) {
                ptr->data[0] = 0;
            } else {
                ptr->data[0] = toml::get<lpReal_t>(tomlMap);
            }
            return ptr;
        }
        // force not found throw exception
    }

};


#endif //PHELL_EXTERNALFORCES_H
