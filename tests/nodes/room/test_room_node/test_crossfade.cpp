/**************************************************************************/
/* test_crossfade.cpp                                                     */
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
 * @file test_crossfade.cpp
 * @brief Tests for RoomNode: the crossfade a published scene takes at the microphone, for a
 * moved, a nudged and an unchanged scene.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room/room_node.hpp>

#include "nodes/room/test_room_node/room_node_rig.hpp"
#include "support/tolerance.hpp"

namespace
{
    using namespace SushiDSP::test::room_node_rig;
    using SushiDSP::room::RoomScene;

    /// @brief Holds the block the publish is applied before. The node reads the taps from a ring
    /// of 8397 frames, 33 blocks; the publish follows at 64 blocks, past a full ring, so the
    /// taps' filters have settled when the arriving bank starts from zero. The tail's network
    /// runs on through the swap, so the steady tail the blend meets is no louder than at a later
    /// swap.
    constexpr std::uint32_t K_SWAP_BLOCK = 64;

    /// @brief Holds the blocks each render runs: the swap, the fade and four blocks past it.
    constexpr std::size_t K_RENDER_BLOCKS = K_SWAP_BLOCK + 8;

    /// @brief Holds the largest step ratio the moved and nudged rows may read.
    constexpr double K_STEP_RATIO_CEILING = 1.01;

    /// @brief Holds the rounded operations that the blend of two equal renders and the node's own
    /// fade each take: the weight product and the sum.
    constexpr std::size_t K_BLEND_OPERATIONS = 2;

    /// @brief Holds the step of a full-scale tone at its frequency, as a fraction of its peak:
    /// 2 pi f / fs, the largest difference of two neighbouring samples.
    constexpr double K_TONE_STEP = 2.0 * K_PI * 1000.0 / K_SAMPLE_RATE;

    /// @brief Holds what one publish renders: the two steady renders and the moving one.
    struct FadeRenders
    {
            /// @brief Stores the steady render of the old scene.
            std::vector<float> steady_a;

            /// @brief Stores the steady render of the new scene.
            std::vector<float> steady_b;

            /// @brief Stores the render that takes the publish.
            std::vector<float> moving;

            /// @brief Stores the frame the publish is applied at.
            std::size_t swap;
    };

    /// @brief Renders the microphone's tone through a node that moves from the microphone scene
    /// to another, and through a steady node for each. A scene equal to the first needs no second
    /// steady node.
    FadeRenders render_fade(const RoomScene& scene_b, bool same_scene)
    {
        const RoomScene scene_a = microphone_scene();
        const std::vector<float> input = tone(K_RENDER_BLOCKS * K_BLOCK);
        FadeRenders renders;
        renders.swap = static_cast<std::size_t>(K_SWAP_BLOCK) * K_BLOCK;
        renders.steady_a = render(*prepared_node(scene_a, K_LISTENER_MICROPHONE), input);
        renders.steady_b =
            same_scene ? renders.steady_a
                       : render(*prepared_node(scene_b, K_LISTENER_MICROPHONE), input);
        renders.moving = render(*prepared_node(scene_a, K_LISTENER_MICROPHONE), input,
                                {K_SWAP_BLOCK}, scene_edit(scene_b));
        return renders;
    }

    /// @brief Names one move of the microphone and what its fade may leave.
    struct MoveRow
    {
            /// @brief Names the row.
            const char* name;

            /// @brief Stores how far the microphone moves along x, in metres.
            double move_x;

            /// @brief Stores how far it moves along y, in metres.
            double move_y;

            /// @brief Stores the residual ceiling, in dBFS.
            double residual_ceiling_db;
    };

    /// @brief Holds the moves: 0.36 m reorders the taps and refits every filter, so the arriving
    /// bank starts from zero; over one centimetre 78 of the 96 taps carry their state.
    const MoveRow K_MOVES[] = {
        {"MovedThirtySixCentimetres", -0.3, 0.2, -44.0},
        {"NudgedOneCentimetre", 0.01, 0.0, -58.0},
    };

    /// @brief Returns the renders of one move, made once.
    const FadeRenders& moved_renders(const MoveRow& row)
    {
        static std::map<std::string, FadeRenders> cache;
        auto found = cache.find(row.name);
        if (found == cache.end())
        {
            RoomScene moved = microphone_scene();
            moved.microphone_position.x += row.move_x;
            moved.microphone_position.y += row.move_y;
            found = cache.emplace(row.name, render_fade(moved, false)).first;
        }
        return found->second;
    }

    /// @brief Returns the renders of a publish that changes nothing, made once.
    const FadeRenders& unchanged_renders()
    {
        static const FadeRenders renders = render_fade(microphone_scene(), true);
        return renders;
    }

    /// @brief Names a move row for the test names.
    std::string move_name(const ::testing::TestParamInfo<MoveRow>& info)
    {
        return info.param.name;
    }
} // namespace

class Medium_RoomNodeCrossfade : public ::testing::TestWithParam<MoveRow>
{
};

TEST_P(Medium_RoomNodeCrossfade, RoomNodeRendersTheOldSceneUntilThePublishIsAdopted)
{
    const FadeRenders& renders = moved_renders(GetParam());
    EXPECT_EQ(peak_difference(renders.moving, renders.steady_a, 0, renders.swap), 0.0);
}

TEST_P(Medium_RoomNodeCrossfade, RoomNodeCrossfadesAPublishWithoutAStep)
{
    const FadeRenders& renders = moved_renders(GetParam());
    const FadeMeasure measure =
        measure_fade(renders.steady_a, renders.steady_b, renders.moving, renders.swap);
    RecordProperty("step_ratio", measure.step_ratio);
    EXPECT_LE(measure.step_ratio, K_STEP_RATIO_CEILING);
}

TEST_P(Medium_RoomNodeCrossfade, RoomNodeCrossfadesAPublishToTheBlendOfTheTwoRenders)
{
    const FadeRenders& renders = moved_renders(GetParam());
    const FadeMeasure measure =
        measure_fade(renders.steady_a, renders.steady_b, renders.moving, renders.swap);
    RecordProperty("residual_dbfs", to_decibels(measure.residual));
    EXPECT_LT(to_decibels(measure.residual), GetParam().residual_ceiling_db);
}

INSTANTIATE_TEST_SUITE_P(Moves, Medium_RoomNodeCrossfade, ::testing::ValuesIn(K_MOVES), move_name);

TEST(Medium_RoomNodeCrossfadeUnchanged, RoomNodeRendersTheOldSceneUntilThePublishIsAdopted)
{
    const FadeRenders& renders = unchanged_renders();
    EXPECT_EQ(peak_difference(renders.moving, renders.steady_a, 0, renders.swap), 0.0);
}

TEST(Medium_RoomNodeCrossfadeUnchanged, RoomNodeRepublishesTheSameSceneWithoutAStep)
{
    // The same scene refits to the same taps, so the arriving bank renders the retiring one.
    const FadeRenders& renders = unchanged_renders();
    const FadeMeasure measure =
        measure_fade(renders.steady_a, renders.steady_b, renders.moving, renders.swap);
    // Each step carries K_BLEND_OPERATIONS roundings of the peak against K_TONE_STEP of it.
    EXPECT_LE(measure.step_ratio,
              1.0 + 2.0 * static_cast<double>(K_BLEND_OPERATIONS) *
                        TestSupport::unit_roundoff<float>() / K_TONE_STEP);
}

TEST(Medium_RoomNodeCrossfadeUnchanged, RoomNodeRepublishesTheSameSceneWithoutAResidual)
{
    const FadeRenders& renders = unchanged_renders();
    const FadeMeasure measure =
        measure_fade(renders.steady_a, renders.steady_b, renders.moving, renders.swap);
    EXPECT_LE(measure.residual,
              static_cast<double>(K_BLEND_OPERATIONS) *
                  TestSupport::unit_roundoff<float>() * measure.peak);
}
