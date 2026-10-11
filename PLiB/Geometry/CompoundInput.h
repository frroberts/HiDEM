//
// Created by Fredrik Robertsen on 25/02/2022.
//

#ifndef PHELL_COMPOUNDINPUT_H
#define PHELL_COMPOUNDINPUT_H

#include "InputGeometry.h"

class InputOR : public  InputGeometry {
    std::shared_ptr<InputGeometry> a;
    std::shared_ptr<InputGeometry> b;
public:
    InputOR(std::shared_ptr<InputGeometry> a, std::shared_ptr<InputGeometry> b) : a{a}, b{b} {
        // construct the references
    }
    virtual void setOffset(Real3 offset) override;
    virtual auto getLimits() -> std::tuple<Real3, Real3> override;
    virtual bool withinGeometry(Real3 cord, lpReal_t radius) override;
};

class InputAND : public  InputGeometry {
    std::shared_ptr<InputGeometry> a;
    std::shared_ptr<InputGeometry> b;
public:
    InputAND(std::shared_ptr<InputGeometry> a, std::shared_ptr<InputGeometry> b) : a{a}, b{b} {
        // construct the references
    }
    virtual void setOffset(Real3 offset) override;
    virtual auto getLimits() -> std::tuple<Real3, Real3> override;
    virtual bool withinGeometry(Real3 cord, lpReal_t radius) override;
};

class InputNOT : public  InputGeometry {
    std::shared_ptr<InputGeometry> a;
    std::shared_ptr<InputGeometry> b;
public:
    InputNOT(std::shared_ptr<InputGeometry> a, std::shared_ptr<InputGeometry> b) : a{a}, b{b} {
        // construct the references
    }
    virtual void setOffset(Real3 offset) override;
    virtual auto getLimits() -> std::tuple<Real3, Real3> override;
    virtual bool withinGeometry(Real3 cord, lpReal_t radius) override;
};





#endif //PHELL_COMPOUNDINPUT_H
