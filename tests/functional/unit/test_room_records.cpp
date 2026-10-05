/****************************************************************/
/* test_room_records.cpp                                        */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_room_records.cpp
 * @brief Tests for the three records the room's control thread publishes to its audio thread.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <type_traits>

#include "SushiDSP/room/EarlyTapSet.hpp"
#include "SushiDSP/room/FdnDesign.hpp"
#include "SushiDSP/room/ModalDesign.hpp"

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
    EXPECT_EQ(SushiDSP::room::EarlyTapSet::kMaxEarlyTaps, 128u);
    EXPECT_EQ(SushiDSP::room::FdnDesign::kLineCount, 16u);
    EXPECT_EQ(SushiDSP::room::ModalDesign::kMaxModes, 512u);
}

TEST(Unit_Dsp, RoomRecordsAreSmallEnoughToPublishPerGuiFrame)
{
    // One EarlyTap is 152 bytes and one Mode is 56. See tests/functional/unit/README.md.
    EXPECT_EQ(sizeof(SushiDSP::room::EarlyTap), 152u);
    EXPECT_EQ(sizeof(SushiDSP::room::ModalDesign::Mode), 56u);
    EXPECT_LT(sizeof(SushiDSP::room::EarlyTapSet), 24u * 1024u);
    EXPECT_LT(sizeof(SushiDSP::room::ModalDesign), 64u * 1024u);
}
