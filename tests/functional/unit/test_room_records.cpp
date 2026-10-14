/**************************************************************************/
/* test_room_records.cpp                                                  */
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
 * @file test_room_records.cpp
 * @brief Tests for the three records the room's control thread publishes to its audio thread.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <type_traits>

#include <SushiDSP/nodes/room/early_tap_set.hpp>
#include <SushiDSP/nodes/room/fdn_design.hpp>
#include <SushiDSP/nodes/room/modal_design.hpp>

TEST(Unit_Dsp, RoomRecordsAreTriviallyCopyable)
{
    static_assert(std::is_trivially_copyable<SushiDSP::room::EarlyTap>::value,
                  "the audio thread reads it");
    static_assert(std::is_trivially_copyable<SushiDSP::room::EarlyTapSet>::value,
                  "the audio thread reads it");
    static_assert(std::is_trivially_copyable<SushiDSP::room::FdnDesign>::value,
                  "the audio thread reads it");
    static_assert(std::is_trivially_copyable<SushiDSP::room::ModalDesign>::value,
                  "the audio thread reads it");
    SUCCEED();
}

TEST(Unit_Dsp, RoomRecordsCarryTheSpecsCapacities)
{
    EXPECT_EQ(SushiDSP::room::EarlyTapSet::K_MAX_EARLY_TAPS, 128u);
    EXPECT_EQ(SushiDSP::room::FdnDesign::K_LINE_COUNT, 16u);
    EXPECT_EQ(SushiDSP::room::ModalDesign::K_MAX_MODES, 512u);
}

TEST(Unit_Dsp, RoomRecordsAreSmallEnoughToPublishPerGuiFrame)
{
    // One EarlyTap is 152 bytes and one Mode is 56. See tests/functional/unit/README.md.
    EXPECT_EQ(sizeof(SushiDSP::room::EarlyTap), 152u);
    EXPECT_EQ(sizeof(SushiDSP::room::ModalDesign::Mode), 56u);
    EXPECT_LT(sizeof(SushiDSP::room::EarlyTapSet), 24u * 1024u);
    EXPECT_LT(sizeof(SushiDSP::room::ModalDesign), 64u * 1024u);
}
