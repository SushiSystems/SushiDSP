/**************************************************************************/
/* test_stereo_strip_node.cpp                                             */
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
 * @file test_stereo_strip_node.cpp
 * @brief Tests the stereo strip's level, balance and width against hand arithmetic.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/nodes/routing/stereo_strip_node.hpp>

#include "support/tolerance.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    constexpr std::uint32_t K_BLOCK = 64;
    constexpr double K_RATE = 48000.0;

    /// @brief Counts the rounded operations on the longest path to a sample: the sum and the
    /// difference of the inputs, the width product, the mid-side add, the balance gain and the
    /// level gain are six; the level itself is a float pow of a divided exponent, which at 20 dB
    /// costs about three more units of u; ten bounds the whole path.
    constexpr std::size_t K_ROUNDED_OPERATIONS = 10;

    /// @brief Holds one block of a strip's two output channels.
    struct StereoBlock
    {
            std::vector<float> left = std::vector<float>(K_BLOCK, 0.0f);
            std::vector<float> right = std::vector<float>(K_BLOCK, 0.0f);
    };

    /// @brief One parameter that clamps: writing @p written reads back @p expected.
    struct ClampRow
    {
            const char* name;
            const char* id;
            float written;
            float expected;
    };

    /// @brief Names a clamp row by its name.
    std::string clamp_row_name(const ::testing::TestParamInfo<ClampRow>& info)
    {
        return info.param.name;
    }

    /// @brief Prepares a strip, writes its parameters, snaps the smoothers and runs one block of a
    /// constant stereo input.
    StereoBlock render(float level_db, float balance, float width, float in_left, float in_right)
    {
        SushiDSP::StereoStripNode strip;
        strip.prepare(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 2});
        strip.set_parameter_for_control_thread("level", level_db);
        strip.set_parameter_for_control_thread("balance", balance);
        strip.set_parameter_for_control_thread("width", width);
        strip.reset();

        const std::vector<float> left(K_BLOCK, in_left);
        const std::vector<float> right(K_BLOCK, in_right);
        const float* inputs[2] = {left.data(), right.data()};
        StereoBlock block;
        float* outputs[2] = {block.left.data(), block.right.data()};
        SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK, 0.0};
        strip.process(context);
        return block;
    }

    /// @brief Expects @p computed to match @p reference within the rounding of the strip's path.
    void expect_strip_value(float computed, double reference)
    {
        EXPECT_TRUE(TestSupport::is_within(
            TestSupport::measure_relative_error<float>(computed, reference,
                                                       K_ROUNDED_OPERATIONS)));
    }

    class Small_StereoStripClamp : public ::testing::TestWithParam<ClampRow>
    {
    };
} // namespace

TEST(Small_StereoStripNode, StereoStripAtItsDefaultsPassesTheInput)
{
    const StereoBlock block = render(0.0f, 0.0f, 1.0f, 0.75f, -0.25f);

    expect_strip_value(block.left[0], 0.75);
    expect_strip_value(block.right[0], -0.25);
}

TEST(Small_StereoStripNode, StereoStripLevelSetsTheLinearGain)
{
    // -20 dB is a gain of 0.1.
    const StereoBlock block = render(-20.0f, 0.0f, 1.0f, 1.0f, 0.5f);

    expect_strip_value(block.left[0], 0.1);
    expect_strip_value(block.right[0], 0.05);
}

TEST(Small_StereoStripNode, StereoStripBalanceTurnsTheFarSideDownAndKeepsTheNearSide)
{
    // Balance +0.5 keeps the right at unity and scales the left by 1 - 0.5.
    const StereoBlock right_leaning = render(0.0f, 0.5f, 1.0f, 1.0f, 1.0f);
    expect_strip_value(right_leaning.left[0], 0.5);
    expect_strip_value(right_leaning.right[0], 1.0);

    // Balance -1 keeps the left at unity and scales the right by 1 - 1, which is exactly zero.
    const StereoBlock hard_left = render(0.0f, -1.0f, 1.0f, 1.0f, 1.0f);
    expect_strip_value(hard_left.left[0], 1.0);
    expect_strip_value(hard_left.right[0], 0.0);
}

TEST(Small_StereoStripNode, StereoStripWidthZeroIsTheMonoSumOnBothSides)
{
    // Mid of (1.0, 0.0) is 0.5.
    const StereoBlock block = render(0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    expect_strip_value(block.left[0], 0.5);
    expect_strip_value(block.right[0], 0.5);
}

TEST(Small_StereoStripNode, StereoStripHalfWidthHalvesTheSideSignal)
{
    // Mid 0.5, side 0.5 * 0.5 = 0.25: left 0.75, right 0.25.
    const StereoBlock block = render(0.0f, 0.0f, 0.5f, 1.0f, 0.0f);

    expect_strip_value(block.left[0], 0.75);
    expect_strip_value(block.right[0], 0.25);
}

TEST_P(Small_StereoStripClamp, StereoStripReadsAParameterBackClamped)
{
    const ClampRow& row = GetParam();
    SushiDSP::StereoStripNode strip;

    strip.set_parameter_for_control_thread(row.id, row.written);

    EXPECT_FLOAT_EQ(strip.get_parameter(row.id), row.expected);
}

INSTANTIATE_TEST_SUITE_P(Parameters, Small_StereoStripClamp,
                         ::testing::Values(ClampRow{"LevelAboveItsMaximum", "level", 40.0f, 12.0f},
                                           ClampRow{"BalanceBelowItsMinimum", "balance", -3.0f,
                                                    -1.0f},
                                           ClampRow{"WidthAboveItsMaximum", "width", 2.0f, 1.0f}),
                         clamp_row_name);
