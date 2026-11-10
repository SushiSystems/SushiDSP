/**************************************************************************/
/* test_fade_window.cpp                                                   */
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
 * @file test_fade_window.cpp
 * @brief Tests that the fade weight rises monotonically to one over the crossfade window.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/fade_window.hpp>

#include <array>
#include <cmath>
#include <cstdint>

#include <support/tolerance.hpp>

namespace
{
    using SushiDSP::room::FadeWindow;

    /// @brief Counts the frames of a block the blend tests run.
    constexpr std::size_t K_FRAMES = 4;

    using Channel = std::array<float, K_FRAMES>;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Returns the raised-cosine weight of frame index @p index of a 512-frame fade, which
    /// reads one frame ahead: 0.5 (1 - cos(pi (index + 1) / 512)).
    double raised_cosine(std::uint32_t index)
    {
        return 0.5 * (1.0 - std::cos(K_PI * static_cast<double>(index + 1u) / 512.0));
    }

    /// @brief Expects @p weight to be the raised cosine at @p index to a float rounding of the
    /// cosine's evaluation.
    void expect_raised_cosine(float weight, std::uint32_t index)
    {
        EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(weight, raised_cosine(index), 4u)));
    }
    /// @brief Expects the weights of every frame of a fade of @p total frames to lie in (0, 1]
    /// and never to fall.
    void expect_weights_rise_within_unit_range(std::uint32_t total)
    {
        float previous = 0.0f;
        for (std::uint32_t frame = 0; frame < total; ++frame)
        {
            const float weight = SushiDSP::room::fade_weight(frame, total);
            EXPECT_GT(weight, 0.0f) << "frame " << frame;
            EXPECT_LE(weight, 1.0f) << "frame " << frame;
            EXPECT_GE(weight, previous) << "frame " << frame;
            previous = weight;
        }
    }
} // namespace

TEST(Small_FadeWindow, WeightEndsAtOne)
{
    EXPECT_FLOAT_EQ(SushiDSP::room::fade_weight(511u, 512u), 1.0f);
}

TEST(Small_FadeWindow, WeightRisesMonotonicallyAndStaysInUnitRange)
{
    expect_weights_rise_within_unit_range(SushiDSP::room::K_CROSSFADE_FRAMES);
}

TEST(Small_FadeWindow, WindowWeightUnlaggedReadsTheFadeAtTheFramesRunPlusTheFrame)
{
    const FadeWindow window{100u, 64u, 10u};

    expect_raised_cosine(SushiDSP::room::window_weight(window, 5u, false), 105u);
}

TEST(Small_FadeWindow, WindowWeightLaggedIsZeroBeforeTheLag)
{
    const FadeWindow window{2u, 8u, 10u};

    EXPECT_EQ(SushiDSP::room::window_weight(window, 3u, true), 0.0f);
}

TEST(Small_FadeWindow, WindowWeightLaggedReadsTheFadeBehindTheLag)
{
    const FadeWindow window{100u, 64u, 10u};

    expect_raised_cosine(SushiDSP::room::window_weight(window, 5u, true), 95u);
}

TEST(Small_FadeWindow, BlendPairKeepsTheRetiringPairWhileTheWeightIsZero)
{
    // A lagged window that starts at the first frame reads weight zero for the first ten frames.
    Channel left = {1.5f, 1.5f, 1.5f, 1.5f};
    Channel right = {2.5f, 2.5f, 2.5f, 2.5f};
    const Channel old_left = {0.5f, 0.5f, 0.5f, 0.5f};
    const Channel old_right = {0.25f, 0.25f, 0.25f, 0.25f};
    const FadeWindow window{0u, 4u, 10u};

    SushiDSP::room::blend_pair(left.data(), right.data(), old_left.data(), old_right.data(), window, true);

    EXPECT_EQ(left, old_left);
    EXPECT_EQ(right, old_right);
}

TEST(Small_FadeWindow, BlendPairReachesTheArrivingPairAtTheEndOfTheFade)
{
    // The weight is one on the last frame of the fade; the values are dyadic, so old + (new - old) is exact.
    Channel left = {1.5f, 1.5f, 1.5f, 1.5f};
    Channel right = {2.5f, 2.5f, 2.5f, 2.5f};
    const Channel old_left = {0.5f, 0.5f, 0.5f, 0.5f};
    const Channel old_right = {0.25f, 0.25f, 0.25f, 0.25f};
    const Channel arriving_left = left;
    const Channel arriving_right = right;
    const FadeWindow window{511u, 1u, 0u};

    SushiDSP::room::blend_pair(left.data(), right.data(), old_left.data(), old_right.data(), window, false);

    EXPECT_EQ(left[0], arriving_left[0]);
    EXPECT_EQ(right[0], arriving_right[0]);
}

TEST(Small_FadeWindow, BlendPairLeavesTheFramesPastTheFadeUntouched)
{
    Channel left = {1.5f, 1.5f, 1.5f, 1.5f};
    Channel right = {2.5f, 2.5f, 2.5f, 2.5f};
    const Channel old_left = {0.5f, 0.5f, 0.5f, 0.5f};
    const Channel old_right = {0.25f, 0.25f, 0.25f, 0.25f};
    const FadeWindow window{0u, 2u, 10u};

    SushiDSP::room::blend_pair(left.data(), right.data(), old_left.data(), old_right.data(), window, true);

    const Channel expected_left = {0.5f, 0.5f, 1.5f, 1.5f};
    const Channel expected_right = {0.25f, 0.25f, 2.5f, 2.5f};
    EXPECT_EQ(left, expected_left);
    EXPECT_EQ(right, expected_right);
}
