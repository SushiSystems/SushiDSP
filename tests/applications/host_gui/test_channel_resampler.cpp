/**************************************************************************/
/* test_channel_resampler.cpp                                             */
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
 * @file test_channel_resampler.cpp
 * @brief Covers the rate conversion the host applies to a decoded file.
 * @author Mustafa Garip & Sushi Systems
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "channel_resampler.hpp"
#include "support/tolerance.hpp"

namespace
{
    /// @brief Builds one cycle-locked sine.
    /// @param frames How many samples to produce.
    /// @param frequency The tone's frequency in Hz.
    /// @param rate The sample rate in Hz.
    /// @return The samples.
    std::vector<float> sine(std::size_t frames, double frequency, double rate)
    {
        std::vector<float> out(frames);
        for (std::size_t index = 0; index < frames; ++index)
        {
            const double phase =
                2.0 * 3.14159265358979323846 * frequency * static_cast<double>(index) / rate;
            out[index] = static_cast<float>(std::sin(phase));
        }
        return out;
    }

    /// @brief Measures how far a converted channel is from the tone it should carry.
    /// @param converted The converted channel.
    /// @param frequency The tone's frequency in Hz.
    /// @param rate The converted rate in Hz.
    /// @param skip How many samples to ignore at each end.
    /// @return The largest absolute difference.
    double worst_error(const std::vector<float>& converted, double frequency, double rate,
                       std::size_t skip)
    {
        double worst = 0.0;
        for (std::size_t index = skip; index + skip < converted.size(); ++index)
        {
            const double phase =
                2.0 * 3.14159265358979323846 * frequency * static_cast<double>(index) / rate;
            worst =
                std::max(worst, std::abs(static_cast<double>(converted[index]) - std::sin(phase)));
        }
        return worst;
    }
} // namespace

TEST(Small_ChannelResampler, ResampledFrameCountRoundsTheRatio)
{
    // 9093888 frames at 44.1 kHz are 9898109 at 48 kHz. See tests/applications/host_gui/README.md.
    EXPECT_EQ(SushiDSP::host::resampled_frame_count(1000, 44100.0, 48000.0), 1088u);
    EXPECT_EQ(SushiDSP::host::resampled_frame_count(9093888, 44100.0, 48000.0), 9898109u);
    EXPECT_EQ(SushiDSP::host::resampled_frame_count(48000, 48000.0, 44100.0), 44100u);
}

TEST(Small_ChannelResampler, ResampledFrameCountKeepsTheLengthWhenNoConversionIsAsked)
{
    EXPECT_EQ(SushiDSP::host::resampled_frame_count(777, 48000.0, 48000.0), 777u);
    EXPECT_EQ(SushiDSP::host::resampled_frame_count(777, 0.0, 48000.0), 777u);
    EXPECT_EQ(SushiDSP::host::resampled_frame_count(777, 48000.0, -1.0), 777u);
}

TEST(Small_ChannelResampler, ResampleChannelsReturnsTheInputWhenTheRatesMatch)
{
    const std::vector<std::vector<float>> channels{sine(64, 500.0, 8000.0)};
    EXPECT_EQ(SushiDSP::host::resample_channels(channels, 48000.0, 48000.0), channels);
    EXPECT_EQ(SushiDSP::host::resample_channels(channels, 0.0, 48000.0), channels);
}

namespace
{
    constexpr double K_SOURCE_RATE = 8000.0;
    constexpr double K_DOUBLED_RATE = 16000.0;
    constexpr double K_TONE_HZ = 500.0;
    constexpr std::size_t K_TONE_FRAMES = 400;

    /// The kernel's stopband level, 10^(-100.477 / 20) as K_RESAMPLER_STOPBAND_DECIBELS states it.
    /// The tone sits at 0.0625 of the source rate, inside the 0.4 passband edge, and a Kaiser
    /// design of that stopband holds its passband ripple to the same level, so the interior of
    /// the output differs from the analytic sine by the ripple plus the image, two stopband
    /// levels, plus two float roundings of a sample of magnitude one.
    const double K_INTERIOR_BOUND = 2.0 * std::pow(10.0, -100.477 / 20.0) + 2.0 * TestSupport::unit_roundoff<float>();

    /// The error at the first and last samples, where the filter has silence on one side. It is
    /// characterised from the converter as measured, 0.035, and no derivation backs it.
    constexpr double K_END_ERROR_CHARACTERISED = 0.035;

    /// The samples skipped at each end of the interior check; 100 is the skip the converter was measured with.
    constexpr std::size_t K_EDGE_SKIP = 100;

    /// @brief Converts two copies of the test tone from 8 kHz to 16 kHz.
    std::vector<std::vector<float>> doubled()
    {
        const std::vector<float> tone = sine(K_TONE_FRAMES, K_TONE_HZ, K_SOURCE_RATE);
        return SushiDSP::host::resample_channels({tone, tone}, K_SOURCE_RATE, K_DOUBLED_RATE);
    }
} // namespace

TEST(Small_ChannelResampler, ResampleChannelsDoublesTheLengthOfEveryChannel)
{
    const std::vector<std::vector<float>> converted = doubled();

    ASSERT_EQ(converted.size(), 2u);
    EXPECT_EQ(converted[0].size(), 2 * K_TONE_FRAMES);
}

TEST(Small_ChannelResampler, ResampleChannelsConvertsEveryChannelTheSameWay)
{
    const std::vector<std::vector<float>> converted = doubled();

    ASSERT_EQ(converted.size(), 2u);
    EXPECT_EQ(converted[0], converted[1]);
}

TEST(Small_ChannelResampler, ResampleChannelsKeepsTheWaveAwayFromTheEnds)
{
    const std::vector<std::vector<float>> converted = doubled();

    ASSERT_FALSE(converted.empty());
    EXPECT_LT(worst_error(converted[0], K_TONE_HZ, K_DOUBLED_RATE, K_EDGE_SKIP), K_INTERIOR_BOUND);
}

TEST(Small_ChannelResampler, ResampleChannelsKeepsTheWaveWithinTheEndErrorAtTheEnds)
{
    const std::vector<std::vector<float>> converted = doubled();

    ASSERT_FALSE(converted.empty());
    EXPECT_LT(worst_error(converted[0], K_TONE_HZ, K_DOUBLED_RATE, 0), K_END_ERROR_CHARACTERISED);
}

TEST(Small_ChannelResampler, ResampleChannelsAcceptsAnEmptyInput)
{
    const std::vector<std::vector<float>> nothing;
    EXPECT_TRUE(SushiDSP::host::resample_channels(nothing, 44100.0, 48000.0).empty());
}
