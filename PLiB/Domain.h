//
// Created by Fredrik Robertsen on 25/12/2020.
//

#ifndef PHELL_DOMAIN_H
#define PHELL_DOMAIN_H

#include "Util/Real3.h"
#include "Global.h"
#include "DevCall.h"

#include <iostream>

class Domain
{
private:
    /* data */
public:
    Domain(/* args */) = default;
    ~Domain() = default;
    
    Real3 upper;
    Real3 lower;

    Real3 upperHalo;
    Real3 lowerHalo;

    int owner;
    double load;

    DEVCALLABLE
    bool within(Real3 cord)
    {
        //
        bool xIn;
        if(lower.x == upper.x && cord.x == lower.x)
            xIn = true;
        else
            xIn = (cord.x >= lower.x && cord.x < upper.x);

        bool yIn;
        if(lower.y == upper.y && cord.y == lower.y)
            yIn = true;
        else
            yIn = (cord.y >= lower.y && cord.y < upper.y);

        bool zIn;
        if(lower.z == upper.z && cord.z == lower.z)
            zIn = true;
        else
            zIn = (cord.z >= lower.z && cord.z < upper.z);

        return xIn && yIn && zIn;

        return
            (cord.x >= lower.x && cord.x < upper.x) &&
            (cord.y >= lower.y && cord.y < upper.y) &&
            (cord.z >= lower.z && cord.z < upper.z);
    }

    DEVCALLABLE
    bool withinHalo(Real3 cord)
    {
        //
        return (cord.x >= lowerHalo.x && cord.x < upperHalo.x)
        && (cord.y >= lowerHalo.y && cord.y < upperHalo.y)
        && (cord.z >= lowerHalo.z && cord.z < upperHalo.z);
    }

    void print()
    {
        std::cout << "owner " << owner << std::endl;
        std::cout << "lower " << lower.x << " " << lower.y << " " << lower.z << std::endl;
        std::cout << "upper " << upper.x << " " << upper.y << " " << upper.z << std::endl;
        std::cout << "lowerHalo " << lowerHalo.x << " " << lowerHalo.y << " " << lowerHalo.z << std::endl;
        std::cout << "upperHalo " << upperHalo.x << " " << upperHalo.y << " " << upperHalo.z << std::endl;
        std::cout << std::endl;

    }
    Real3 getSize()
    {
        return upper-lower;
    }

    void setHalo(double haloSize)
    {
        upperHalo =  upper + haloSize;
        lowerHalo =  lower - haloSize;
    }

    double getDiagonal() {
        Real3 size = getSize();
        return sqrt((size.x * size.x) + (size.y * size.y) + (size.z * size.z));
    }

    double getSurface(){
        Real3 size = getSize();
        return 2 * (size.x*size.y) + (size.y*size.z) + (size.x*size.z);
    }

};



#endif