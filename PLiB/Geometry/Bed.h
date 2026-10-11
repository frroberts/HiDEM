//
// Created by Fredrik Robertsen on 04/07/24.
//

#include "Map2d.h"

class Bed : public ManagedParent {
    Map2d<lpReal_t> *bedPtr;
    Map2d<lpReal_t> *fricPtr;

public:

    Bed(Map2d<lpReal_t> *bedPtr, Map2d<lpReal_t> *fricPtr) : bedPtr(bedPtr), fricPtr(fricPtr) {

    }

    DEVCALLABLE
    lpReal_t getHeight(lpReal_t xCord, lpReal_t yCord){
        //
        return bedPtr->interp(xCord, yCord);
    }

    DEVCALLABLE
    lpReal_t getFric(lpReal_t xCord, lpReal_t yCord){
        //
        return fricPtr->interp(xCord, yCord);
    }


};

