//
// Created by Fredrik Robertsen on 12/02/23.
//

#ifndef PHELL_MAP_H
#define PHELL_MAP_H

#include <DevCall.h>
#include <ManVec.h>
#include <Allocator.h>

template<class T>
class Map : public ManagedParent  {
    //
public:
    DEVCALLABLE
    Map(){
        //
    };
    virtual void setOffset(float offsetX, float offsetY)  =  0;
    virtual T getVal(float x, float y, float z) {
        printf("dummy");
        return T();
    }
};

template<class T>
class MapSingle : public Map<T>{
public:
    T val;
    MapSingle(T v): val(v){

    }
    MapSingle() = default;
    virtual void setOffset(float offsetX, float offsetY)
    {
        // not applicable but needs to be implemented
    };
    virtual T getVal(float x, float y, float z) {
        return val;
    }
    void setValue(T value){
        val = value;
    }
};

#endif //PHELL_MAP_H
