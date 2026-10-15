/**************************************************************************/
/* test_resampler.cpp                                                     */
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
 * @file test_resampler.cpp
 * @brief Tests for the arbitrary-ratio polyphase Resampler.
 * @author Mustafa Garip & Sushi Systems
 */

#define _USE_MATH_DEFINES

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/math/resampler.hpp>
#include <SushiDSP/foundation/signal_measure/null_test.hpp>
#include <SushiDSP/foundation/signal_measure/spectrum.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    /// @brief Frames at the outer rate that a round trip's start-up covers.
    ///
    /// A stage's first outputs are formed against a zero history where the signal's own past
    /// belongs, so they carry whatever that past held. The error dies inside one kernel half
    /// span, widest at the slowest ratio used here: 34 / (20000/48000) = 81.6 source samples.
    constexpr std::size_t K_SETTLING_FRAMES = 256;

    /// @brief Streams a signal through a resampler in fixed-size pushes.
    /// @param resampler A prepared resampler.
    /// @param input The signal at the resampler's source rate.
    /// @param push Frames per process() call.
    /// @return Everything the resampler produced, in order.
    std::vector<float> stream(SushiDSP::Resampler& resampler, const std::vector<float>& input,
                              std::uint32_t push)
    {
        std::vector<float> output;
        std::vector<float> scratch(resampler.maximum_output_frames(push) + 8u, 0.0f);
        for (std::size_t position = 0; position < input.size(); position += push)
        {
            const std::uint32_t frames =
                static_cast<std::uint32_t>(std::min<std::size_t>(push, input.size() - position));
            const std::uint32_t produced =
                resampler.process(input.data() + position, frames, scratch.data(),
                                  static_cast<std::uint32_t>(scratch.size()));
            output.insert(output.end(), scratch.begin(), scratch.begin() + produced);
        }
        return output;
    }

    /// @brief Residual of b against a once both have left their start-up region.
    /// @param a Reference signal, sample-aligned with b.
    /// @param b Signal under test.
    /// @pre Both signals hold more than K_SETTLING_FRAMES samples.
    /// @return Residual in dB.
    double settled_null_test(const std::vector<float>& a, const std::vector<float>& b)
    {
        const std::vector<float> settled_a(a.begin() + K_SETTLING_FRAMES, a.end());
        const std::vector<float> settled_b(b.begin() + K_SETTLING_FRAMES, b.end());
        return SushiDSP::measure::null_test(settled_a, settled_b, 512u);
    }

    /// @brief Sum of unit-amplitude sines, scaled so the peak stays inside +/-1.
    /// @param frequencies_hz The partials.
    /// @param sample_rate Sample rate.
    /// @param length Samples to generate.
    /// @return The signal.
    std::vector<float> multitone(const std::vector<double>& frequencies_hz, double sample_rate,
                                 std::size_t length)
    {
        std::vector<float> signal(length, 0.0f);
        const double amplitude = 0.8 / static_cast<double>(frequencies_hz.size());
        for (std::size_t n = 0; n < length; ++n)
        {
            double value = 0.0;
            for (const double frequency : frequencies_hz)
            {
                value += amplitude *
                         std::sin(2.0 * M_PI * frequency * static_cast<double>(n) / sample_rate);
            }
            signal[n] = static_cast<float>(value);
        }
        return signal;
    }
} // namespace

TEST(Resampler, ResamplerHoldsADcLevelAtEveryRatio)
{
    // Every polyphase branch has unity DC gain. See tests/foundation/math/README.md.
    for (const double target : {20000.0, 31250.0, 48000.0, 82500.0, 96000.0})
    {
        SushiDSP::Resampler resampler;
        resampler.prepare(48000.0, target, 256u, 20000.0 / 48000.0);
        const std::vector<float> ones(48000u, 1.0f);
        const std::vector<float> output = stream(resampler, ones, 256u);
        ASSERT_GT(output.size(), 4000u) << target;
        for (std::size_t n = 2000; n < output.size() - 200; ++n)
        {
            ASSERT_NEAR(output[n], 1.0f, 1e-3f) << target << " " << n;
        }
    }
}

TEST(Resampler, ResamplerRoundTripNullsAtTheDd3SlowClock)
{
    // The 7 kHz content sits inside the 8000 Hz passband. See tests/foundation/math/README.md.
    const std::vector<float> input = multitone({100.0, 1000.0, 3000.0, 7000.0}, 48000.0, 96000u);
    SushiDSP::Resampler down, up;
    down.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    up.prepare(20000.0, 48000.0, 256u, 20000.0 / 82500.0);
    const std::vector<float> narrow = stream(down, input, 256u);
    const std::vector<float> wide = stream(up, narrow, 256u);
    const double residual = settled_null_test(input, wide);
    std::printf("[ measure ] resampler round trip 48k -> 20k -> 48k: %.2f dB\n", residual);
    EXPECT_LE(residual, -80.0);
}

TEST(Resampler, ResamplerRoundTripNullsAtTheDd3FastClockAndTheSpx90Ratio)
{
    // 12 kHz content survives at the SPX90 ratio. See tests/foundation/math/README.md.
    struct Case
    {
            double middle_rate;
            std::vector<double> partials;
    };
    const Case cases[2] = {
        {82500.0, {100.0, 1000.0, 7000.0, 15000.0}},
        {31250.0, {100.0, 1000.0, 7000.0, 12000.0}},
    };
    for (const Case& item : cases)
    {
        const std::vector<float> input = multitone(item.partials, 48000.0, 96000u);
        SushiDSP::Resampler down, up;
        down.prepare(48000.0, item.middle_rate, 256u, 31250.0 / 48000.0);
        up.prepare(item.middle_rate, 48000.0, 512u, 48000.0 / 82500.0);
        const std::vector<float> middle = stream(down, input, 256u);
        const std::vector<float> back = stream(up, middle, 256u);
        const double residual = settled_null_test(input, back);
        std::printf("[ measure ] resampler round trip 48k -> %.0f -> 48k: %.2f dB\n",
                    item.middle_rate, residual);
        EXPECT_LE(residual, -80.0) << item.middle_rate;
    }
}

TEST(Resampler, ResamplerSizesTheHistoryForAnUpsampleOnlyInstance)
{
    // The history is sized on min(1, minimum_ratio). See tests/foundation/math/README.md.
    const std::vector<float> input = multitone({100.0, 1000.0, 7000.0}, 48000.0, 24000u);
    SushiDSP::Resampler up, down;
    up.prepare(48000.0, 192000.0, 8u, 2.0);
    down.prepare(192000.0, 48000.0, 128u, 48000.0 / 192000.0);
    const std::vector<float> fast = stream(up, input, 8u);
    const std::vector<float> back = stream(down, fast, 128u);
    const double residual = settled_null_test(input, back);
    std::printf("[ measure ] resampler round trip 48k -> 192k -> 48k: %.2f dB\n", residual);
    EXPECT_LE(residual, -80.0);
}

TEST(Resampler, ResamplerRejectsAToneAboveTheOutputNyquist)
{
    // 15 kHz into 20 kHz would fold to 5000 Hz. See tests/foundation/math/README.md.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    std::vector<float> input(400000u, 0.0f);
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        input[n] = 0.8f * static_cast<float>(
                              std::sin(2.0 * M_PI * 15000.0 * static_cast<double>(n) / 48000.0));
    }
    const std::vector<float> output = stream(resampler, input, 256u);
    ASSERT_GT(output.size(), 2u * SushiDSP::measure::K_FFT_SIZE);
    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(
        output, output.size() - SushiDSP::measure::K_FFT_SIZE);
    const double image =
        SushiDSP::measure::peak_near(spectrum, SushiDSP::measure::bin_of(5000.0, 20000.0));
    // The 0 dB reference is the input at its own rate. See tests/foundation/math/README.md.
    const auto reference_spectrum = SushiDSP::measure::magnitude_spectrum_db(
        input, input.size() - SushiDSP::measure::K_FFT_SIZE);
    const double reference = SushiDSP::measure::peak_near(
        reference_spectrum, SushiDSP::measure::bin_of(15000.0, 48000.0));
    std::printf("[ measure ] resampler 15 kHz -> 20 kHz image at 5 kHz: %.2f dB\n",
                image - reference);
    EXPECT_LE(image - reference, -90.0);
}

TEST(Resampler, ResamplerPassesAToneAtThePassbandEdgeAtItsLevel)
{
    // 0.399 of the source rate is inside the 0.4 passband edge. See tests/foundation/math/README.md.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 96000.0, 256u, 1.0);
    const double frequency =
        SushiDSP::measure::bin_exact_frequency(19152.0, 96000.0, SushiDSP::measure::K_FFT_SIZE);
    std::vector<float> input(400000u, 0.0f);
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        input[n] = 0.5f * static_cast<float>(
                              std::sin(2.0 * M_PI * frequency * static_cast<double>(n) / 48000.0));
    }
    const std::vector<float> output = stream(resampler, input, 256u);
    ASSERT_GT(output.size(), 2u * SushiDSP::measure::K_FFT_SIZE);
    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(
        output, output.size() - SushiDSP::measure::K_FFT_SIZE);
    const double level =
        SushiDSP::measure::peak_near(spectrum, SushiDSP::measure::bin_of(frequency, 96000.0));
    const auto reference_spectrum = SushiDSP::measure::magnitude_spectrum_db(
        input, input.size() - SushiDSP::measure::K_FFT_SIZE);
    const double reference = SushiDSP::measure::peak_near(
        reference_spectrum, SushiDSP::measure::bin_of(frequency, 48000.0));
    std::printf("[ measure ] resampler 19.15 kHz passband edge, 48k -> 96k: %.3f dB\n",
                level - reference);
    EXPECT_NEAR(level - reference, 0.0, 0.10);
}

TEST(Resampler, ResamplerSuppressesTheImageOfAnUpsampledTone)
{
    // The image of f sits at 48000 - f, inside the stopband. See tests/foundation/math/README.md.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 96000.0, 256u, 1.0);
    const double frequency =
        SushiDSP::measure::bin_exact_frequency(14400.0, 96000.0, SushiDSP::measure::K_FFT_SIZE);
    std::vector<float> input(400000u, 0.0f);
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        input[n] = 0.5f * static_cast<float>(
                              std::sin(2.0 * M_PI * frequency * static_cast<double>(n) / 48000.0));
    }
    const std::vector<float> output = stream(resampler, input, 256u);
    ASSERT_GT(output.size(), 2u * SushiDSP::measure::K_FFT_SIZE);
    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(
        output, output.size() - SushiDSP::measure::K_FFT_SIZE);
    const double tone =
        SushiDSP::measure::peak_near(spectrum, SushiDSP::measure::bin_of(frequency, 96000.0));
    const double image = SushiDSP::measure::peak_near(
        spectrum, SushiDSP::measure::bin_of(48000.0 - frequency, 96000.0));
    std::printf("[ measure ] resampler 14.4 kHz image at 33.6 kHz, 48k -> 96k: %.2f dB\n",
                image - tone);
    EXPECT_LE(image - tone, -95.0);
}

TEST(Resampler, ResamplerOutputCountFollowsTheRatioWithoutDrift)
{
    // The output count stays within two samples of the ratio. See tests/foundation/math/README.md.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    const std::vector<float> silence(256u, 0.0f);
    std::vector<float> scratch(resampler.maximum_output_frames(256u) + 8u, 0.0f);
    std::uint64_t produced = 0;
    for (std::uint32_t block = 0; block < 4000u; ++block)
    {
        produced += resampler.process(silence.data(), 256u, scratch.data(),
                                      static_cast<std::uint32_t>(scratch.size()));
    }
    // 4000 * 256 = 1 024 000 input frames; 1 024 000 * 20000/48000 = 426 666.67 outputs.
    EXPECT_NEAR(static_cast<double>(produced), 426666.67, 80.0);
}

TEST(Resampler, ResamplerResumesAfterAnUnderSizedOutputBuffer)
{
    // Deferred outputs come out on the next call. See tests/foundation/math/README.md.
    const std::vector<float> input = multitone({100.0, 1000.0, 7000.0}, 48000.0, 8192u);
    SushiDSP::Resampler reference, starved;
    reference.prepare(48000.0, 48000.0, 256u, 20000.0 / 48000.0);
    starved.prepare(48000.0, 48000.0, 256u, 20000.0 / 48000.0);
    const std::vector<float> expected = stream(reference, input, 256u);

    std::vector<float> actual;
    std::vector<float> scratch(1024u, 0.0f);
    for (std::size_t block = 0; block * 256u < input.size(); ++block)
    {
        // Room for 100 outputs on every fourth push, where about 256 are due.
        const std::uint32_t capacity = (block % 4u == 0u) ? 100u : 1024u;
        const std::uint32_t produced =
            starved.process(input.data() + block * 256u, 256u, scratch.data(), capacity);
        actual.insert(actual.end(), scratch.begin(), scratch.begin() + produced);
    }
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t n = 0; n < expected.size(); ++n)
    {
        ASSERT_FLOAT_EQ(actual[n], expected[n]) << n;
    }
}

TEST(Resampler, ResamplerBoundsThePhaseWhenTheOutputBufferStaysTooSmall)
{
    // process() drops the backlog instead of wrapping. See tests/foundation/math/README.md.
    const std::vector<float> input = multitone({100.0, 1000.0, 7000.0}, 48000.0, 65536u);
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 48000.0, 256u, 20000.0 / 48000.0);
    const std::size_t blocks = input.size() / 256u;
    std::vector<float> scratch(1024u, 0.0f);
    std::vector<float> tail;
    for (std::size_t block = 0; block < blocks; ++block)
    {
        const std::uint32_t capacity = (block + 8u < blocks) ? 100u : 1024u;
        const std::uint32_t produced =
            resampler.process(input.data() + block * 256u, 256u, scratch.data(), capacity);
        ASSERT_GT(produced, 0u) << block;
        ASSERT_LE(produced, capacity) << block;
        for (std::uint32_t j = 0; j < produced; ++j)
        {
            ASSERT_LE(std::abs(scratch[j]), 1.0f) << block << " " << j;
        }
        if (block + 8u >= blocks)
        {
            tail.insert(tail.end(), scratch.begin(), scratch.begin() + produced);
        }
    }
    ASSERT_GT(tail.size(), 1024u);
    const std::size_t latency = resampler.latency_samples();
    const std::vector<float> window(input.end() -
                                        static_cast<std::ptrdiff_t>(latency + tail.size()),
                                    input.end() - static_cast<std::ptrdiff_t>(latency));
    const double residual = SushiDSP::measure::null_test(window, tail, 256u);
    std::printf("[ measure ] resampler starved tail against the live input: %.2f dB\n", residual);
    EXPECT_LE(residual, -60.0);
}

TEST(Resampler, ResamplerReclampsThePhaseWhenARateChangeWidensTheKernel)
{
    // y[n-1] + y[n+1] = 2*cos(w)*y[n] for a sampled sinusoid. See tests/foundation/math/README.md.
    std::vector<float> input(262144u, 0.0f);
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        input[n] =
            static_cast<float>(std::sin(2.0 * M_PI * 100.0 * static_cast<double>(n) / 48000.0));
    }
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 48000.0, 256u, 3000.0 / 48000.0);
    std::vector<float> scratch(1024u, 0.0f);
    const std::size_t starved_blocks = 64u;
    for (std::size_t block = 0; block < starved_blocks; ++block)
    {
        resampler.process(input.data() + block * 256u, 256u, scratch.data(), 100u);
    }
    resampler.set_rates(48000.0, 3000.0);

    std::vector<float> output;
    for (std::size_t block = starved_blocks; block < input.size() / 256u; ++block)
    {
        const std::uint32_t produced =
            resampler.process(input.data() + block * 256u, 256u, scratch.data(),
                              static_cast<std::uint32_t>(scratch.size()));
        for (std::uint32_t j = 0; j < produced; ++j)
        {
            ASSERT_TRUE(std::isfinite(scratch[j])) << block << " " << j;
            ASSERT_LE(std::abs(scratch[j]), 1.0f) << block << " " << j;
        }
        output.insert(output.end(), scratch.begin(), scratch.begin() + produced);
    }

    ASSERT_GT(output.size(), 1000u);
    const double recurrence = 2.0 * std::cos(2.0 * M_PI * 100.0 / 3000.0);
    double worst = 0.0;
    for (std::size_t n = 1; n + 1 < output.size(); ++n)
    {
        const double residual =
            std::abs(static_cast<double>(output[n - 1]) + static_cast<double>(output[n + 1]) -
                     recurrence * static_cast<double>(output[n]));
        worst = std::max(worst, residual);
    }
    std::printf("[ measure ] resampler phase re-clamp, sinusoid identity residual: %.3e\n", worst);
    EXPECT_LE(worst, 1e-5);
}

TEST(Resampler, ResamplerLatencyIsThirtyFourSamplesOfTheLowerRate)
{
    // The delay is 34 / min(source, target) seconds. See tests/foundation/math/README.md.
    struct Case
    {
            double source;
            double target;
            double expected_seconds;
            std::uint32_t expected_samples;
    };
    const Case cases[6] = {
        {48000.0, 48000.0, 34.0 / 48000.0, 34u},
        {48000.0, 82500.0, 34.0 / 48000.0, 34u},
        {82500.0, 48000.0, 34.0 / 48000.0, 58u}, // 34 / (48000/82500) = 58.4375 -> 58
        {48000.0, 20000.0, 34.0 / 20000.0, 82u}, // 34 / (20000/48000) = 81.6 -> 82
        {20000.0, 48000.0, 34.0 / 20000.0, 34u},
        {48000.0, 31250.0, 34.0 / 31250.0, 52u}, // 34 / (31250/48000) = 52.224 -> 52
    };
    for (const Case& item : cases)
    {
        SushiDSP::Resampler resampler;
        resampler.prepare(item.source, item.target, 256u, 20000.0 / 48000.0);
        EXPECT_NEAR(resampler.latency_seconds(), item.expected_seconds, 1e-12) << item.target;
        EXPECT_EQ(resampler.latency_samples(), item.expected_samples) << item.target;
    }
}

TEST(Resampler, ResamplerKeepsAToneAtItsFrequency)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    const double frequency =
        SushiDSP::measure::bin_exact_frequency(1000.0, 20000.0, SushiDSP::measure::K_FFT_SIZE);
    std::vector<float> input(400000u, 0.0f);
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        input[n] = 0.5f * static_cast<float>(
                              std::sin(2.0 * M_PI * frequency * static_cast<double>(n) / 48000.0));
    }
    const std::vector<float> output = stream(resampler, input, 256u);
    ASSERT_GT(output.size(), 2u * SushiDSP::measure::K_FFT_SIZE);
    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(
        output, output.size() - SushiDSP::measure::K_FFT_SIZE);
    const std::uint32_t expected_bin = SushiDSP::measure::bin_of(frequency, 20000.0);
    double best_bin = 0.0;
    double best_level = -1e300;
    for (std::uint32_t bin = 1; bin + 1 < spectrum.size(); ++bin)
    {
        if (spectrum[bin] > best_level)
        {
            best_level = spectrum[bin];
            best_bin = bin;
        }
    }
    EXPECT_NEAR(best_bin, static_cast<double>(expected_bin), 1.0);
}

TEST(Resampler, ResamplerRateChangeMidStreamIsContinuous)
{
    // At 20 kHz a 100 Hz sine steps at most 0.0314. See tests/foundation/math/README.md.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 82500.0, 8u, 20000.0 / 48000.0);
    std::vector<float> scratch(resampler.maximum_output_frames(8u) + 8u, 0.0f);
    std::vector<float> output;
    float previous = 0.0f;
    for (std::uint32_t sub_block = 0; sub_block < 2000u; ++sub_block)
    {
        const double position = static_cast<double>(sub_block) / 2000.0;
        resampler.set_rates(48000.0, 82500.0 * std::pow(4.125, -position));
        float input[8];
        for (std::uint32_t j = 0; j < 8u; ++j)
        {
            const std::size_t n = sub_block * 8u + j;
            input[j] =
                static_cast<float>(std::sin(2.0 * M_PI * 100.0 * static_cast<double>(n) / 48000.0));
        }
        const std::uint32_t produced = resampler.process(
            input, 8u, scratch.data(), static_cast<std::uint32_t>(scratch.size()));
        for (std::uint32_t j = 0; j < produced; ++j)
        {
            if (!output.empty())
            {
                ASSERT_LE(std::abs(scratch[j] - previous), 0.05f) << sub_block << " " << j;
            }
            previous = scratch[j];
            output.push_back(scratch[j]);
        }
    }
    EXPECT_GT(output.size(), 10000u);
}

TEST(Resampler, ResamplerResetReturnsItToSilence)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    const std::vector<float> loud(4096u, 0.9f);
    stream(resampler, loud, 256u);
    resampler.reset();
    const std::vector<float> silence(4096u, 0.0f);
    for (const float sample : stream(resampler, silence, 256u))
    {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(Resampler, ProcessPerformsNoHeapAllocation)
{
    // At the slowest clock interpolate() touches 164 taps. See tests/foundation/math/README.md.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 440.0f * i / 48000.0f);
    }
    std::vector<float> output(resampler.maximum_output_frames(256u) + 16u, 0.0f);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 200; ++block)
    {
        resampler.process(input.data(), 256u, output.data(),
                          static_cast<std::uint32_t>(output.size()));
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Resampler, RateChangePerformsNoHeapAllocation)
{
    // One 256-frame block calls set_rates 32 times. See tests/foundation/math/README.md.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 82500.0, 8u, 20000.0 / 48000.0);

    std::vector<float> input(8);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.25f * std::sin(2.0f * static_cast<float>(M_PI) * 440.0f * i / 48000.0f);
    }
    std::array<float, 64> output{};

    SushiDSP::test::AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 200; ++block)
    {
        for (int sub_block = 0; sub_block < 32; ++sub_block)
        {
            const double clock_hz =
                20000.0 + (82500.0 - 20000.0) * static_cast<double>(sub_block) / 31.0;
            resampler.set_rates(48000.0, clock_hz);
            resampler.process(input.data(), 8u, output.data(),
                              static_cast<std::uint32_t>(output.size()));
        }
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
