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
 * @file test_channel_resampler.cpp
 * @brief Covers the rate conversion the host applies to a decoded file.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "ChannelResampler.hpp"

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
            const double phase = 2.0 * 3.14159265358979323846 * frequency
                * static_cast<double>(index) / rate;
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
            const double phase = 2.0 * 3.14159265358979323846 * frequency
                * static_cast<double>(index) / rate;
            worst = std::max(worst, std::abs(static_cast<double>(converted[index])
                                             - std::sin(phase)));
        }
        return worst;
    }
}

TEST(Unit_Dsp, ResampledFrameCountRoundsTheRatio)
{
    // 9093888 frames at 44.1 kHz are 9898109 at 48 kHz. See tests/functional/unit/README.md.
    EXPECT_EQ(SushiDSP::host::resampled_frame_count(1000, 44100.0, 48000.0), 1088u);
    EXPECT_EQ(SushiDSP::host::resampled_frame_count(9093888, 44100.0, 48000.0), 9898109u);
    EXPECT_EQ(SushiDSP::host::resampled_frame_count(48000, 48000.0, 44100.0), 44100u);
}

TEST(Unit_Dsp, ResampledFrameCountKeepsTheLengthWhenNoConversionIsAsked)
{
    EXPECT_EQ(SushiDSP::host::resampled_frame_count(777, 48000.0, 48000.0), 777u);
    EXPECT_EQ(SushiDSP::host::resampled_frame_count(777, 0.0, 48000.0), 777u);
    EXPECT_EQ(SushiDSP::host::resampled_frame_count(777, 48000.0, -1.0), 777u);
}

TEST(Unit_Dsp, ResampleChannelsReturnsTheInputWhenTheRatesMatch)
{
    const std::vector<std::vector<float>> channels{ sine(64, 500.0, 8000.0) };
    EXPECT_EQ(SushiDSP::host::resample_channels(channels, 48000.0, 48000.0), channels);
    EXPECT_EQ(SushiDSP::host::resample_channels(channels, 0.0, 48000.0), channels);
}

TEST(Unit_Dsp, ResampleChannelsDoublesTheRateAndKeepsTheWave)
{
    const std::vector<float>              tone = sine(400, 500.0, 8000.0);
    const std::vector<std::vector<float>> channels{ tone, tone };

    const std::vector<std::vector<float>> converted =
        SushiDSP::host::resample_channels(channels, 8000.0, 16000.0);

    ASSERT_EQ(converted.size(), 2u);
    EXPECT_EQ(converted[0].size(), 800u);
    EXPECT_EQ(converted[0], converted[1]);

    // The ends read silence on one side and stay within 0.035. See tests/functional/unit/README.md.
    EXPECT_LT(worst_error(converted[0], 500.0, 16000.0, 100), 1.0e-3);
    EXPECT_LT(worst_error(converted[0], 500.0, 16000.0, 0), 0.05);
}

TEST(Unit_Dsp, ResampleChannelsAcceptsAnEmptyInput)
{
    const std::vector<std::vector<float>> nothing;
    EXPECT_TRUE(SushiDSP::host::resample_channels(nothing, 44100.0, 48000.0).empty());
}
