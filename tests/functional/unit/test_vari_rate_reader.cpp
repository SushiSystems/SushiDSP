/**************************************************************************/
/* test_vari_rate_reader.cpp                                              */
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
 * @file test_vari_rate_reader.cpp
 * @brief Tests for VariRateReader, which reads a Take at a chosen playback speed.
 * @author Mustafa Garip & Sushi Systems
 */

#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/capture/take.hpp"
#include "SushiDSP/capture/vari_rate_reader.hpp"
#include "measure/spectrum.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    /// @brief Builds a mono take carrying one sine at the given frequency.
    /// @param frequency_hz Tone frequency in Hz.
    /// @param frames Length of the take.
    /// @param sample_rate Rate the take claims, in Hz.
    /// @return The filled take.
    SushiDSP::Take sine_take(double frequency_hz, std::size_t frames, double sample_rate)
    {
        SushiDSP::Take take(1u, sample_rate, frames);
        float* samples = take.mutable_channel(0);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            samples[frame] = static_cast<float>(
                std::sin(2.0 * M_PI * frequency_hz * static_cast<double>(frame) / sample_rate));
        }
        return take;
    }

    /// @brief Reads a run of blocks out of one reader and reports where it left the cursor.
    /// @param reader A prepared reader.
    /// @param take The recording to read.
    /// @param speed Speed passed to every read().
    /// @param block_frames Output frames per read().
    /// @param blocks How many reads to perform.
    /// @param output Destination for blocks * block_frames samples.
    /// @return The take frame the next read() would start at.
    std::uint64_t read_run(SushiDSP::VariRateReader& reader, const SushiDSP::Take& take,
                           double speed, std::uint32_t block_frames, std::uint32_t blocks,
                           std::vector<float>& output)
    {
        output.assign(static_cast<std::size_t>(block_frames) * blocks, 0.0f);
        std::uint64_t cursor = 0u;
        for (std::uint32_t block = 0; block < blocks; ++block)
        {
            float* pointers[1] = {output.data() + static_cast<std::size_t>(block) * block_frames};
            cursor = reader.read(take, cursor, speed, pointers, 1u, block_frames);
        }
        return cursor;
    }
} // namespace

TEST(Unit_Dsp, VariRateReaderAtHalfSpeedLastsTwiceAsLong)
{
    const SushiDSP::Take take = sine_take(440.0, 48000u, 48000.0);

    SushiDSP::VariRateReader reader;
    reader.prepare(1u, 256u);

    std::vector<float> output;
    const std::uint64_t cursor = read_run(reader, take, 0.5, 256u, 375u, output);

    // 375 blocks of 256 is 96000 output frames; at half speed they cost 48000 take frames.
    EXPECT_NEAR(static_cast<double>(cursor), 48000.0, 64.0);
}

TEST(Unit_Dsp, VariRateReaderAtHalfSpeedDropsAnOctave)
{
    const double heard =
        SushiDSP::measure::bin_exact_frequency(220.0, 48000.0, SushiDSP::measure::K_FFT_SIZE);
    const SushiDSP::Take take = sine_take(2.0 * heard, 48000u, 48000.0);

    SushiDSP::VariRateReader reader;
    reader.prepare(1u, 256u);

    std::vector<float> output;
    read_run(reader, take, 0.5, 256u, 375u, output);

    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(
        output, output.size() - SushiDSP::measure::K_FFT_SIZE);
    const std::uint32_t heard_bin = SushiDSP::measure::bin_of(heard, 48000.0);
    const std::uint32_t source_bin = SushiDSP::measure::bin_of(2.0 * heard, 48000.0);

    std::uint32_t peak_bin = 0u;
    double peak = -300.0;
    for (std::uint32_t bin = 1u; bin < spectrum.size(); ++bin)
    {
        if (spectrum[bin] > peak)
        {
            peak = spectrum[bin];
            peak_bin = bin;
        }
    }

    EXPECT_LE(peak_bin, heard_bin + 1u);
    EXPECT_GE(peak_bin + 1u, heard_bin);
    EXPECT_GT(SushiDSP::measure::peak_near(spectrum, heard_bin),
              SushiDSP::measure::peak_near(spectrum, source_bin) + 80.0);
}

TEST(Unit_Dsp, VariRateReaderReturnsExactlyTheFramesAsked)
{
    const SushiDSP::Take take = sine_take(440.0, 48000u, 48000.0);
    const float sentinel = 12345.0f;

    for (double speed : {0.25, 0.5, 1.0, 1.5, 2.0, 4.0})
    {
        SushiDSP::VariRateReader reader;
        reader.prepare(2u, 256u);

        std::vector<float> left(256u, sentinel);
        std::vector<float> right(256u, sentinel);
        float* pointers[2] = {left.data(), right.data()};

        std::uint64_t cursor = 0u;
        for (std::uint32_t block = 0; block < 8u; ++block)
        {
            std::fill(left.begin(), left.end(), sentinel);
            std::fill(right.begin(), right.end(), sentinel);
            cursor = reader.read(take, cursor, speed, pointers, 2u, 256u);

            for (std::size_t frame = 0; frame < 256u; ++frame)
            {
                EXPECT_NE(left[frame], sentinel) << "speed " << speed << " frame " << frame;
                EXPECT_NE(right[frame], sentinel) << "speed " << speed << " frame " << frame;
                EXPECT_TRUE(std::isfinite(left[frame]));
                EXPECT_TRUE(std::isfinite(right[frame]));
            }
        }
        EXPECT_GT(cursor, 0u);
    }
}

TEST(Unit_Dsp, VariRateReaderClampsASpeedOutsideItsRange)
{
    const SushiDSP::Take take = sine_take(440.0, 48000u, 48000.0);
    std::vector<float> output;

    SushiDSP::VariRateReader slowest;
    SushiDSP::VariRateReader too_slow;
    SushiDSP::VariRateReader fastest;
    SushiDSP::VariRateReader too_fast;
    slowest.prepare(1u, 256u);
    too_slow.prepare(1u, 256u);
    fastest.prepare(1u, 256u);
    too_fast.prepare(1u, 256u);

    EXPECT_EQ(read_run(too_slow, take, 0.1, 256u, 64u, output),
              read_run(slowest, take, SushiDSP::VariRateReader::K_MIN_SPEED, 256u, 64u, output));
    EXPECT_EQ(read_run(too_fast, take, 9.0, 256u, 64u, output),
              read_run(fastest, take, SushiDSP::VariRateReader::K_MAX_SPEED, 256u, 64u, output));
}

TEST(Unit_Dsp, VariRateReaderResetStartsFromSilence)
{
    SushiDSP::Take loud(1u, 48000.0, 48000u);
    float* samples = loud.mutable_channel(0);
    for (std::size_t frame = 0; frame < 48000u; ++frame)
    {
        samples[frame] = frame % 2u == 0u ? 1.0f : -1.0f;
    }
    const SushiDSP::Take silent(1u, 48000.0, 48000u);

    SushiDSP::VariRateReader reader;
    reader.prepare(1u, 256u);

    std::vector<float> output(256u, 0.0f);
    float* pointers[1] = {output.data()};
    std::uint64_t cursor = 0u;
    for (std::uint32_t block = 0; block < 32u; ++block)
    {
        cursor = reader.read(loud, cursor, 1.0, pointers, 1u, 256u);
    }

    reader.reset();
    reader.read(silent, 0u, 1.0, pointers, 1u, 256u);

    double peak = 0.0;
    for (std::size_t frame = 0; frame < output.size(); ++frame)
    {
        peak = std::max(peak, std::fabs(static_cast<double>(output[frame])));
    }
    EXPECT_LT(20.0 * std::log10(std::max(peak, 1e-30)), -120.0);
}

TEST(Unit_Dsp, VariRateReaderDoesNotDriftFarDownTheTake)
{
    // At half speed 256 outputs cost 128 take frames. See tests/functional/unit/README.md.
    const SushiDSP::Take take = sine_take(440.0, 4096u, 48000.0);

    SushiDSP::VariRateReader reader;
    reader.prepare(1u, 256u);

    std::vector<float> output(256u, 0.0f);
    float* pointers[1] = {output.data()};
    std::uint64_t cursor = 0u;
    std::uint64_t after_settling = 0u;

    for (std::uint32_t block = 0; block < 6000u; ++block)
    {
        cursor = reader.read(take, cursor, 0.5, pointers, 1u, 256u);
        if (block == 99u)
        {
            after_settling = cursor;
        }
    }

    EXPECT_EQ(cursor - after_settling, 5900ull * 128ull);
}

TEST(Unit_Dsp, VariRateReaderHoldsItsRatioAtEveryLegalSpeed)
{
    const SushiDSP::Take take = sine_take(440.0, 4096u, 48000.0);

    for (double speed : {0.5, 0.75, 1.0, 1.25, 1.5, 2.0})
    {
        SushiDSP::VariRateReader reader;
        reader.prepare(1u, 256u);

        std::vector<float> output(256u, 0.0f);
        float* pointers[1] = {output.data()};
        std::uint64_t cursor = 0u;
        std::uint64_t after_settling = 0u;

        for (std::uint32_t block = 0; block < 4000u; ++block)
        {
            cursor = reader.read(take, cursor, speed, pointers, 1u, 256u);
            if (block == 99u)
            {
                after_settling = cursor;
            }
        }

        const double expected = 3900.0 * 256.0 * speed;
        EXPECT_NEAR(static_cast<double>(cursor - after_settling), expected, 2.0)
            << "speed " << speed;
    }
}
