/**************************************************************************/
/* test_room_layout_codec.cpp                                             */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file test_room_layout_codec.cpp
 * @brief Tests the JSON codec for the cabinet section's layout.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "room_layout_codec.hpp"

namespace
{
    using SushiDSP::room::PolarPattern;

    /// @brief Builds a placement from a position and a yaw.
    RoomPlacement place_at(double x, double y, double z, double yaw)
    {
        RoomPlacement place;
        place.position = {x, y, z};
        place.yaw_rad = yaw;
        return place;
    }

    /// @brief Builds a layout with a moved listener, two cabinets and two microphones, the first
    /// microphone removed so the ids start above 1.
    RoomLayout full_layout()
    {
        RoomLayout layout;
        std::string reason;
        layout.set_listener(place_at(2.0, 3.0, 1.2, 0.5));
        layout.set_cabinet(0, place_at(1.0, 1.0, 0.4, 0.1), reason);
        layout.set_cabinet(1, place_at(1.5, 1.0, 0.4, -0.2), reason);
        MicrophoneSpot spot;
        const std::uint32_t dropped = layout.add_microphone(spot, reason);
        spot.place = place_at(1.1, 1.2, 0.5, 3.0);
        spot.pattern = PolarPattern::Supercardioid;
        layout.add_microphone(spot, reason);
        spot.place = place_at(2.5, 2.5, 2.2, -1.0);
        spot.pattern = PolarPattern::FigureEight;
        spot.room_mic = true;
        layout.add_microphone(spot, reason);
        layout.remove_microphone(dropped);
        return layout;
    }

    /// @brief Checks two placements match exactly.
    void expect_place(const RoomPlacement& a, const RoomPlacement& b)
    {
        EXPECT_EQ(a.position.x, b.position.x);
        EXPECT_EQ(a.position.y, b.position.y);
        EXPECT_EQ(a.position.z, b.position.z);
        EXPECT_EQ(a.yaw_rad, b.yaw_rad);
    }
}

TEST(Unit_DspRoomLayoutCodec, RoundTripKeepsListenerCabinetsAndMicrophones)
{
    const RoomLayout layout = full_layout();
    const RoomLayoutSnapshot back = room_layout_from_json(room_layout_to_json(layout, false));

    expect_place(back.layout.listener(), layout.listener());
    ASSERT_EQ(back.layout.cabinets().size(), layout.cabinets().size());
    for (std::size_t k = 0; k < layout.cabinets().size(); ++k)
    {
        expect_place(back.layout.cabinets()[k], layout.cabinets()[k]);
    }
    ASSERT_EQ(back.layout.microphones().size(), layout.microphones().size());
    for (std::size_t m = 0; m < layout.microphones().size(); ++m)
    {
        const MicrophoneSpot& a = back.layout.microphones()[m];
        const MicrophoneSpot& b = layout.microphones()[m];
        EXPECT_EQ(a.id, b.id);
        expect_place(a.place, b.place);
        EXPECT_EQ(a.pattern, b.pattern);
        EXPECT_EQ(a.room_mic, b.room_mic);
    }
}

TEST(Unit_DspRoomLayoutCodec, RoomOpenSurvivesRoundTrip)
{
    const RoomLayout layout = full_layout();
    EXPECT_TRUE(room_layout_from_json(room_layout_to_json(layout, true)).room_open);
    EXPECT_FALSE(room_layout_from_json(room_layout_to_json(layout, false)).room_open);
}

TEST(Unit_DspRoomLayoutCodec, RefusesVersionTwo)
{
    nlohmann::json json = room_layout_to_json(full_layout(), false);
    json["version"] = 2;
    EXPECT_THROW(room_layout_from_json(json), std::runtime_error);
}

TEST(Unit_DspRoomLayoutCodec, RefusesUnknownPatternName)
{
    nlohmann::json json = room_layout_to_json(full_layout(), false);
    json["microphones"][0]["pattern"] = "Hypercardioid";
    EXPECT_THROW(room_layout_from_json(json), std::runtime_error);
}

TEST(Unit_DspRoomLayoutCodec, RefusesMissingListener)
{
    nlohmann::json json = room_layout_to_json(full_layout(), false);
    json.erase("listener");
    EXPECT_THROW(room_layout_from_json(json), std::runtime_error);
}

TEST(Unit_DspRoomLayoutCodec, RefusesDuplicateMicrophoneId)
{
    nlohmann::json json = room_layout_to_json(full_layout(), false);
    json["microphones"][1]["id"] = json["microphones"][0]["id"];
    EXPECT_THROW(room_layout_from_json(json), std::runtime_error);
}
