//
// Created by Fredrik Robertsen on 25/02/2022.
//

//
#include <toml++/toml.h>


#include <catch2/catch.hpp>

#include <iostream>

TEST_CASE("testing multi level toml parse", "[toml]") {
    //

    auto config = toml::parse(R"(
[[type1]]
name = "name1"

[[type1]]
name = "name2"
)");

    std::cout << config["type1"].as_array()->size() << "\n";
    std::cout << config["type1"][0]["name"] << "\n";

}

TEST_CASE("testing inserting new values", "[toml]") {
    //

    auto config = toml::parse(R"(
[base]
name = "name1"

)");

    config.find("base")->second.as_table()->insert_or_assign("name", "name2");

    std::cout << config << std::endl;

}