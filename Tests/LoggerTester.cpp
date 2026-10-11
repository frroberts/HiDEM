//
// Created by Fredrik Robertsen on 01/03/2021.
//

#include <catch2/catch.hpp>

#include <Domain.h>
#include <World.h>

#include <Logger/EventCollection.h>

#include <thread>

TEST_CASE( "Test basic EventCollection setup", "[Logger]" ) {
    //

    EventCollection events;
    events.setTimestep(0);
    events.syncClocks();
    std::this_thread::sleep_for (std::chrono::milliseconds (100));
    events.startEvent("test");
    std::this_thread::sleep_for (std::chrono::milliseconds (200));
    events.endEvent("test");

    REQUIRE(events.eventHistory["test"].size() == 1);
    REQUIRE(events.eventHistory["test"].front().start.count() == Approx(0.1).margin(0.001));
    REQUIRE(events.eventHistory["test"].front().duration.count() == Approx(0.2).margin(0.001));
}


TEST_CASE( "Dump to HDF5", "[Logger]" ) {
    // todo implement
}