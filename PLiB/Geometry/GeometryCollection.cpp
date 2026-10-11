//
// Created by Fredrik Robertsen on 26/02/2022.
//

#include <queue>

#include "GeometryCollection.h"
#include "CompoundInput.h"
#include "BoundingMesh.h"


std::shared_ptr<InputGeometry> GeometryCollection::generateCompound(std::string inputStr) {

    std::regex words_regex(R"(\w+|\(|\))");
    auto reTolkens =
            std::sregex_iterator(inputStr.begin(), inputStr.end(), words_regex);

    std::queue<std::string> tolkens;

    for (std::sregex_iterator i = reTolkens; i != std::sregex_iterator(); ++i) {
        tolkens.push(i->str());
    }

    return parseTolkens(tolkens);
}

std::shared_ptr<InputGeometry> GeometryCollection::getCompound(std::shared_ptr<InputGeometry> aArg, std::shared_ptr<InputGeometry> bArg, std::string op) {

    // std::cout << " op " << op << std::endl;

    if (op == "AND") {
        return std::make_shared<InputAND>(aArg, bArg);
    } else if (op == "OR") {
        return std::make_shared<InputOR>(aArg, bArg);
    } else if (op == "NOT") {
        return std::make_shared<InputNOT>(aArg, bArg);
    } else {
        std::cout << "malformed bounding expression" << std::endl;
        exit(1);
    }

    return nullptr;
}

std::shared_ptr<InputGeometry> GeometryCollection::parseTolkens(std::queue<std::string> &tolkens) {

    // first tolken should be the first argument
    std::string tolken = tolkens.front();
    tolkens.pop();
    std::shared_ptr<InputGeometry> baseGeom;
    if(tolken == "(")
    {
        baseGeom = parseTolkens(tolkens);
    } else {
        if(allGeometries.find(tolken) != allGeometries.end()){
            baseGeom = allGeometries[tolken];
        } else {
            std::cout << "failed to find geometry " << tolken << std::endl;
            exit(1);
        }
    }

    // next call the recurse function for the rest of the tokens
    if(!tolkens.empty())
    baseGeom = recurseTolkens(baseGeom, tolkens);

    return baseGeom;
}

std::shared_ptr<InputGeometry> GeometryCollection::recurseTolkens(
        std::shared_ptr<InputGeometry> base, std::queue<std::string> &tolkens) {
    // get the op
    std::string op = tolkens.front();
    tolkens.pop();
    // if the operation is ) we have reached the end of a () part in the expression, return the constructed bed
    // and dont look at the next tolken
    if (op == ")") {
        return base;
    }

    // get the next tolken, should be the next argument or the start of ()
    std::string argStr = tolkens.front();
    // std::cout << " argStr " << argStr << std::endl;
    tolkens.pop();
    // if arg is ( redo the start i.e. run parseTolkens
    std::shared_ptr<InputGeometry> arg;
    if (argStr == "(") {
        // if it is the start of ( start a new parsing of what is inside ()
        arg = parseTolkens(tolkens);
    } else {
        // if not get the corresponding shared pointer
        arg = allGeometries[argStr];
    }

    // construct the compound input
    base = getCompound(base, arg, op);

    // if tolkens are empty return the bed
    if (tolkens.empty())
        return base;
    else
        // if not recurse to the next set fo tolkens
        return recurseTolkens(base, tolkens);
}

std::shared_ptr<InputDEM> GeometryCollection::createDem(Map2d<lpReal_t>* base, Map2d<lpReal_t>* surface) {
    auto geom = std::make_shared<InputDEM>();

    geom->surface = surface;
    geom->base = base;
    return geom;
}

void GeometryCollection::fillMesh(Mesh* geom, std::string name, std::string objPath, Real3 scale, Real3 position,
                                  MeshData *meshData, bool flipNormal) {

    auto triangles = MeshLoader::loadObj(objPath);

    // Validate mesh, check that it is airtight


    for (auto &t: triangles) {
        t.autoNormal();
        if (flipNormal)
            t.normal = t.normal * Real3(-1);
        for (auto &p: t.points) {
            p = p * scale;
            p = p + position;
        }
    }

    //geom->cellSizeScale = meshCellSizeScale;
    //geom->rangeFactor = meshRangeFactor;
    // geom->outerShell = outerShell;

    geom->meshId = meshData->addMesh(triangles, name);
    // geom->shellScale = shellScale;

    geom->dataPtr = meshData;
    geom->accel = new MeshAccel();

}

std::shared_ptr<InputBox> GeometryCollection::createBox(Real3 lower, Real3 upper) {

    auto geom = std::make_shared<InputBox>(lower, upper);

    return geom;
}

std::shared_ptr<InputBed> GeometryCollection::createBed(Map2d<lpReal_t>* bed, Map2d<lpReal_t>* friction) {
    auto rawPtr = new InputBed;
    auto geom = std::shared_ptr<InputBed>(rawPtr);

    geom->friction = friction;
    geom->bed = bed;

    return geom;
}

void GeometryCollection::buildFromConfig(ArgParser &parser) {

    // for each entry in [[geometry]] parse it and generate it
    //int numGeometry = tomlTable["geometry"].as_array()->size();
    const auto geometryTable = toml::find<std::vector<toml::table>>(parser.table, "geometry");
    parseGeometryTable(geometryTable);

    // these get defaulted if you dont specify it, not the best
    meshCellSizeScale = parser.getArg<double>("collisionMeshParameters.cellSizeScale").value_or(4.0);
    meshRangeFactor = parser.getArg<double>("collisionMeshParameters.rangeFactor").value_or(1.5);

    if(parser.table.contains("collisionGeometry")) {
        // build base geometries

        const std::vector<toml::table> collisionGeomTable = toml::find<std::vector<toml::table>>(parser.table, "collisionGeometry");
        parseCollisionTable(collisionGeomTable);
    }

    // generate mesh acceleration structs
}

void GeometryCollection::setOffset(Real3 offset) {
    for (auto&t: allGeometries) {
        t.second->setOffset(offset);
    }
}

void GeometryCollection::parseGeometryTable(std::vector<toml::table> geometryTable) {
    for(toml::value tomlBase: geometryTable){

        std::string type{ toml::find<std::string>(tomlBase, "type") };//*tomlBase["type"].as_string()};
        std::string name{ toml::find<std::string>(tomlBase, "name") };//*tomlBase["name"].as_string()};

        if (type == "box") {
            Real3 lower = TomlParser::getReal3(tomlBase, "lower");
            Real3 upper = TomlParser::getReal3(tomlBase, "upper");

            auto geom = createBox(lower, upper);
            allGeometries[name] = geom;
        } else if (type == "dem") {
            std::string path{toml::find<std::string>(tomlBase, "path") };//*tomlBase["path"].as_string()};
            addMapLoader(path);
            auto top = mapLoaders[path].get<lpReal_t>(toml::find<int>(tomlBase, "topFieldID"));//tomlBase["topFieldID"].value<>().value());
            auto bottom = mapLoaders[path].get<lpReal_t>(toml::find<int>(tomlBase, "baseFieldID"));//tomlBase["baseFieldID"].value<>().value());
            auto geom = createDem(bottom, top);
            allGeometries[name] = geom;
            //
        } else if (type == "mesh") {
            //
            std::string path{toml::find<std::string>(tomlBase, "path") };//*tomlBase["path"].as_string()};
            Real3 scale = TomlParser::getReal3Opt(tomlBase, "scale", {1});
            Real3 pos = TomlParser::getReal3Opt(tomlBase, "position", {0});
            lpReal_t shellScale = toml::find<float>(tomlBase, "shellScale");
            bool outerShell = true;
            if (shellScale < 0) {
                shellScale = abs(shellScale);
                outerShell = false;
            }
            std::shared_ptr<BoundingMesh> geom = std::make_shared<BoundingMesh>();

            fillMesh(geom.get(), name, path, scale, pos, geometryMeshData, false);

            geom->outerShell = outerShell;
            geom->shellScale = shellScale;

            allGeometries[name] = geom;
            boundingMeshes[name] = geom;
        } else if (type == "plane") {
            //
        } else if (type == "bed") {
            std::string path{toml::find<std::string>(tomlBase, "path") };//*tomlBase["path"].as_string()};
            addMapLoader(path);
            auto friction = mapLoaders[path].get<lpReal_t>(toml::find<int>(tomlBase, "frictionFieldID"));//tomlBase["frictionFieldID"].value<>().value());
            auto bottom = mapLoaders[path].get<lpReal_t>(toml::find<int>(tomlBase, "baseFieldID"));//tomlBase["baseFieldID"].value<>().value());
            auto geom = createBed(bottom, friction);
            allGeometries[name] = geom;
            bed = geom;
        } else {
            std::cout << "unknown geometry type" << std::endl;
            exit(1);
        }
    }
}



void GeometryCollection::parseCollisionTable(std::vector<toml::table> geometryTable) {
    for(toml::value tomlBase: geometryTable){

        std::string type{ toml::find<std::string>(tomlBase, "type") };//*tomlBase["type"].as_string()};
        std::string name{ toml::find<std::string>(tomlBase, "name") };//*tomlBase["name"].as_string()};
        
        if (type == "mesh") {
            //
            std::string path{toml::find<std::string>(tomlBase, "path") };//*tomlBase["path"].as_string()};
            Real3 scale = TomlParser::getReal3Opt(tomlBase, "scale", {1});
            Real3 pos = TomlParser::getReal3Opt(tomlBase, "position", {0});
            lpReal_t shellScale = toml::find_or<float>(tomlBase, "shellScale", 0.f);
            // needs to be optional
            bool flipNormal = toml::find_or<bool>(tomlBase, "flipNormal", false);
            std::cout << path << " " << flipNormal << std::endl;
            bool outerShell = true;
            if (shellScale < 0) {
                shellScale = abs(shellScale);
                outerShell = false;
            }

            std::shared_ptr<BoundingMesh> boundingGeom = std::make_shared<BoundingMesh>();
            std::shared_ptr<CollisionMesh> collisionGeom = std::make_shared<CollisionMesh>();

            fillMesh(boundingGeom.get(), name, path, scale, pos, geometryMeshData, false);
            fillMesh(collisionGeom.get(), name, path, scale, pos, collisionMeshData, flipNormal);
            boundingGeom->outerShell = outerShell;
            boundingGeom->shellScale = shellScale;

            allGeometries[name] = boundingGeom;
            boundingMeshes[name] = boundingGeom;
            collisionMeshes[name] = collisionGeom;
        } else if (type == "plane") {
            //
        } else if (type == "bed") {
            std::string path{toml::find<std::string>(tomlBase, "path") };//*tomlBase["path"].as_string()};
            addMapLoader(path);
            auto friction = mapLoaders[path].get<lpReal_t>(toml::find<int>(tomlBase, "frictionFieldID"));//tomlBase["frictionFieldID"].value<>().value());
            auto bottom = mapLoaders[path].get<lpReal_t>(toml::find<int>(tomlBase, "baseFieldID"));//tomlBase["baseFieldID"].value<>().value());
            auto geom = createBed(bottom, friction);
            allGeometries[name] = geom;
            bed = geom;
        } else {
            std::cout << "unknown collision geometry type" << std::endl;
            exit(1);
        }
    }
}

// to not load the same map multiple times
void GeometryCollection::addMapLoader(std::string path) {

    auto pos = mapLoaders.find(path);
    if(pos == mapLoaders.end()) {
        mapLoaders[path] = Map2dLoader();

        mapLoaders[path].readInput(path);
    }
}

void GeometryCollection::generatedAllAccel(lpReal_t maxRadius){
    /*
    for (auto p: collisionMeshes){
        p.second->generateAccel(maxRadius);
        p.second->accel->flattenZ();
    }
    */
    for (auto p: boundingMeshes){
        p.second->generateAccel(maxRadius);
        p.second->accel->flattenZ();
    }
}



