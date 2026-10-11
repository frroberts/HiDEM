//
// Created by Fredrik Robertsen on 15/11/25.
//

#ifndef PHELL_DYNAMICMESH_H
#define PHELL_DYNAMICMESH_H

#include "InputGeometry.h"
#include "../MeshData.h"
#include "MeshAccel.h"
#include "../Util/Util.h"
#include <AtomicOps.h>

#include "../Util/MPIHelpers.h"
#include "Mesh.h"


class DynamicMesh : public CollisionMesh {

public:

    lpReal_t xForce;
    lpReal_t yForce;
    lpReal_t zForce;
    lpReal_t uForce;
    lpReal_t vForce;
    lpReal_t wForce;

    DynamicMesh() {

    }

    ~DynamicMesh() {
        //
    }

    // with scale from the mesh
    virtual void generateAccel(lpReal_t radius){

        accel->generateAccel(std::max(radius*shellScale, radius), std::max(radius*shellScale, radius),
                             dataPtr->getXVtxPtr(meshId),
                             dataPtr->getYVtxPtr(meshId),
                             dataPtr->getZVtxPtr(meshId),
                             dataPtr->getMeshSize(meshId));
    }

    DEVCALLABLE
    virtual void addForceToMesh(int tri, lpReal_t xForce,lpReal_t yForce,lpReal_t zForce){
        //
        Atomic::atomicAddVal(dataPtr->xFaceForce[tri], xForce);
        Atomic::atomicAddVal(dataPtr->yFaceForce[tri], yForce);
        Atomic::atomicAddVal(dataPtr->zFaceForce[tri], zForce);
    }

    DEVCALLABLE
    virtual void addForceToFace(int tri, lpReal_t xForce,lpReal_t yForce,lpReal_t zForce){
        // expand for rotational forces 
        Atomic::atomicAddVal(this->xForce[tri], xForce);
        Atomic::atomicAddVal(this->yForce[tri], yForce);
        Atomic::atomicAddVal(this->zForce[tri], zForce);
    }

    virtual void zeroForce(){
        int start = dataPtr->meshStarts[meshId];
        Iterator::iterate(OffloadExec(), [=] DEVCALLABLE(int i, MeshData &mesh) {
            mesh.xFaceForce[start+i] = 0;
            mesh.yFaceForce[start+i] = 0;
            mesh.zFaceForce[start+i] = 0;
        }, dataPtr->getMeshSize(meshId), *dataPtr);
        Iterator::syncAll();
    }

    virtual void reduceForces() {
        // mpi reduce for forces
        int size = dataPtr->getMeshSize(meshId);
        int start = dataPtr->meshStarts[meshId];
        MPI_Allreduce(MPI_IN_PLACE, &dataPtr->xFaceForce[start], size, getMPIType<lpReal_t>(), MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(MPI_IN_PLACE, &dataPtr->yFaceForce[start], size, getMPIType<lpReal_t>(), MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(MPI_IN_PLACE, &dataPtr->zFaceForce[start], size, getMPIType<lpReal_t>(), MPI_SUM, MPI_COMM_WORLD);
    }
};

#endif //PHELL_DYNAMICMESH_H
