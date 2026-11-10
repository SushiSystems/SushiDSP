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
 * @file test_vari_rate_reader.cpp
 * @brief Tests the speed, pitch and bounds of VariRateReader.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/capture/take.hpp>
#include <SushiDSP/nodes/capture/vari_rate_reader.hpp>
#include <SushiDSP/foundation/signal_measure/spectrum.hpp>

#include "support/tolerance.hpp"

namespace
{
    constexpr double K_PI = 3.141592653589793;
    constexpr float K_SENTINEL = 12345.0f;

    /// @brief How far the converter keeps the image of the source tone below the heard tone at half
    /// speed, in dB. Characterized from the first version of the test; it has no derivation yet.
    constexpr double K_MIN_IMAGE_REJECTION_DB = 80.0;

    /// @brief Blocks of 256 outputs the ratio case reads before it counts: the converter's start-up
    /// consumes at most its latency, 34 * max(speed, 1) take frames, 68 at the fastest row; the first
    /// block already costs 128 take frames or more, so four blocks leave three more.
    constexpr std::uint32_t K_RATIO_SETTLING_BLOCKS = 4u;

    /// @brief Blocks of 256 outputs the ratio case counts. Every speed of its rows is a multiple of
    /// 0.25, so the phase arithmetic is exact in double and a read that loses the fraction at a block
    /// boundary loses at least a quarter of a take frame per block: sixteen blocks turn that into four
    /// frames or more, where the exact count is an integer.
    constexpr std::uint32_t K_RATIO_COUNTED_BLOCKS = 16u;

    /// @brief Names a speed and the take frames a run at it must consume.
    struct RatioRow
    {
        const char* name;
        double speed;
        std::uint64_t frames_consumed;
    };

    /// @brief Names a speed to read at.
    struct SpeedRow
    {
        const char* name;
        double speed;
    };

    /// @brief Counts the samples a run left unwritten or not finite, and where it left the cursor.
    struct ReadAudit
    {
        std::size_t bad_samples;
        std::uint64_t cursor;
    };

    /// @brief Builds a mono take carrying one sine at the given frequency.
    SushiDSP::Take sine_take(double frequency_hz, std::size_t frames, double sample_rate)
    {
        SushiDSP::Take take(1u, sample_rate, frames);
        float* samples = take.mutable_channel(0);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            samples[frame] = static_cast<float>(
                std::sin(2.0 * K_PI * frequency_hz * static_cast<double>(frame) / sample_rate));
        }
        return take;
    }

    /// @brief Builds a stereo take of a quiet sine on both channels.
    SushiDSP::Take stereo_sine_take()
    {
        SushiDSP::Take take(2u, 48000.0, 48000u);
        for (std::uint32_t channel = 0; channel < 2u; ++channel)
        {
            float* samples = take.mutable_channel(channel);
            for (std::size_t frame = 0; frame < 48000u; ++frame)
            {
                samples[frame] = 0.25f * std::sin(2.0f * static_cast<float>(K_PI) * 110.0f *
                                                  static_cast<float>(frame) / 48000.0f);
            }
        }
        return take;
    }

    /// @brief Builds a mono take that alternates between 1 and -1.
    SushiDSP::Take alternating_take()
    {
        SushiDSP::Take loud(1u, 48000.0, 48000u);
        float* samples = loud.mutable_channel(0);
        for (std::size_t frame = 0; frame < 48000u; ++frame)
        {
            samples[frame] = frame % 2u == 0u ? 1.0f : -1.0f;
        }
        return loud;
    }

    /// @brief Reads a run of blocks out of one reader and reports where it left the cursor.
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

    /// @brief Reads blocks of 256 frames and returns the take frames consumed after the first
    /// settling blocks, once the converter has settled.
    std::uint64_t frames_consumed_after_settling(const SushiDSP::Take& take, double speed,
                                                 std::uint32_t settling_blocks, std::uint32_t counted_blocks)
    {
        SushiDSP::VariRateReader reader;
        reader.prepare(1u, 256u);
        std::vector<float> output(256u, 0.0f);
        float* pointers[1] = {output.data()};
        std::uint64_t cursor = 0u;
        std::uint64_t after_settling = 0u;

        for (std::uint32_t block = 0; block < settling_blocks + counted_blocks; ++block)
        {
            cursor = reader.read(take, cursor, speed, pointers, 1u, 256u);
            if (block + 1u == settling_blocks)
            {
                after_settling = cursor;
            }
        }
        return cursor - after_settling;
    }

    /// @brief Reads eight stereo blocks at a speed from a sentinel-filled buffer and counts the
    /// samples a block left at the sentinel or not finite.
    ReadAudit audit_eight_stereo_blocks(const SushiDSP::Take& take, double speed)
    {
        SushiDSP::VariRateReader reader;
        reader.prepare(2u, 256u);
        std::vector<float> left(256u, K_SENTINEL);
        std::vector<float> right(256u, K_SENTINEL);
        float* pointers[2] = {left.data(), right.data()};
        ReadAudit audit{0u, 0u};

        for (std::uint32_t block = 0; block < 8u; ++block)
        {
            std::fill(left.begin(), left.end(), K_SENTINEL);
            std::fill(right.begin(), right.end(), K_SENTINEL);
            audit.cursor = reader.read(take, audit.cursor, speed, pointers, 2u, 256u);

            for (std::size_t frame = 0; frame < 256u; ++frame)
            {
                audit.bad_samples += (left[frame] == K_SENTINEL || !std::isfinite(left[frame])) ? 1u : 0u;
                audit.bad_samples += (right[frame] == K_SENTINEL || !std::isfinite(right[frame])) ? 1u : 0u;
            }
        }
        return audit;
    }

    /// @brief Returns the bin of the largest magnitude in a spectrum, skipping bin zero.
    std::uint32_t loudest_bin(const std::vector<double>& spectrum)
    {
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
        return peak_bin;
    }

    /// @brief Reads 32 blocks of a loud take at unity speed, leaving the reader's history full.
    void fill_history(SushiDSP::VariRateReader& reader, const SushiDSP::Take& loud)
    {
        std::vector<float> output(256u, 0.0f);
        float* pointers[1] = {output.data()};
        std::uint64_t cursor = 0u;
        for (std::uint32_t block = 0; block < 32u; ++block)
        {
            cursor = reader.read(loud, cursor, 1.0, pointers, 1u, 256u);
        }
    }

    /// @brief Reads three runs of 200 trapped blocks, one at each speed, resetting before each.
    void read_at_three_speeds_trapped(SushiDSP::VariRateReader& reader, const SushiDSP::Take& take,
                                      float* const* outputs, SushiDSP::test::AllocationTrap& trap)
    {
        trap.arm();
        for (double speed : {0.5, 1.0, 2.0})
        {
            reader.reset();
            std::uint64_t cursor = 0u;
            for (int block = 0; block < 200; ++block)
            {
                cursor = reader.read(take, cursor, speed, outputs, 2u, 256u);
            }
        }
        trap.disarm();
    }

    void expect_finite(const std::vector<float>& samples)
    {
        for (std::size_t frame = 0; frame < samples.size(); ++frame)
        {
            EXPECT_TRUE(std::isfinite(samples[frame])) << "frame " << frame;
        }
    }
} // namespace

TEST(Small_VariRateReader, VariRateReaderAtHalfSpeedDropsAnOctave)
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

    // The tone sits on a bin centre, and halving the speed halves the frequency on to another.
    EXPECT_EQ(loudest_bin(spectrum), SushiDSP::measure::bin_of(heard, 48000.0));
}

TEST(Small_VariRateReader, VariRateReaderAtHalfSpeedKeepsTheSourceToneImageDown)
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

    const double heard_db =
        SushiDSP::measure::peak_near(spectrum, SushiDSP::measure::bin_of(heard, 48000.0));
    const double source_db =
        SushiDSP::measure::peak_near(spectrum, SushiDSP::measure::bin_of(2.0 * heard, 48000.0));
    EXPECT_GT(heard_db, source_db + K_MIN_IMAGE_REJECTION_DB);
}

class Small_VariRateReaderFrames : public ::testing::TestWithParam<SpeedRow>
{
};

TEST_P(Small_VariRateReaderFrames, VariRateReaderReturnsExactlyTheFramesAsked)
{
    const SushiDSP::Take take = sine_take(440.0, 48000u, 48000.0);

    const ReadAudit audit = audit_eight_stereo_blocks(take, GetParam().speed);

    EXPECT_EQ(audit.bad_samples, 0u);
    EXPECT_GT(audit.cursor, 0u);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_VariRateReaderFrames,
                         ::testing::Values(SpeedRow{"Quarter", 0.25}, SpeedRow{"Half", 0.5},
                                           SpeedRow{"Unity", 1.0}, SpeedRow{"OneAndAHalf", 1.5},
                                           SpeedRow{"Double", 2.0}, SpeedRow{"Quadruple", 4.0}),
                         [](const ::testing::TestParamInfo<SpeedRow>& info)
                         { return std::string(info.param.name); });

TEST(Small_VariRateReader, VariRateReaderClampsASpeedOutsideItsRange)
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

TEST(Small_VariRateReader, VariRateReaderResetStartsFromSilence)
{
    const SushiDSP::Take loud = alternating_take();
    const SushiDSP::Take silent(1u, 48000.0, 48000u);
    SushiDSP::VariRateReader reader;
    reader.prepare(1u, 256u);
    fill_history(reader, loud);
    reader.reset();
    std::vector<float> output(256u, 1.0f);
    float* pointers[1] = {output.data()};

    reader.read(silent, 0u, 1.0, pointers, 1u, 256u);

    // A zeroed history under a silent take gives exactly zero, and any left-over sample would not.
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        *std::max_element(output.begin(), output.end(), [](float a, float b) { return std::fabs(a) < std::fabs(b); }),
        0.0, 0.0)));
}

class Small_VariRateReaderRatio : public ::testing::TestWithParam<RatioRow>
{
};

TEST_P(Small_VariRateReaderRatio, VariRateReaderHoldsItsRatioAtEveryLegalSpeed)
{
    // At half speed 256 outputs cost 128 take frames. See tests/nodes/capture/README.md.
    const SushiDSP::Take take = sine_take(440.0, 4096u, 48000.0);

    const std::uint64_t consumed = frames_consumed_after_settling(
        take, GetParam().speed, K_RATIO_SETTLING_BLOCKS, K_RATIO_COUNTED_BLOCKS);

    // 16 blocks of 256 outputs follow the first four, each costing 256 * speed take frames.
    EXPECT_EQ(consumed, GetParam().frames_consumed);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_VariRateReaderRatio,
                         ::testing::Values(RatioRow{"Half", 0.5, 2048ull},
                                           RatioRow{"ThreeQuarters", 0.75, 3072ull},
                                           RatioRow{"Unity", 1.0, 4096ull},
                                           RatioRow{"FiveQuarters", 1.25, 5120ull},
                                           RatioRow{"OneAndAHalf", 1.5, 6144ull},
                                           RatioRow{"Double", 2.0, 8192ull}),
                         [](const ::testing::TestParamInfo<RatioRow>& info)
                         { return std::string(info.param.name); });

TEST(Medium_VariRateReader, ReadPerformsNoHeapAllocation)
{
    // The converters are sized before the trap opens. See tests/nodes/capture/README.md.
    const SushiDSP::Take take = stereo_sine_take();
    SushiDSP::VariRateReader reader;
    reader.prepare(2u, 256u);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* outputs[2] = {left.data(), right.data()};
    SushiDSP::test::AllocationTrap trap;

    read_at_three_speeds_trapped(reader, take, outputs, trap);

    EXPECT_EQ(trap.count(), 0);
    expect_finite(left);
    expect_finite(right);
}
