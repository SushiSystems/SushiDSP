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
 * @file test_stereo_strip_node.cpp
 * @brief Tests the stereo strip's level, balance and width against hand arithmetic.
 * @author Mustafa Garip & Sushi Systems
 */

#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/audio_node.hpp"
#include "SushiDSP/routing/stereo_strip_node.hpp"

namespace
{
    constexpr std::uint32_t K_BLOCK = 64;
    constexpr double K_RATE = 48000.0;

    /// @brief Holds one block of a strip's two output channels.
    struct StereoBlock
    {
            std::vector<float> left = std::vector<float>(K_BLOCK, 0.0f);
            std::vector<float> right = std::vector<float>(K_BLOCK, 0.0f);
    };

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
} // namespace

TEST(Unit_Dsp, StereoStripAtItsDefaultsPassesTheInput)
{
    const StereoBlock block = render(0.0f, 0.0f, 1.0f, 0.75f, -0.25f);

    EXPECT_NEAR(block.left[0], 0.75f, 1e-6f);
    EXPECT_NEAR(block.right[0], -0.25f, 1e-6f);
}

TEST(Unit_Dsp, StereoStripLevelSetsTheLinearGain)
{
    // -20 dB is a gain of 0.1.
    const StereoBlock block = render(-20.0f, 0.0f, 1.0f, 1.0f, 0.5f);

    EXPECT_NEAR(block.left[0], 0.1f, 1e-5f);
    EXPECT_NEAR(block.right[0], 0.05f, 1e-5f);
}

TEST(Unit_Dsp, StereoStripBalanceTurnsTheFarSideDownAndKeepsTheNearSide)
{
    // Balance +0.5 keeps the right at unity and scales the left by 1 - 0.5.
    const StereoBlock right_leaning = render(0.0f, 0.5f, 1.0f, 1.0f, 1.0f);
    EXPECT_NEAR(right_leaning.left[0], 0.5f, 1e-6f);
    EXPECT_NEAR(right_leaning.right[0], 1.0f, 1e-6f);

    const StereoBlock hard_left = render(0.0f, -1.0f, 1.0f, 1.0f, 1.0f);
    EXPECT_NEAR(hard_left.left[0], 1.0f, 1e-6f);
    EXPECT_NEAR(hard_left.right[0], 0.0f, 1e-6f);
}

TEST(Unit_Dsp, StereoStripWidthZeroIsTheMonoSumOnBothSides)
{
    // Mid of (1.0, 0.0) is 0.5.
    const StereoBlock block = render(0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    EXPECT_NEAR(block.left[0], 0.5f, 1e-6f);
    EXPECT_NEAR(block.right[0], 0.5f, 1e-6f);
}

TEST(Unit_Dsp, StereoStripHalfWidthHalvesTheSideSignal)
{
    // Mid 0.5, side 0.5 * 0.5 = 0.25: left 0.75, right 0.25.
    const StereoBlock block = render(0.0f, 0.0f, 0.5f, 1.0f, 0.0f);

    EXPECT_NEAR(block.left[0], 0.75f, 1e-6f);
    EXPECT_NEAR(block.right[0], 0.25f, 1e-6f);
}

TEST(Unit_Dsp, StereoStripReadsItsParametersBackClamped)
{
    SushiDSP::StereoStripNode strip;
    strip.set_parameter_for_control_thread("level", 40.0f);
    strip.set_parameter_for_control_thread("balance", -3.0f);
    strip.set_parameter_for_control_thread("width", 2.0f);

    EXPECT_FLOAT_EQ(strip.get_parameter("level"), 12.0f);
    EXPECT_FLOAT_EQ(strip.get_parameter("balance"), -1.0f);
    EXPECT_FLOAT_EQ(strip.get_parameter("width"), 1.0f);
    EXPECT_STREQ(strip.type_id(), "sushidsp.stereo_strip");
    EXPECT_EQ(strip.ports().input_channels, 2u);
    EXPECT_EQ(strip.ports().output_channels, 2u);
}
