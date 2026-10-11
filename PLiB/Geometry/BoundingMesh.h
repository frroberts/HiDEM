//
// Created by robertse on 20/05/23.
//

#ifndef PHELL_INPUTMESH_H
#define PHELL_INPUTMESH_H

#include "InputGeometry.h"
#include "../MeshData.h"
#include "MeshAccel.h"
#include "../Util/Util.h"

#include "../Util/MPIHelpers.h"
#include "Mesh.h"


class BoundingMesh : public Mesh {

public:
    lpReal_t shellScale = 1;
    bool outerShell = true;
    lpReal_t gridScale = 4;


    BoundingMesh() {

    }

    ~BoundingMesh() {
        //
    }

    bool withinGeometry(Real3 cord, lpReal_t radius) {
        return accel->withinGeometryShellCheck(cord, radius * shellScale, outerShell,
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
        if (tri >= 0) {
            fun(tri, dist);
        }
    }

    // with scale from the mesh
    virtual void generateAccel(lpReal_t radius){

        // make the cells largeish, but if the shell scale is larger use radius * cellScale
        lpReal_t cellSize = radius*gridScale;
        cellSize = std::max(cellSize, radius*shellScale);

        accel->generateAccel(cellSize, radius*shellScale,
                             dataPtr->getXVtxPtr(meshId),
                             dataPtr->getYVtxPtr(meshId),
                             dataPtr->getZVtxPtr(meshId),
                             dataPtr->getMeshSize(meshId));
                             // flatten it as well ? that way input meshes will always be flat 
    }


};

#endif //PHELL_INPUTMESH_H
