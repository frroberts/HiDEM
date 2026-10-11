//
// Created by Fredrik Robertsen on 5/23/24.
//


#ifndef PHELL_MESHDATA_H
#define PHELL_MESHDATA_H

#include "Geometry/MeshAccel.h"
#include "Util/Util.h"

class MeshData : public ManagedParent {

    lpReal_t xUpper;
    lpReal_t yUpper;
    lpReal_t zUpper;

    lpReal_t xLower;
    lpReal_t yLower;
    lpReal_t zLower;

public:
    ManVec<lpReal_t> xVtxCord;
    ManVec<lpReal_t> yVtxCord;
    ManVec<lpReal_t> zVtxCord;

    ManVec<lpReal_t> xNorm; // sized at 1/3rd
    ManVec<lpReal_t> yNorm; // sized at 1/3rd
    ManVec<lpReal_t> zNorm; // sized at 1/3rd

    ManVec<lpReal_t> xFaceForce; // sized at 1/3rd
    ManVec<lpReal_t> yFaceForce; // sized at 1/3rd
    ManVec<lpReal_t> zFaceForce; // sized at 1/3rd

    ManVec<lpReal_t> xMeshForce; // sized at 1 per mesh
    ManVec<lpReal_t> yMeshForce; // sized at 1 per mesh
    ManVec<lpReal_t> zMeshForce; // sized at 1 per mesh

    // the mesh that the triangle belongs to
    ManVec<int> triMesh; // sized at 1 per mesh

    int numTris = 0;
    int numMehses = 0;
    ManVec<int> meshStarts;
    ManVec<int> meshEnds;
    std::unordered_map<std::string, int> meshNames;

    MeshAccel *accel;

    MeshData(){
        // push on one mesh, for the whole mesh.
        numMehses = 1;
        meshStarts.push_back(0);
        meshEnds.push_back(0);
    }

    int addMesh(std::vector<Triangle> &faces, std::string &name) {
        // unpack faces, add space for forces
        meshStarts.push_back(numTris);
        meshNames[name] = numMehses;
        for (const auto &f: faces) {
            for (int i = 0; i < 3; i++) {
                xVtxCord.push_back(f.points[i].x);
                yVtxCord.push_back(f.points[i].y);
                zVtxCord.push_back(f.points[i].z);
            }
            xNorm.push_back(f.normal.x);
            yNorm.push_back(f.normal.y);
            zNorm.push_back(f.normal.z);

            xFaceForce.push_back(0);
            yFaceForce.push_back(0);
            zFaceForce.push_back(0);
            triMesh.push_back(numMehses);
            numTris++;
        }
        meshEnds.push_back(numTris);
        // set the end for the full mesh
        meshEnds[0] = numTris;

        xMeshForce.push_back(0);
        yMeshForce.push_back(0);
        zMeshForce.push_back(0);

        numMehses++;
        return meshNames[name];
    }

    void computeMeshForce() {
        // compute first reduce second ?
    }

    DEVCALLABLE
    bool inBB(lpReal_t xCord, lpReal_t yCord, lpReal_t zCord) {
        if (xCord > xUpper || yCord > yUpper || zCord > zUpper || xCord < xLower || yCord < yLower || zCord < zLower) {
            return false;
        }
        return true;
    }

    DEVCALLABLE
    lpReal_t * getXVtxPtr(int meshId){
        if(meshId == -1)
            return xVtxCord.data();
        return &xVtxCord[meshStarts[meshId]*3];
    }
    DEVCALLABLE
    lpReal_t * getYVtxPtr(int meshId){
        if(meshId == -1)
            return yVtxCord.data();
        return &yVtxCord[meshStarts[meshId]*3];
    }
    DEVCALLABLE
    lpReal_t * getZVtxPtr(int meshId){
        if(meshId == -1)
            return zVtxCord.data();
        return &zVtxCord[meshStarts[meshId]*3];
    }
    DEVCALLABLE
    int getMeshSize(int meshId){
        if(meshId == -1)
            return numTris;
        return meshEnds[meshId] - meshStarts[meshId];
    }






};

#endif //PHELL_MESHDATA_H
