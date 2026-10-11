//
// Created by Fredrik Robertsen on 21/02/2022.
//


#include <catch2/catch.hpp>

#include <Geometry/MeshLoader.h>
#include <Geometry/InputGeometry.h>
#include <Geometry/BoundingMesh.h>
#include "Geometry/GeometryCollection.h"


TEST_CASE( "Test loading basic mesh from collection", "[Mesh]" ) {
    //

    GeometryCollection collection;

    auto m1 = collection.createMesh("mi", "../CatchTests/cube.obj", Real3(1), Real3(0), 1, collection.collisionMeshData);
    REQUIRE(m1->dataPtr->numTris == 12);
    m1->generateAccel(0.5, 0.2);

    // mesh IDs start from 1, the full collision mesh is id 0
    REQUIRE(m1->meshId == 1);


    REQUIRE(m1->withinGeometry(Real3(0.01,0.01,0.01),0.1) == true);
    REQUIRE(m1->withinGeometry(Real3(1,1,1),0.1) == true);
    // REQUIRE(m1->withinGeometry(Real3(1.51,1.51,1.51),0.1) == true);
    REQUIRE(m1->withinGeometry(Real3(1.61,1.61,1.61),0.1) == false);
    REQUIRE(m1->withinGeometry(Real3(2,2,2),0.1) == false);
    REQUIRE(m1->withinGeometry(Real3(5,5,5),0.1) == false);

    REQUIRE(m1->withinGeometry(Real3(0.01,0.01,0.01),0.1) == true);
    REQUIRE(m1->withinGeometry(Real3(0.5,0.01,0.01),0.1) == true);
    REQUIRE(m1->withinGeometry(Real3(1,0.01,0.01),0.1) == true);
    REQUIRE(m1->withinGeometry(Real3(1.5,0.01,0.01),0.1) == false);
    REQUIRE(m1->withinGeometry(Real3(2,0.01,0.01),0.1) == false);
    REQUIRE(m1->withinGeometry(Real3(3,0.01,0.01),0.1) == false);
    REQUIRE(m1->withinGeometry(Real3(4,0.01,0.01),0.1) == false);
    REQUIRE(m1->withinGeometry(Real3(5,0.01,0.01),0.1) == false);



}

TEST_CASE( "Test loading basic mesh from full mesh", "[Mesh]" ) {
    //

    GeometryCollection collection;

    auto m1 = collection.createMesh("mi", "../CatchTests/cube.obj", Real3(1), Real3(0), 1, collection.collisionMeshData);
    REQUIRE(m1->dataPtr->numTris == 12);
    m1->generateAccel(0.5, 0.2);

    auto m2 = collection.getColMeshData().value();
    m2->generateAccel(0.5, 0.2);

    // mesh IDs start from 1, the full collision mesh is id 0
    REQUIRE(m2->meshId == 0);
    bool hit = false;
    m2->closestTriFun(0,0,0,2.0, 0, [&](int tri, lpReal_t dist){
        if(dist <= 1.0)
            hit =true;
    });
    REQUIRE(hit);


    REQUIRE(m2->withinGeometry(Real3(0.01,0.01,0.01),0.1) == true);
    REQUIRE(m2->withinGeometry(Real3(1,1,1),0.1) == true);
    // REQUIRE(m2->withinGeometry(Real3(1.51,1.51,1.51),0.1) == true);
    REQUIRE(m2->withinGeometry(Real3(1.61,1.61,1.61),0.1) == false);
    REQUIRE(m2->withinGeometry(Real3(2,2,2),0.1) == false);
    REQUIRE(m2->withinGeometry(Real3(5,5,5),0.1) == false);

    REQUIRE(m2->withinGeometry(Real3(0.01,0.01,0.01),0.1) == true);
    REQUIRE(m2->withinGeometry(Real3(0.5,0.01,0.01),0.1) == true);
    REQUIRE(m2->withinGeometry(Real3(1,0.01,0.01),0.1) == true);
    REQUIRE(m2->withinGeometry(Real3(1.5,0.01,0.01),0.1) == false);
    REQUIRE(m2->withinGeometry(Real3(2,0.01,0.01),0.1) == false);
    REQUIRE(m2->withinGeometry(Real3(3,0.01,0.01),0.1) == false);
    REQUIRE(m2->withinGeometry(Real3(4,0.01,0.01),0.1) == false);
    REQUIRE(m2->withinGeometry(Real3(5,0.01,0.01),0.1) == false);


}