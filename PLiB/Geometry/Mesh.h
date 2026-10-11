//
// Created by Fredrik Robertsen on 15/11/25.
//

#ifndef PHELL_MESH_H
#define PHELL_MESH_H

#include "InputGeometry.h"
#include "../MeshData.h"
#include "MeshAccel.h"
#include "../Util/Util.h"
#include <AtomicOps.h>

#include "../Util/MPIHelpers.h"


class Mesh : public InputGeometry {

public:
    int meshId;

    MeshData *dataPtr;
    MeshAccel *accel;


    Mesh() {

    }

    ~Mesh() {
        //
    }

    virtual void setOffset(Real3 offset) override{
        this->offset = offset;
        for (int i = dataPtr->meshStarts[meshId]; i < dataPtr->meshEnds[meshId]; i++) {
            for (int j = 0; j < 3; j++) {
                dataPtr->xVtxCord[i * 3 + j] -= offset.x;
                dataPtr->yVtxCord[i * 3 + j] -= offset.y;
                dataPtr->zVtxCord[i * 3 + j] -= offset.z;
            }
        }
    }

    virtual auto getLimits() -> std::tuple<Real3, Real3> override {
        Real3 min{std::numeric_limits<lpReal_t>::max()};
        Real3 max{std::numeric_limits<lpReal_t>::lowest()};

        for(int i = dataPtr->meshStarts[meshId]; i < dataPtr->meshEnds[meshId]; i++){

            for (int j = 0; j < 3; j++){
                max.x = std::max(dataPtr->xVtxCord[i*3+j], (lpReal_t )max.x);
                max.y = std::max(dataPtr->yVtxCord[i*3+j], (lpReal_t )max.y);
                max.z = std::max(dataPtr->zVtxCord[i*3+j], (lpReal_t )max.z);

                min.x = std::min(dataPtr->xVtxCord[i*3+j], (lpReal_t )min.x);
                min.y = std::min(dataPtr->yVtxCord[i*3+j], (lpReal_t )min.y);
                min.z = std::min(dataPtr->zVtxCord[i*3+j], (lpReal_t )min.z);
            }
        }
        return std::tuple<Real3, Real3>(min, max);
    }

    // maybe should be pure virtual
    virtual bool withinGeometry(Real3 cord, lpReal_t radius) override {
        return accel->withinGeometry(cord,
                                     dataPtr->getXVtxPtr(meshId),
                                     dataPtr->getYVtxPtr(meshId),
                                     dataPtr->getZVtxPtr(meshId),
                                     dataPtr->getMeshSize(meshId));
    }

    template<class Fun>
    DEVCALLABLE
    void closestTriFun(lpReal_t xCord, lpReal_t yCord, lpReal_t zCord, lpReal_t interactDist,
                       pInd_t pInd, Fun fun) {
        //
        auto [tri, dist, searched] = accel->getClosestTriAccel(xCord, yCord, zCord, interactDist,
                                                     dataPtr->getXVtxPtr(meshId),
                                                     dataPtr->getYVtxPtr(meshId),
                                                     dataPtr->getZVtxPtr(meshId));
        // std::cout << tri << " " << dist << std::endl;
        if (tri >= 0) {
            fun(tri, dist);
        }
    }

    // with scale from the mesh
    // maybe should be pure virtual
    virtual void generateAccel(lpReal_t radius){

        accel->generateAccel(std::max(radius, radius), std::max(radius, radius),
                             dataPtr->getXVtxPtr(meshId),
                             dataPtr->getYVtxPtr(meshId),
                             dataPtr->getZVtxPtr(meshId),
                             dataPtr->getMeshSize(meshId));
    }

};

#endif //PHELL_MESH_H
