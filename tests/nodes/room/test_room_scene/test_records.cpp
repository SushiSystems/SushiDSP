/**************************************************************************/
/* test_records.cpp                                                       */
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
 * @file test_records.cpp
 * @brief Tests for the size budget of the records the room's control thread publishes to its audio thread.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <cstddef>

#include <SushiDSP/nodes/room/early_tap_set.hpp>
#include <SushiDSP/nodes/room/modal_design.hpp>

namespace
{
    /// @brief Bounds the size of a record published to the audio thread each GUI frame, in bytes:
    /// the early tap set stays under 24 KiB.
    constexpr std::size_t K_EARLY_TAP_SET_BUDGET_BYTES = 24u * 1024u;

    /// @brief Bounds the size of the modal design published each GUI frame, in bytes: 64 KiB.
    constexpr std::size_t K_MODAL_DESIGN_BUDGET_BYTES = 64u * 1024u;
} // namespace

TEST(Small_RoomScene, RoomRecordsAreSmallEnoughToPublishPerGuiFrame)
{
    // A 128-tap set is 19472 bytes and 512 modes are 28688. See tests/nodes/room/test_room_scene/README.md.
    EXPECT_LT(sizeof(SushiDSP::room::EarlyTapSet), K_EARLY_TAP_SET_BUDGET_BYTES);
    EXPECT_LT(sizeof(SushiDSP::room::ModalDesign), K_MODAL_DESIGN_BUDGET_BYTES);
}
