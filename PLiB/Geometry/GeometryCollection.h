//
// Created by Fredrik Robertsen on 26/02/2022.
//

#ifndef PHELL_GEOMETRYCOLLECTION_H
#define PHELL_GEOMETRYCOLLECTION_H

#include <queue>

#include <toml.hpp>

#include "Map2dLoader.h"
#include "InputGeometry.h"
#include "CollisionMesh.h"
#include "BoundingMesh.h"
#include "../MeshData.h"
#include "../Util/ArgParser.h"
#include "../Util/TomlParser.h"

class GeometryCollection {

    std::shared_ptr<InputGeometry> getCompound(std::shared_ptr<InputGeometry> aArg, std::shared_ptr<InputGeometry> bArg, std::string op);
    std::shared_ptr<InputGeometry> parseTolkens(std::queue<std::string> &tolkens);
    std::shared_ptr<InputGeometry> recurseTolkens(std::shared_ptr<InputGeometry> base, std::queue<std::string> &tolkens);

    // make optional
    std::optional<std::shared_ptr<InputBed>> bed;
    // read here from config then set in every mesh created
public:
    GeometryCollection(){
        collisionMeshData = new MeshData();
        geometryMeshData = new MeshData();
    }
    ~GeometryCollection() {
        delete collisionMeshData;
        delete geometryMeshData;
    }
    lpReal_t meshCellSizeScale = 1;
    lpReal_t meshRangeFactor = 1;

    MeshData *collisionMeshData;
    MeshData *geometryMeshData;
    std::map<std::string, Map2dLoader> mapLoaders;
    std::map<std::string, std::shared_ptr<BoundingMesh>> boundingMeshes;
    std::map<std::string, std::shared_ptr<CollisionMesh>> collisionMeshes;
    std::map<std::string, std::shared_ptr<InputGeometry>> allGeometries;
    // should return the geometry once done
    std::shared_ptr<InputGeometry> generateCompound(std::string inputStr);
    std::shared_ptr<InputDEM> createDem(Map2d<lpReal_t> *base, Map2d<lpReal_t> *surface);

    std::shared_ptr<InputBed> createBed(Map2d<lpReal_t> *bed, Map2d<lpReal_t> *friction);

    void fillMesh(Mesh *geom, std::string name, std::string objPath, Real3 scale, Real3 position, MeshData *meshData, bool flipNormal);

    std::shared_ptr<InputBox> createBox(Real3 lower, Real3 upper);

    void addMapLoader(std::string path);

    void buildFromConfig(ArgParser &parser);

    void setOffset(Real3 offset);

    void parseGeometryTable(std::vector<toml::table> geometry);

    void parseCollisionTable(std::vector<toml::table> geometry);

    std::optional<std::shared_ptr<InputBed>> getBed(){
        return bed;
    }
    std::optional<MeshData*> getMeshData() {
        return collisionMeshData;
    }

    std::optional<CollisionMesh *> getColMeshData() {
        if (collisionMeshData->numMehses <= 1)
            return {};
        CollisionMesh *mesh = new CollisionMesh();

        mesh->dataPtr = collisionMeshData;
        mesh->meshId = 0;
        // mesh->cellSizeScale = meshCellSizeScale;
        // mesh->rangeFactor = meshRangeFactor;
        mesh->accel = new MeshAccel();

        return mesh;
    }

    void generatedAllAccel(lpReal_t maxRadius);
};


#endif //PHELL_GEOMETRYCOLLECTION_H
