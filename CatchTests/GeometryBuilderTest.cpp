//
// Created by Fredrik Robertsen on 27/02/2022.
//

#include <catch2/catch.hpp>

#include <Geometry/GeometryCollection.h>

TEST_CASE( "Bounding string parsing", "[GeometryCollection]" ) {
    //


}

TEST_CASE( "Compound generation 2", "[GeometryCollection]" ) {
    //

    GeometryCollection builder;

    builder.allGeometries["sea"] = std::make_shared<InputBox, Real3, Real3>({0}, {2});
    builder.allGeometries["glacier"] = std::make_shared<InputBox, Real3, Real3>({0}, {1});

    auto geom = builder.generateCompound("sea NOT glacier");

    REQUIRE(!geom->withinGeometry({0.5, 0.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({1.5, 0.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({0.5, 1.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({0.5, 0.5, 1.5},0.0));

    REQUIRE(geom->withinGeometry({1.5, 1.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({1.5, 0.5, 1.5},0.0));
    REQUIRE(geom->withinGeometry({1.5, 1.5, 1.5},0.0));
    REQUIRE(geom->withinGeometry({0.5, 1.5, 1.5},0.0));
}

TEST_CASE("Compound generation 3", "[GeometryCollection]") {
    //

    GeometryCollection builder;

    builder.allGeometries["sea"] = std::make_shared<InputBox, Real3, Real3>({0}, {2});
    builder.allGeometries["glacier"] = std::make_shared<InputBox, Real3, Real3>({0}, {1});
    builder.allGeometries["glacier2"] = std::make_shared<InputBox, Real3, Real3>({1}, {2});

    auto geom = builder.generateCompound("sea NOT glacier NOT glacier2");

    REQUIRE(!geom->withinGeometry({0.5, 0.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({1.5, 0.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({0.5, 1.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({0.5, 0.5, 1.5},0.0));

    REQUIRE(geom->withinGeometry({1.5, 1.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({1.5, 0.5, 1.5},0.0));
    REQUIRE(!geom->withinGeometry({1.5, 1.5, 1.5},0.0));
    REQUIRE(geom->withinGeometry({0.5, 1.5, 1.5},0.0));
}

TEST_CASE("Compound generation 4", "[GeometryCollection]") {
    //

    GeometryCollection builder;

    builder.allGeometries["glacier"] = std::make_shared<InputBox, Real3, Real3>({0}, {1});
    builder.allGeometries["glacier2"] = std::make_shared<InputBox, Real3, Real3>({1}, {2});

    auto geom = builder.generateCompound("glacier OR glacier2");

    REQUIRE(geom->withinGeometry({0.5, 0.5, 0.5},0.0));
    REQUIRE(!geom->withinGeometry({1.5, 0.5, 0.5},0.0));
    REQUIRE(!geom->withinGeometry({0.5, 1.5, 0.5},0.0));
    REQUIRE(!geom->withinGeometry({0.5, 0.5, 1.5},0.0));

    REQUIRE(!geom->withinGeometry({1.5, 1.5, 0.5},0.0));
    REQUIRE(!geom->withinGeometry({1.5, 0.5, 1.5},0.0));
    REQUIRE(geom->withinGeometry({1.5, 1.5, 1.5},0.0));
    REQUIRE(!geom->withinGeometry({0.5, 1.5, 1.5},0.0));
}


TEST_CASE("Compound generation parenthesis", "[GeometryCollection]") {
    //

    GeometryCollection builder;

    builder.allGeometries["sea"] = std::make_shared<InputBox, Real3, Real3>({0}, {2});
    builder.allGeometries["glacier"] = std::make_shared<InputBox, Real3, Real3>({0}, {1});
    builder.allGeometries["glacier2"] = std::make_shared<InputBox, Real3, Real3>({1}, {2});

    auto geom = builder.generateCompound("sea NOT (glacier OR glacier2)");

    REQUIRE(!geom->withinGeometry({0.5, 0.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({1.5, 0.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({0.5, 1.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({0.5, 0.5, 1.5},0.0));

    REQUIRE(geom->withinGeometry({1.5, 1.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({1.5, 0.5, 1.5},0.0));
    REQUIRE(!geom->withinGeometry({1.5, 1.5, 1.5},0.0));
    REQUIRE(geom->withinGeometry({0.5, 1.5, 1.5},0.0));
}

TEST_CASE("Compound generation parenthesis 2", "[GeometryCollection]") {
    //

    GeometryCollection builder;

    builder.allGeometries["sea"] = std::make_shared<InputBox, Real3, Real3>({0}, {2});
    builder.allGeometries["glacier"] = std::make_shared<InputBox, Real3, Real3>({0}, {1});
    builder.allGeometries["glacier2"] = std::make_shared<InputBox, Real3, Real3>({1}, {2});

    auto geom = builder.generateCompound("(glacier OR glacier2) NOT sea");

    REQUIRE(!geom->withinGeometry({0.5, 0.5, 0.5},0.0));
    REQUIRE(!geom->withinGeometry({1.5, 0.5, 0.5},0.0));
    REQUIRE(!geom->withinGeometry({0.5, 1.5, 0.5},0.0));
    REQUIRE(!geom->withinGeometry({0.5, 0.5, 1.5},0.0));

    REQUIRE(!geom->withinGeometry({1.5, 1.5, 0.5},0.0));
    REQUIRE(!geom->withinGeometry({1.5, 0.5, 1.5},0.0));
    REQUIRE(!geom->withinGeometry({1.5, 1.5, 1.5},0.0));
    REQUIRE(!geom->withinGeometry({0.5, 1.5, 1.5},0.0));
}

TEST_CASE("Compound generation 3 boxes", "[GeometryCollection]") {
    //

    GeometryCollection builder;

    builder.allGeometries["sea"] = std::make_shared<InputBox, Real3, Real3>({0}, {3});
    builder.allGeometries["glacier"] = std::make_shared<InputBox, Real3, Real3>({0}, {1,1,1});
    builder.allGeometries["glacier2"] = std::make_shared<InputBox, Real3, Real3>({1,0,0}, {2,1,1});
    builder.allGeometries["glacier3"] = std::make_shared<InputBox, Real3, Real3>({2,0,0}, {3,1,1});

    auto geom = builder.generateCompound("sea NOT glacier NOT glacier2 NOT glacier3");

    REQUIRE(!geom->withinGeometry({0.5, 0.5, 0.5},0.0));
    REQUIRE(!geom->withinGeometry({1.5, 0.5, 0.5},0.0));
    REQUIRE(!geom->withinGeometry({2.5, 0.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({2.5, 1.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({1.5, 1.5, 0.5},0.0));
}

TEST_CASE("Compound generation meshes", "[GeometryCollection]") {
    //

    GeometryCollection builder;

    builder.allGeometries["sea"] = std::make_shared<InputBox, Real3, Real3>({0}, {3});
    //builder.allGeometries["glacier"] = std::make_shared<InputBox, Real3, Real3>({0}, {1,1,1});
    //builder.allGeometries["glacier2"] = std::make_shared<InputBox, Real3, Real3>({1,0,0}, {2,1,1});
    //builder.allGeometries["glacier3"] = std::make_shared<InputBox, Real3, Real3>({2,0,0}, {3,1,1});

    auto m1 = builder.createMesh("glacier", "../Samples/cube.obj", Real3(1), Real3(0), 1, builder.collisionMeshData);
    builder.allGeometries["glacier"] = m1;
    m1->generateAccel(0.5, 0.2);


    auto m2 = builder.createMesh("glacier2", "../Samples/cube.obj", Real3(1), Real3(1,0,0), 1, builder.collisionMeshData);
    builder.allGeometries["glacier2"] = m2;
    m2->generateAccel(0.5, 0.2);


    auto m3 = builder.createMesh("glacier3", "../Samples/cube.obj", Real3(1), Real3(2,0,0), 1, builder.collisionMeshData);
    builder.allGeometries["glacier3"] = m3;
    m3->generateAccel(0.5, 0.2);


    auto geom = builder.generateCompound("sea NOT glacier NOT glacier2 NOT glacier3");

    REQUIRE(!geom->withinGeometry({0.5, 0.5, 0.5},0.0));
    REQUIRE(!geom->withinGeometry({1.5, 0.5, 0.5},0.0));
    REQUIRE(!geom->withinGeometry({2.5, 0.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({2.5, 1.5, 0.5},0.0));
    REQUIRE(geom->withinGeometry({1.5, 1.5, 0.5},0.0));
}




