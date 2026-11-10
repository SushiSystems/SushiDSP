/**************************************************************************/
/* test_pan_node.cpp                                                      */
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
 * @file test_pan_node.cpp
 * @brief Tests the pan law of PanNode.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/routing/pan_node.hpp>

#include "support/tolerance.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
    /// @brief Holds the frames of one block in the tests below.
    constexpr std::uint32_t K_FRAMES = 64;

    /// @brief Holds the most blocks a smoother may take to settle.
    constexpr int K_MAX_BLOCKS = 10000;

    /// @brief Holds the blocks the allocation test processes.
    constexpr int K_TRAP_BLOCKS = 100;

    /// @brief Bounds the absolute error of one settled gain. The angle is (pan + 1) * pi_f / 4 in
    /// float, with pi_f off by 0.47 u relative, one rounding in the sum and one in the product, so
    /// at most pi/2 * 2.5 u = 3.9 u; the cosine or sine of it moves by at most that, and the
    /// library rounds the result by at most u. Five u covers both.
    constexpr double K_GAIN_BOUND = 5.0 * TestSupport::unit_roundoff<float>();

    /// @brief Bounds the absolute error of a squared-gain sum: two squares of gains each off by
    /// K_GAIN_BOUND move by 2 |g| K_GAIN_BOUND, the gains sum to at most sqrt(2) in magnitude,
    /// and the squares and their sum round by u each: 2 * sqrt(2) * 5 u + 2 u = 16.2 u; 17 u.
    constexpr double K_POWER_BOUND = 17.0 * TestSupport::unit_roundoff<float>();

    /// @brief Holds the gain of each side at the centre, one over the square root of two.
    constexpr double K_CENTRE_GAIN = 0.70710678118654752440;

    /// @brief Holds the two gains a settled pan node applies.
    struct Gains
    {
            float left = 0.0f;
            float right = 0.0f;
    };

    /// @brief One pan position, for the rows that check the power.
    struct PanRow
    {
            const char* name;
            float pan;
    };

    /// @brief One parameter bound of the pan descriptor.
    struct BoundRow
    {
            const char* name;
            float SushiDSP::ParameterDescriptor::*bound;
            float expected;
    };

    /// @brief Names a pan row by its name.
    std::string pan_row_name(const ::testing::TestParamInfo<PanRow>& info)
    {
        return info.param.name;
    }

    /// @brief Names a bound row by its name.
    std::string bound_row_name(const ::testing::TestParamInfo<BoundRow>& info)
    {
        return info.param.name;
    }

    /// @brief Runs a unit input through node at a fixed pan until the smoother settles, then
    /// returns the resulting left and right gains (the output value, since the input is 1.0).
    Gains settled_gains(SushiDSP::PanNode& node, float pan)
    {
        node.set_parameter_for_control_thread("pan", pan);
        std::vector<float> input(K_FRAMES, 1.0f);
        std::vector<float> out_left(K_FRAMES, 0.0f);
        std::vector<float> out_right(K_FRAMES, 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[2] = {out_left.data(), out_right.data()};
        SushiDSP::ProcessContext context{outputs, inputs, K_FRAMES, 0.0};

        Gains previous;
        Gains current;
        for (int block = 0; block < K_MAX_BLOCKS; ++block)
        {
            node.process(context);
            current = Gains{out_left.back(), out_right.back()};
            if (block > 0 && current.left == previous.left && current.right == previous.right)
                break;
            previous = current;
        }
        return current;
    }

    /// @brief Prepares a node and returns the gains it settles to at @p pan.
    Gains gains_at(float pan)
    {
        SushiDSP::PanNode node;
        node.prepare(SushiDSP::PrepareInfo{48000.0, K_FRAMES, 1});
        return settled_gains(node, pan);
    }

    /// @brief Expects @p computed to be @p reference within K_GAIN_BOUND.
    void expect_gain(float computed, double reference)
    {
        EXPECT_TRUE(
            TestSupport::is_within(TestSupport::measure_error(computed, reference, K_GAIN_BOUND)));
    }

    /// @brief Processes @p blocks blocks of @p context on @p node.
    void process_blocks(SushiDSP::PanNode& node, SushiDSP::ProcessContext& context, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
            node.process(context);
    }

    class Small_PanNodePower : public ::testing::TestWithParam<PanRow>
    {
    };

    class Small_PanNodeRange : public ::testing::TestWithParam<BoundRow>
    {
    };
} // namespace

TEST(Small_PanNode, PanNodeCentreIsMinus3Point01DbOnBothChannels)
{
    const Gains gains = gains_at(0.0f);

    // One over the square root of two is -3.0103 dB.
    expect_gain(gains.left, K_CENTRE_GAIN);
    expect_gain(gains.right, K_CENTRE_GAIN);
}

TEST(Small_PanNode, PanNodeHardLeftIsUnityLeftAndDeepNullRight)
{
    const Gains gains = gains_at(-1.0f);

    expect_gain(gains.left, 1.0);
    expect_gain(gains.right, 0.0);
}

TEST(Small_PanNode, PanNodeHardRightIsUnityRightAndDeepNullLeft)
{
    const Gains gains = gains_at(1.0f);

    expect_gain(gains.right, 1.0);
    expect_gain(gains.left, 0.0);
}

TEST_P(Small_PanNodePower, PanNodeSumOfSquaredGainsStaysUnity)
{
    const Gains gains = gains_at(GetParam().pan);

    const double power = static_cast<double>(gains.left) * gains.left +
                         static_cast<double>(gains.right) * gains.right;

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(power, 1.0, K_POWER_BOUND)));
}

INSTANTIATE_TEST_SUITE_P(Positions, Small_PanNodePower,
                         ::testing::Values(PanRow{"HardLeft", -1.0f}, PanRow{"ThreeQuartersLeft",
                                                                              -0.75f},
                                           PanRow{"HalfLeft", -0.5f}, PanRow{"QuarterLeft", -0.25f},
                                           PanRow{"Centre", 0.0f}, PanRow{"QuarterRight", 0.25f},
                                           PanRow{"HalfRight", 0.5f},
                                           PanRow{"ThreeQuartersRight", 0.75f},
                                           PanRow{"HardRight", 1.0f}),
                         pan_row_name);

TEST_P(Small_PanNodeRange, PanNodeDescriptorDeclaresTheRangeOfItsPanParameter)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::pan_descriptor();
    ASSERT_EQ(descriptor.parameter_count, 1u);

    EXPECT_EQ(descriptor.parameters[0].*(GetParam().bound), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(Bounds, Small_PanNodeRange,
                         ::testing::Values(BoundRow{"Minimum", &SushiDSP::ParameterDescriptor::min,
                                                    -1.0f},
                                           BoundRow{"Maximum", &SushiDSP::ParameterDescriptor::max,
                                                    1.0f}),
                         bound_row_name);

TEST(Small_PanNode, PanNodeIndexedSetterAgreesWithNamedSetter)
{
    SushiDSP::PanNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, K_FRAMES, 1});

    node.set_parameter_for_audio_thread(static_cast<std::uint16_t>(0), 0.5f);
    EXPECT_FLOAT_EQ(node.get_parameter("pan"), 0.5f);

    node.set_parameter_for_control_thread("pan", -0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("pan"), -0.25f);
}

TEST(Medium_PanNode, ProcessPerformsNoHeapAllocation)
{
    // apply_pan()'s cos/sin recompute runs on stack values only: no heap, lock, syscall, throw.
    SushiDSP::PanNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    node.set_parameter_for_control_thread("pan", 0.5f);
    std::vector<float> input(256, 1.0f);
    std::vector<float> out_left(256, 0.0f);
    std::vector<float> out_right(256, 0.0f);
    const float* inputs[1] = {input.data()};
    float* outputs[2] = {out_left.data(), out_right.data()};
    SushiDSP::ProcessContext context{outputs, inputs, 256, 0.0};

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    process_blocks(node, context, K_TRAP_BLOCKS);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
