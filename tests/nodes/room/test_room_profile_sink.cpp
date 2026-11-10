/**************************************************************************/
/* test_room_profile_sink.cpp                                             */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/


/**
 * @file test_room_profile_sink.cpp
 * @brief Tests that the in-memory profile sink counts the entries and exits of each zone by name.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/room_profile_sink.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace
{
    using SushiDSP::room::InMemoryRoomProfileSink;

    /// @brief Holds one more distinct zone name than the sink's table holds.
    const std::array<const char*, InMemoryRoomProfileSink::K_ZONE_CAPACITY + 1> K_NINE_ZONES = {
        "zone.0", "zone.1", "zone.2", "zone.3", "zone.4", "zone.5", "zone.6", "zone.7", "zone.8"};

    /// @brief Enters a zone the given number of times.
    void enter_times(InMemoryRoomProfileSink& sink, const char* zone, std::uint32_t times)
    {
        for (std::uint32_t entry = 0; entry < times; ++entry)
        {
            sink.enter(zone);
        }
    }

    /// @brief Enters every zone of the table once.
    void enter_each(InMemoryRoomProfileSink& sink)
    {
        for (const char* zone : K_NINE_ZONES)
        {
            sink.enter(zone);
        }
    }
} // namespace

TEST(Small_InMemoryRoomProfileSink, CountsEachEntryOfAZone)
{
    InMemoryRoomProfileSink sink;

    enter_times(sink, "room.tail", 3);

    EXPECT_EQ(sink.entered("room.tail"), 3u);
}

TEST(Small_InMemoryRoomProfileSink, CountsEachExitOfAZoneApartFromItsEntries)
{
    InMemoryRoomProfileSink sink;
    enter_times(sink, "room.tail", 3);

    sink.leave("room.tail");

    EXPECT_EQ(sink.left("room.tail"), 1u);
    EXPECT_EQ(sink.entered("room.tail"), 3u);
}

TEST(Small_InMemoryRoomProfileSink, ReportsZeroForAZoneItNeverSaw)
{
    InMemoryRoomProfileSink sink;
    enter_times(sink, "room.tail", 1);

    EXPECT_EQ(sink.entered("room.modal"), 0u);
    EXPECT_EQ(sink.left("room.modal"), 0u);
}

TEST(Small_InMemoryRoomProfileSink, KeepsTwoZonesApart)
{
    InMemoryRoomProfileSink sink;

    enter_times(sink, "room.tail", 2);
    enter_times(sink, "room.modal", 5);

    EXPECT_EQ(sink.entered("room.tail"), 2u);
    EXPECT_EQ(sink.entered("room.modal"), 5u);
}

TEST(Small_InMemoryRoomProfileSink, JoinsTwoNamesOfTheSameTextAtDifferentAddresses)
{
    InMemoryRoomProfileSink sink;
    const std::array<char, 5> first = {'z', 'o', 'n', 'e', '\0'};
    const std::array<char, 5> second = {'z', 'o', 'n', 'e', '\0'};

    sink.enter(first.data());
    sink.enter(second.data());

    EXPECT_EQ(sink.entered("zone"), 2u);
}

TEST(Small_InMemoryRoomProfileSink, DropsTheZonePastItsCapacityAndKeepsTheOthers)
{
    InMemoryRoomProfileSink sink;

    enter_each(sink);

    EXPECT_EQ(sink.entered("zone.8"), 0u);
    EXPECT_EQ(sink.entered("zone.7"), 1u);
}
