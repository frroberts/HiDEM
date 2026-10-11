//
// Created by Fredrik Robertsen on 13/02/23.
//

#ifndef PHELL_MAPLOADER_H
#define PHELL_MAPLOADER_H


#include "Map2d.h"
#include "Map2dLoader.h"
#include <toml.hpp>
#include <new>

// #define GPU
#ifdef GPU
// hacking around inheretance problems on the GPU, these need to be placement new:ed in dev code
template <typename T>
__global__ void createMapSingle(MapSingle<T> *ptrOut){
    ::new (reinterpret_cast<MapSingle<T>*>(ptrOut)) MapSingle<T>;
}

template <typename T>
__global__ void createMap2d(Map2d<T> *ptrOut){
    ::new (reinterpret_cast<Map2d<T>*>(ptrOut)) Map2d<T>;
}
#endif

class MapLoader {
public:
    template<class T>
    static Map<T>* MapFromToml(toml::value tomlBase, std::string key, T def,  bool devAlloc = false){
        //

        bool found = false;
        toml::value tomlMap;
        if (auto pos = key.find("."); pos == std::string::npos) {
            if(tomlBase.contains(key)) {
                tomlMap = toml::find(tomlBase, key);
                found = true;
            }
        } else {
            //
            std::string first = key.substr(0, pos);
            std::string second = key.substr(pos + 1, key.length());

            if (tomlBase.contains(first)) {
                toml::value tomlFirst = toml::find(tomlBase, first);
                if (tomlFirst.contains(second)) {
                    tomlMap = toml::find(tomlFirst, second);
                    found = true;
                }
            }
        }

        if (!found) {
            // get as T
            MapSingle<T> *ptr;
#ifdef GPU__
            if (devAlloc) {
                devMallocManaged((void **) &ptr, sizeof(MapSingle<T>));
                createMapSingle<<<1, 1>>>(ptr);
            } else
#endif
            {
                ptr = new MapSingle<T>();
            }
            ptr->setValue(def);
            return ptr;
        }

        // figure out what map we are building
        if(tomlMap.is_table()){
            // decode table
            // todo check what type it is, current is only csv2d
            // todo clean up memory
            Map2dLoader loader;
            loader.readInput(toml::find<std::string>(tomlMap, "path"));
            auto ptrHost = loader.get<T>(toml::find<int>(tomlMap, "fieldID"));

            Map2d<T> *ptr = ptrHost;
#ifdef GPU__
            if(devAlloc) {
                devMallocManaged((void **) &ptr, sizeof(Map2d<T>));
                createMap2d<<<1, 1>>>(ptr);
                devMemcpy(ptr, ptrHost, sizeof(Map2d<T>), devMemcpyDefault);
            }
#endif
            return ptr;

        } else {
            // get as T
            MapSingle<T> *ptr;
#ifdef GPU__
            if(devAlloc)
            {
                devMallocManaged((void**) &ptr, sizeof(MapSingle<T>));
                createMapSingle<<<1,1>>>(ptr);
            }
            else
#endif
            {
                ptr = new MapSingle<T>();
            }
            ptr->setValue(toml::get<T>(tomlMap));
            return ptr;
        }
    }
};


#endif //PHELL_MAPLOADER_H
