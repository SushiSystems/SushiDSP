/**************************************************************************/
/* test_channel_strip_node.cpp                                            */
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
 * @file test_channel_strip_node.cpp
 * @brief Tests the mixer channel strip: level, pan, polarity and delay.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/nodes/routing/channel_strip_node.hpp>

#include "support/tolerance.hpp"

#include <cstdint>
#include <vector>

namespace
{
    constexpr std::uint32_t K_BLOCK = 256;
    constexpr double K_RATE = 48000.0;

    /// @brief Holds the gain of each side at the centre: cos(pi/4), the constant-power centre.
    constexpr double K_CENTRE_GAIN = 0.70710678118654752440;

    /// @brief Holds the longest delay the strip documents, in milliseconds.
    constexpr float K_DOCUMENTED_MAX_DELAY_MS = 50.0f;

    /// @brief Holds the blocks that let the 20 ms smoothers reach their targets: a 0.9 step decays
    /// below the snap tolerance of 1e-4 in about 34 blocks of 256 frames.
    constexpr int K_SETTLE_BLOCKS = 60;

    /// @brief Counts the units of u that bound one output sample relative to its level: the pan
    /// angle and its cosine or sine cost 5 u, the float pow of the level about 3.3 u at -20 dB,
    /// and the two products into the sample 2 u; 11 covers the sum.
    constexpr double K_SAMPLE_UNITS = 11.0;

    /// @brief Holds the two output channels of one block.
    struct StripBlock
    {
            std::vector<float> left = std::vector<float>(K_BLOCK);
            std::vector<float> right = std::vector<float>(K_BLOCK);
    };

    /// @brief Runs one block of a mono input through the strip and returns its two channels.
    StripBlock run_block(SushiDSP::ChannelStripNode& strip, const std::vector<float>& input)
    {
        StripBlock block;
        const float* inputs[1] = {input.data()};
        float* outputs[2] = {block.left.data(), block.right.data()};
        SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK, 0.0};
        strip.process(context);
        return block;
    }

    /// @brief Runs enough constant-input blocks for the 20 ms smoothers to reach their targets.
    void settle(SushiDSP::ChannelStripNode& strip, float level_input)
    {
        const std::vector<float> input(K_BLOCK, level_input);
        for (int block = 0; block < K_SETTLE_BLOCKS; ++block)
            static_cast<void>(run_block(strip, input));
    }

    /// @brief Prepares a strip at the rate of the tests below.
    void prepare(SushiDSP::ChannelStripNode& strip)
    {
        strip.prepare(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 2});
    }

    /// @brief Expects @p computed to be @p reference within K_SAMPLE_UNITS of u times @p level.
    void expect_sample(float computed, double reference, double level)
    {
        const double bound = K_SAMPLE_UNITS * TestSupport::unit_roundoff<float>() * level;
        EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(computed, reference, bound)));
    }
} // namespace

TEST(Small_ChannelStripNode, ChannelStripCentreSplitsEachSideAtConstantPower)
{
    SushiDSP::ChannelStripNode strip;
    prepare(strip);

    const StripBlock block = run_block(strip, std::vector<float>(K_BLOCK, 1.0f));

    expect_sample(block.left[0], K_CENTRE_GAIN, 1.0);
    expect_sample(block.right[0], K_CENTRE_GAIN, 1.0);
}

TEST(Small_ChannelStripNode, ChannelStripLevelSetsTheLinearGain)
{
    SushiDSP::ChannelStripNode strip;
    prepare(strip);
    strip.set_parameter_for_control_thread("level", -20.0f);
    settle(strip, 1.0f);

    const StripBlock block = run_block(strip, std::vector<float>(K_BLOCK, 1.0f));

    // -20 dB is a gain of 0.1.
    expect_sample(block.left[K_BLOCK - 1], 0.1 * K_CENTRE_GAIN, 0.1);
}

TEST(Small_ChannelStripNode, ChannelStripFullLeftSilencesTheRight)
{
    SushiDSP::ChannelStripNode strip;
    prepare(strip);
    strip.set_parameter_for_control_thread("pan", -1.0f);
    settle(strip, 1.0f);

    const StripBlock block = run_block(strip, std::vector<float>(K_BLOCK, 1.0f));

    expect_sample(block.left[K_BLOCK - 1], 1.0, 1.0);
    expect_sample(block.right[K_BLOCK - 1], 0.0, 1.0);
}

TEST(Small_ChannelStripNode, ChannelStripPolarityInvertsTheSignal)
{
    SushiDSP::ChannelStripNode strip;
    prepare(strip);
    strip.set_parameter_for_control_thread("polarity", 1.0f);

    const StripBlock block = run_block(strip, std::vector<float>(K_BLOCK, 1.0f));

    expect_sample(block.left[0], -K_CENTRE_GAIN, 1.0);
}

TEST(Small_ChannelStripNode, ChannelStripDelayShiftsTheSignalByWholeSamples)
{
    SushiDSP::ChannelStripNode strip;
    prepare(strip);
    strip.set_parameter_for_control_thread("delay", 2.0f); // 2 ms at 48 kHz is 96 samples
    std::vector<float> impulse(K_BLOCK, 0.0f);
    impulse[0] = 1.0f;

    const StripBlock block = run_block(strip, impulse);

    expect_sample(block.left[95], 0.0, 1.0);
    expect_sample(block.left[96], K_CENTRE_GAIN, 1.0);
}

TEST(Small_ChannelStripNode, ChannelStripDelayIsClampedToFiftyMilliseconds)
{
    SushiDSP::ChannelStripNode strip;
    prepare(strip);

    strip.set_parameter_for_control_thread("delay", 500.0f);

    EXPECT_FLOAT_EQ(strip.get_parameter("delay"), K_DOCUMENTED_MAX_DELAY_MS);
}

TEST(Small_ChannelStripNode, ChannelStripLevelReadsBackInDecibels)
{
    SushiDSP::ChannelStripNode strip;
    prepare(strip);

    strip.set_parameter_for_control_thread("level", -12.0f);

    EXPECT_FLOAT_EQ(strip.get_parameter("level"), -12.0f);
}
