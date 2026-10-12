/**************************************************************************/
/* test_room_layout.cpp                                                   */
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

/** @file test_room_layout.cpp
 *  @brief Tests the host's record of the cabinet section: placements, the microphone limit and ids.
 *  @author Mustafa Garip
 */

#include <string>

#include <gtest/gtest.h>

#include "room_layout.hpp"

namespace
{
    RoomPlacement at(double x) { return RoomPlacement{SushiDSP::room::Vec3{x, 2.0, 0.5}, 0.0}; }
} // namespace

TEST(Unit_Dsp, RoomLayoutPlacesCabinetsInPathOrder)
{
    RoomLayout layout;
    std::string reason;

    EXPECT_FALSE(layout.set_cabinet(1, at(1.0), reason));
    EXPECT_TRUE(layout.set_cabinet(0, at(1.0), reason));
    EXPECT_TRUE(layout.set_cabinet(1, at(2.0), reason));
    EXPECT_EQ(layout.cabinets().size(), 2u);
    EXPECT_FALSE(layout.set_cabinet(RoomLayout::K_MAX_CABINETS, at(3.0), reason));
}

TEST(Unit_Dsp, RoomLayoutRefusesTheSeventhMicrophone)
{
    RoomLayout layout;
    std::string reason;
    MicrophoneSpot spot;

    for (std::size_t i = 0; i < RoomLayout::K_MAX_MICROPHONES; ++i)
    {
        EXPECT_NE(layout.add_microphone(spot, reason), 0u);
    }
    EXPECT_EQ(layout.add_microphone(spot, reason), 0u);
    EXPECT_NE(reason.find("microphones"), std::string::npos);
}

TEST(Unit_Dsp, RoomLayoutGivesEachMicrophoneAFreshIdAndFindsIt)
{
    RoomLayout layout;
    std::string reason;
    MicrophoneSpot spot;
    spot.place = at(1.0);

    const std::uint32_t first = layout.add_microphone(spot, reason);
    const std::uint32_t second = layout.add_microphone(spot, reason);

    EXPECT_NE(first, second);
    ASSERT_NE(layout.find_microphone(second), nullptr);
    EXPECT_TRUE(layout.remove_microphone(first));
    EXPECT_EQ(layout.find_microphone(first), nullptr);
    EXPECT_FALSE(layout.remove_microphone(first));
}

TEST(Unit_Dsp, RoomLayoutUpdatesAMicrophoneInPlace)
{
    RoomLayout layout;
    std::string reason;
    MicrophoneSpot spot;
    const std::uint32_t id = layout.add_microphone(spot, reason);

    MicrophoneSpot moved = *layout.find_microphone(id);
    moved.place = at(3.0);
    moved.room_mic = true;
    EXPECT_TRUE(layout.update_microphone(moved));

    const MicrophoneSpot* held = layout.find_microphone(id);
    ASSERT_NE(held, nullptr);
    EXPECT_DOUBLE_EQ(held->place.position.x, 3.0);
    EXPECT_TRUE(held->room_mic);
}

TEST(Unit_Dsp, RoomLayoutPairSceneCarriesBothPoses)
{
    RoomLayout layout;
    std::string reason;
    layout.set_listener(at(4.0));
    ASSERT_TRUE(layout.set_cabinet(0, at(1.5), reason));
    MicrophoneSpot spot;
    spot.place = at(2.5);
    layout.add_microphone(spot, reason);

    const SushiDSP::room::RoomScene scene = layout.pair_scene(0, 1);

    EXPECT_DOUBLE_EQ(scene.source_position.x, 1.5);
    EXPECT_DOUBLE_EQ(scene.microphone_position.x, 2.5);
}
