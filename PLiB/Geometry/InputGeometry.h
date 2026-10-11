//
// Created by Fredrik Robertsen on 16/02/2022.
//

#ifndef PHELL_INPUTGEOMETRY_H
#define PHELL_INPUTGEOMETRY_H

#include "../Global.h"

#include <Allocator.h>

#include "../Util/Real3.h"
#include "Map2d.h"
#include "MeshLoader.h"


class InputGeometry : public ManagedParent{

public:
    virtual bool withinGeometry(Real3 point, lpReal_t radius) = 0;
    virtual void setOffset(Real3 offset) = 0;
    virtual auto getLimits() -> std::tuple<Real3, Real3> = 0;
};

class InputDEM : public InputGeometry {

public:
    Map2d<lpReal_t>* base;
    Map2d<lpReal_t>* surface;
    virtual bool withinGeometry(Real3 cord, lpReal_t radius) override;
    virtual void setOffset(Real3 offset) override;
    virtual auto getLimits() -> std::tuple<Real3, Real3> override;

    void toVTKHDF(std::string path);
};


class InputBox: public InputGeometry {
    Real3 lower;
    Real3 upper;
public:
    InputBox(Real3 lower, Real3 upper): lower{lower}, upper{upper}{

    }
    virtual void setOffset(Real3 offset) override;
    virtual auto getLimits() -> std::tuple<Real3, Real3> override;
    virtual bool withinGeometry(Real3 cord, lpReal_t radius) override;
};

class InputBed: public InputGeometry {
public:
    Map2d<lpReal_t>* bed;
    Map2d<lpReal_t>* friction;
    DEVCALLABLE
    lpReal_t getHeight(lpReal_t xCord, lpReal_t yCord){
        //
        return bed->interp(xCord, yCord);
    }

    DEVCALLABLE
    lpReal_t getFric(lpReal_t xCord, lpReal_t yCord){
        //
        return friction->interp(xCord, yCord);
    }

    virtual bool withinGeometry(Real3 cord, lpReal_t radius) override;
    virtual void setOffset(Real3 offset) override;
    virtual auto getLimits() -> std::tuple<Real3, Real3> override;

    std::tuple<Map2d<lpReal_t>*, Map2d<lpReal_t>*> getPtrs();

    void toVTKHDF(std::string path);
};




#endif //PHELL_INPUTGEOMETRY_H
