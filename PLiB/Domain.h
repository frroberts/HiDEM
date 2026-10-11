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
    bool within(Real3 cord) const
    {
        if ((cord.x < lower.x || cord.x >= upper.x))
            return false;

        if ((cord.y < lower.y || cord.y >= upper.y))
            return false;

        if ((cord.z < lower.z || cord.z >= upper.z))
            return false;

        return true;
    }

    DEVCALLABLE
    bool withinHalo(Real3 cord)
    {
        //
        if ((cord.x < lowerHalo.x || cord.x >= upperHalo.x))
            return false;

        if ((cord.y < lowerHalo.y || cord.y >= upperHalo.y))
            return false;

        if ((cord.z < lowerHalo.z || cord.z >= upperHalo.z))
            return false;

        return true;
    }

    void print()
    {
        std::cout << "owner " << owner << std::endl;
        std::cout << "load " << load << std::endl;
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