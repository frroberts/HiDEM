//
// Created by robertse on 20/05/23.
//

#ifndef PHELL_INPUTMESH_H
#define PHELL_INPUTMESH_H

#include "InputGeometry.h"
#include "../MeshData.h"
#include "MeshAccel.h"
#include "../Util/Util.h"
#include <AtomicOps.h>

#include "../Util/MPIHelpers.h"

class InputMesh : public InputGeometry {

public:
    int meshId;

    MeshData *dataPtr;
    MeshAccel *accel;

    lpReal_t shellScale = 1;
    lpReal_t cellSizeScale = 1;
    lpReal_t rangeFactor = 1;

    InputMesh() {

    }

    ~InputMesh() {
        //
    }

    virtual void setOffset(Real3 offset) override{

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

    virtual bool withinGeometry(Real3 cord, lpReal_t radius) override {
        //std::cout << "." << std::endl;
        return accel->withinGeometry(cord, radius * shellScale,
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
    void generateAccel(lpReal_t radius){

        accel->generateAccel(radius*cellSizeScale, radius*rangeFactor,
                             dataPtr->getXVtxPtr(meshId),
                             dataPtr->getYVtxPtr(meshId),
                             dataPtr->getZVtxPtr(meshId),
                             dataPtr->getMeshSize(meshId));
    }

    // set absolute values
    void generateAccel(lpReal_t cellSize, lpReal_t searchDistance){
        accel->generateAccel(cellSize, searchDistance,
                             dataPtr->getXVtxPtr(meshId),
                             dataPtr->getYVtxPtr(meshId),
                             dataPtr->getZVtxPtr(meshId),
                             dataPtr->getMeshSize(meshId));
    }

    DEVCALLABLE
    void addForce(int tri, lpReal_t xForce,lpReal_t yForce,lpReal_t zForce){
        //
        Atomic::atomicAddVal(dataPtr->xFaceForce[tri], xForce);
        Atomic::atomicAddVal(dataPtr->yFaceForce[tri], yForce);
        Atomic::atomicAddVal(dataPtr->zFaceForce[tri], zForce);
    }

    void zeroForce(){
        int start = dataPtr->meshStarts[meshId];
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(int i, MeshData &mesh) {
            mesh.xFaceForce[start+i] = 0;
            mesh.yFaceForce[start+i] = 0;
            mesh.zFaceForce[start+i] = 0;
        }, dataPtr->getMeshSize(meshId), *dataPtr);
        Iterator::syncAll();
    }

    void reduceForces() {
        // mpi reduce for forces
        int size = dataPtr->getMeshSize(meshId);
        int start = dataPtr->meshStarts[meshId];
        MPI_Allreduce(MPI_IN_PLACE, &dataPtr->xFaceForce[start], size, getMPIType<lpReal_t>(), MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(MPI_IN_PLACE, &dataPtr->yFaceForce[start], size, getMPIType<lpReal_t>(), MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(MPI_IN_PLACE, &dataPtr->zFaceForce[start], size, getMPIType<lpReal_t>(), MPI_SUM, MPI_COMM_WORLD);
    }
};

#endif //PHELL_INPUTMESH_H
