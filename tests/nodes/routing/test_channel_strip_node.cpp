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

/** @file test_channel_strip_node.cpp
 *  @brief Tests the mixer channel strip: level, pan, polarity and delay.
 *  @author Mustafa Garip
 */

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/nodes/routing/channel_strip_node.hpp>

namespace
{
    constexpr std::uint32_t K_BLOCK = 256;
    constexpr double K_RATE = 48000.0;
    constexpr float K_CENTRE_GAIN = 0.70710678f; // cos(pi/4): constant-power centre

    /// @brief Runs one block of a mono input through the strip and returns its two channels.
    void run_block(SushiDSP::ChannelStripNode& strip, const std::vector<float>& input,
                   std::vector<float>& left, std::vector<float>& right)
    {
        const float* inputs[1] = {input.data()};
        float* outputs[2] = {left.data(), right.data()};
        SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK, 0.0};
        strip.process(context);
    }

    /// @brief Runs enough constant-input blocks for the 20 ms smoothers to reach their targets.
    /// A 0.9 step decays below the snap tolerance of 1e-4 in about 34 blocks of 256 frames.
    void settle(SushiDSP::ChannelStripNode& strip, float level_input)
    {
        const std::vector<float> input(K_BLOCK, level_input);
        std::vector<float> left(K_BLOCK), right(K_BLOCK);
        for (int block = 0; block < 60; ++block)
        {
            run_block(strip, input, left, right);
        }
    }
} // namespace

TEST(ChannelStripNode, ChannelStripCentreSplitsEachSideAtConstantPower)
{
    SushiDSP::ChannelStripNode strip;
    strip.prepare(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 2});

    const std::vector<float> input(K_BLOCK, 1.0f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);
    run_block(strip, input, left, right);

    EXPECT_NEAR(left[0], K_CENTRE_GAIN, 1e-4f);
    EXPECT_NEAR(right[0], K_CENTRE_GAIN, 1e-4f);
}

TEST(ChannelStripNode, ChannelStripLevelSetsTheLinearGain)
{
    SushiDSP::ChannelStripNode strip;
    strip.prepare(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 2});
    strip.set_parameter_for_control_thread("level", -20.0f);
    settle(strip, 1.0f);

    const std::vector<float> input(K_BLOCK, 1.0f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);
    run_block(strip, input, left, right);

    EXPECT_NEAR(left[K_BLOCK - 1], 0.1f * K_CENTRE_GAIN, 1e-3f);
}

TEST(ChannelStripNode, ChannelStripFullLeftSilencesTheRight)
{
    SushiDSP::ChannelStripNode strip;
    strip.prepare(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 2});
    strip.set_parameter_for_control_thread("pan", -1.0f);
    settle(strip, 1.0f);

    const std::vector<float> input(K_BLOCK, 1.0f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);
    run_block(strip, input, left, right);

    EXPECT_NEAR(left[K_BLOCK - 1], 1.0f, 1e-4f);
    EXPECT_NEAR(right[K_BLOCK - 1], 0.0f, 1e-4f);
}

TEST(ChannelStripNode, ChannelStripPolarityInvertsTheSignal)
{
    SushiDSP::ChannelStripNode strip;
    strip.prepare(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 2});
    strip.set_parameter_for_control_thread("polarity", 1.0f);

    const std::vector<float> input(K_BLOCK, 1.0f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);
    run_block(strip, input, left, right);

    EXPECT_LT(left[0], 0.0f);
    EXPECT_NEAR(left[0], -K_CENTRE_GAIN, 1e-4f);
}

TEST(ChannelStripNode, ChannelStripDelayShiftsTheSignalByWholeSamples)
{
    SushiDSP::ChannelStripNode strip;
    strip.prepare(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 2});
    strip.set_parameter_for_control_thread("delay", 2.0f); // 2 ms at 48 kHz is 96 samples

    std::vector<float> impulse(K_BLOCK, 0.0f);
    impulse[0] = 1.0f;
    std::vector<float> left(K_BLOCK), right(K_BLOCK);
    run_block(strip, impulse, left, right);

    EXPECT_NEAR(left[95], 0.0f, 1e-6f);
    EXPECT_NEAR(left[96], K_CENTRE_GAIN, 1e-4f);
}

TEST(ChannelStripNode, ChannelStripDelayIsClampedToFiftyMilliseconds)
{
    SushiDSP::ChannelStripNode strip;
    strip.prepare(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 2});
    strip.set_parameter_for_control_thread("delay", 500.0f);

    EXPECT_FLOAT_EQ(strip.get_parameter("delay"), SushiDSP::ChannelStripNode::K_MAX_DELAY_MS);
}

TEST(ChannelStripNode, ChannelStripParametersReadBackWhatWasSet)
{
    SushiDSP::ChannelStripNode strip;
    strip.prepare(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 2});

    strip.set_parameter_for_control_thread("level", -12.0f);
    strip.set_parameter_for_control_thread("pan", 0.25f);
    strip.set_parameter_for_control_thread("polarity", 1.0f);

    EXPECT_FLOAT_EQ(strip.get_parameter("level"), -12.0f);
    EXPECT_FLOAT_EQ(strip.get_parameter("pan"), 0.25f);
    EXPECT_FLOAT_EQ(strip.get_parameter("polarity"), 1.0f);
}
