/**************************************************************************/
/* test_direct_path.cpp                                                   */
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
 * @file test_direct_path.cpp
 * @brief Tests that the direct path delays its sound by the alignment and that a cleared bank goes silent.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/delay_ring.hpp>
#include <SushiDSP/nodes/room/direct_path.hpp>

#include <array>
#include <cstdint>
#include <memory>

namespace
{
    using SushiDSP::room::DelayRing;
    using SushiDSP::room::DirectPath;
    using SushiDSP::room::EarlyTap;
    using SushiDSP::room::RoomRenderSet;

    /// @brief Counts the frames of every block the tests write and render.
    constexpr std::uint32_t K_FRAMES = 16;

    /// @brief Counts the frames the path delays every read by.
    constexpr std::uint32_t K_ALIGNMENT = 8;

    using Block = std::array<float, K_FRAMES>;

    /// @brief Returns a tap that passes its input unchanged: no delay, unit gain, and both
    /// sections the identity.
    EarlyTap unit_tap()
    {
        EarlyTap tap{};
        tap.gain = 1.0f;
        tap.section_a = {1.0, 0.0, 0.0, 0.0, 0.0};
        tap.section_b = {1.0, 0.0, 0.0, 0.0, 0.0};
        return tap;
    }

    /// @brief Returns a set whose two ears' impulse responses are a unit impulse and whose
    /// interaural delay is zero.
    std::unique_ptr<RoomRenderSet> delta_set()
    {
        auto set = std::make_unique<RoomRenderSet>();
        set->direct_left[0] = 1.0f;
        set->direct_right[0] = 1.0f;
        return set;
    }

    /// @brief Builds a ring that holds a unit impulse at the first frame of its last block.
    DelayRing ring_with_impulse()
    {
        DelayRing ring;
        ring.prepare(64u, K_FRAMES);
        Block block{};
        block[0] = 1.0f;
        ring.write(block.data(), K_FRAMES);
        return ring;
    }

    /// @brief Returns a path prepared for K_ALIGNMENT frames of alignment with bank 0 loaded.
    DirectPath loaded_path()
    {
        DirectPath path;
        path.prepare(K_ALIGNMENT, K_FRAMES);
        path.load_bank(0u, unit_tap(), *delta_set());
        return path;
    }
} // namespace

TEST(Small_DirectPath, DirectPathDelaysThePassedThroughImpulseByTheAlignment)
{
    DirectPath path = loaded_path();
    const DelayRing ring = ring_with_impulse();
    Block left{};
    Block right{};

    path.render(ring, 0u, left.data(), right.data(), K_FRAMES);

    Block expected{};
    expected[K_ALIGNMENT] = 1.0f;
    EXPECT_EQ(left, expected);
    EXPECT_EQ(right, expected);
}

TEST(Small_DirectPath, LoadingABankActivatesIt)
{
    DirectPath path = loaded_path();

    EXPECT_TRUE(path.active(0u));
}

TEST(Small_DirectPath, ClearingALoadedBankDeactivatesIt)
{
    DirectPath path = loaded_path();

    path.clear_bank(0u);

    EXPECT_FALSE(path.active(0u));
}

TEST(Small_DirectPath, AClearedBankRendersSilence)
{
    DirectPath path = loaded_path();
    const DelayRing ring = ring_with_impulse();
    Block left{};
    Block right{};
    left.fill(1.0f);
    right.fill(1.0f);

    path.clear_bank(0u);
    path.render(ring, 0u, left.data(), right.data(), K_FRAMES);

    const Block silence{};
    EXPECT_EQ(left, silence);
    EXPECT_EQ(right, silence);
}
