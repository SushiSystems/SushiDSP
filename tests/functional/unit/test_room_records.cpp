/**************************************************************************/
/* test_room_records.cpp                                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

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
    // One EarlyTap is 4 + 4 + 40 + 40 + 64 = 152 bytes, a multiple of the double's alignment,
    // so a 128-tap set is 19456 plus 12 of header, padded to 19472; one Mode is 40 + 4 + 8 = 52
    // padded to 56, so 512 of them are 28672 plus 12, padded to 28688.
    EXPECT_EQ(sizeof(SushiDSP::room::EarlyTap), 152u);
    EXPECT_EQ(sizeof(SushiDSP::room::ModalDesign::Mode), 56u);
    EXPECT_LT(sizeof(SushiDSP::room::EarlyTapSet), 24u * 1024u);
    EXPECT_LT(sizeof(SushiDSP::room::ModalDesign), 64u * 1024u);
}
