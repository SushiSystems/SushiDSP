/**************************************************************************/
/* test_resampler.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_resampler.cpp
 * @brief Tests for the arbitrary-ratio polyphase Resampler.
 * @author Mustafa Garip & Sushi Systems
 */

#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/Resampler.hpp"
#include "measure/NullTest.hpp"
#include "measure/Spectrum.hpp"

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
    constexpr std::size_t kSettlingFrames = 256;

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
            const std::uint32_t produced = resampler.process(
                input.data() + position, frames, scratch.data(),
                static_cast<std::uint32_t>(scratch.size()));
            output.insert(output.end(), scratch.begin(), scratch.begin() + produced);
        }
        return output;
    }

    /// @brief Residual of b against a once both have left their start-up region.
    /// @param a Reference signal, sample-aligned with b.
    /// @param b Signal under test.
    /// @pre Both signals hold more than kSettlingFrames samples.
    /// @return Residual in dB.
    double settled_null_test(const std::vector<float>& a, const std::vector<float>& b)
    {
        const std::vector<float> settled_a(a.begin() + kSettlingFrames, a.end());
        const std::vector<float> settled_b(b.begin() + kSettlingFrames, b.end());
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
                value += amplitude * std::sin(2.0 * M_PI * frequency * static_cast<double>(n) / sample_rate);
            }
            signal[n] = static_cast<float>(value);
        }
        return signal;
    }
}

TEST(Unit_Dsp, ResamplerHoldsADcLevelAtEveryRatio)
{
    // Every polyphase branch has unity DC gain and the decimating scale factor restores it, so a
    // constant comes back as the same constant whichever way the rate moves.
    for (const double target : { 20000.0, 31250.0, 48000.0, 82500.0, 96000.0 })
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

TEST(Unit_Dsp, ResamplerRoundTripNullsAtTheDd3SlowClock)
{
    // 48 kHz -> 20 kHz -> 48 kHz. The content stops at 7 kHz, which is the pedal's own delay-path
    // limit and is inside the prototype's passband (0.400 * 20000 = 8000 Hz, N1), so the round
    // trip is limited by the kernel's own error: -100 dB stopband, -108 dB interpolation floor.
    // -80 dB is asserted with margin for the float buffers.
    const std::vector<float> input = multitone({ 100.0, 1000.0, 3000.0, 7000.0 }, 48000.0, 96000u);
    SushiDSP::Resampler down, up;
    down.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    up.prepare(20000.0, 48000.0, 256u, 20000.0 / 82500.0);
    const std::vector<float> narrow = stream(down, input, 256u);
    const std::vector<float> wide = stream(up, narrow, 256u);
    const double residual = settled_null_test(input, wide);
    std::printf("[ measure ] resampler round trip 48k -> 20k -> 48k: %.2f dB\n", residual);
    EXPECT_LE(residual, -80.0);
}

TEST(Unit_Dsp, ResamplerRoundTripNullsAtTheDd3FastClockAndTheSpx90Ratio)
{
    // 48 kHz -> 82.5 kHz -> 48 kHz, and 48 kHz -> 31.25 kHz -> 48 kHz (the SPX90's 1.536).
    // At the SPX90 ratio the passband reaches 0.400 * 31250 = 12500 Hz, which is exactly the
    // unit's published 20 Hz - 12 kHz band, so 12 kHz content must survive.
    struct Case { double middle_rate; std::vector<double> partials; };
    const Case cases[2] = {
        { 82500.0, { 100.0, 1000.0, 7000.0, 15000.0 } },
        { 31250.0, { 100.0, 1000.0, 7000.0, 12000.0 } },
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

TEST(Unit_Dsp, ResamplerSizesTheHistoryForAnUpsampleOnlyInstance)
{
    // An interpolating ratio never widens the kernel past its own 34 input samples, so a
    // minimum_ratio above 1 must size the history on min(1, minimum_ratio). Dividing by the
    // minimum_ratio alone reserves 64 slots here against a live reach of 76 and the ring wraps
    // on top of itself, which shows up as a round trip that does not null.
    const std::vector<float> input = multitone({ 100.0, 1000.0, 7000.0 }, 48000.0, 24000u);
    SushiDSP::Resampler up, down;
    up.prepare(48000.0, 192000.0, 8u, 2.0);
    down.prepare(192000.0, 48000.0, 128u, 48000.0 / 192000.0);
    const std::vector<float> fast = stream(up, input, 8u);
    const std::vector<float> back = stream(down, fast, 128u);
    const double residual = settled_null_test(input, back);
    std::printf("[ measure ] resampler round trip 48k -> 192k -> 48k: %.2f dB\n", residual);
    EXPECT_LE(residual, -80.0);
}

TEST(Unit_Dsp, ResamplerRejectsAToneAboveTheOutputNyquist)
{
    // 15 kHz into a 20 kHz output rate would fold to |15000 - 20000| = 5000 Hz. Normalised to
    // the lower rate, 15 kHz is 0.75 cycles/sample, deep in the prototype's stopband, so the
    // image must land below -90 dB relative to the input tone.
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    std::vector<float> input(400000u, 0.0f);
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        input[n] = 0.8f * static_cast<float>(std::sin(2.0 * M_PI * 15000.0 * static_cast<double>(n) / 48000.0));
    }
    const std::vector<float> output = stream(resampler, input, 256u);
    ASSERT_GT(output.size(), 2u * SushiDSP::measure::kFftSize);
    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(
        output, output.size() - SushiDSP::measure::kFftSize);
    const double image = SushiDSP::measure::peak_near(spectrum, SushiDSP::measure::bin_of(5000.0, 20000.0));
    // 0 dB reference: what the same tone would read if it had passed unattenuated. Use the input
    // itself, analysed at its own rate, so the two spectra share a window and a length.
    const auto reference_spectrum = SushiDSP::measure::magnitude_spectrum_db(
        input, input.size() - SushiDSP::measure::kFftSize);
    const double reference = SushiDSP::measure::peak_near(
        reference_spectrum, SushiDSP::measure::bin_of(15000.0, 48000.0));
    std::printf("[ measure ] resampler 15 kHz -> 20 kHz image at 5 kHz: %.2f dB\n", image - reference);
    EXPECT_LE(image - reference, -90.0);
}

TEST(Unit_Dsp, ResamplerOutputCountFollowsTheRatioWithoutDrift)
{
    // Over a million frames the accumulated output count must stay within a sample or two of
    // input_frames * ratio; a phase accumulator that rebases badly drifts here and nowhere else.
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

TEST(Unit_Dsp, ResamplerResumesAfterAnUnderSizedOutputBuffer)
{
    // A short output buffer defers the outputs it had no room for. While the ring still holds
    // their history the next call emits them, so the stream is the one an always-fed instance
    // produces, sample for sample. Every position here is an integer, so equality is exact.
    const std::vector<float> input = multitone({ 100.0, 1000.0, 7000.0 }, 48000.0, 8192u);
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
        const std::uint32_t produced = starved.process(input.data() + block * 256u, 256u,
                                                       scratch.data(), capacity);
        actual.insert(actual.end(), scratch.begin(), scratch.begin() + produced);
    }
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t n = 0; n < expected.size(); ++n)
    {
        ASSERT_FLOAT_EQ(actual[n], expected[n]) << n;
    }
}

TEST(Unit_Dsp, ResamplerBoundsThePhaseWhenTheOutputBufferStaysTooSmall)
{
    // Truncation that never lets up cannot be deferred forever: the ring holds one span of
    // history, and past that process() drops the backlog instead of reading slots it has
    // overwritten. The proof that it drops rather than wraps is the tail: once the buffer is
    // big enough again, the output lines up with the live input to better than -60 dB.
    const std::vector<float> input = multitone({ 100.0, 1000.0, 7000.0 }, 48000.0, 65536u);
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 48000.0, 256u, 20000.0 / 48000.0);
    const std::size_t blocks = input.size() / 256u;
    std::vector<float> scratch(1024u, 0.0f);
    std::vector<float> tail;
    for (std::size_t block = 0; block < blocks; ++block)
    {
        const std::uint32_t capacity = (block + 8u < blocks) ? 100u : 1024u;
        const std::uint32_t produced = resampler.process(input.data() + block * 256u, 256u,
                                                         scratch.data(), capacity);
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
    const std::vector<float> window(input.end() - static_cast<std::ptrdiff_t>(latency + tail.size()),
                                    input.end() - static_cast<std::ptrdiff_t>(latency));
    const double residual = SushiDSP::measure::null_test(window, tail, 256u);
    std::printf("[ measure ] resampler starved tail against the live input: %.2f dB\n", residual);
    EXPECT_LE(residual, -60.0);
}

TEST(Unit_Dsp, ResamplerLatencyIsThirtyFourSamplesOfTheLowerRate)
{
    // DR1+DR2 plan N1, as the Task 1.1 ruling widened it: the delay is
    // kResamplerHalfWidthZeroCrossings = 34 divided by min(source, target) seconds at every ratio.
    struct Case { double source; double target; double expected_seconds; std::uint32_t expected_samples; };
    const Case cases[6] = {
        { 48000.0, 48000.0, 34.0 / 48000.0, 34u },
        { 48000.0, 82500.0, 34.0 / 48000.0, 34u },
        { 82500.0, 48000.0, 34.0 / 48000.0, 58u },   // 34 / (48000/82500) = 58.4375 -> 58
        { 48000.0, 20000.0, 34.0 / 20000.0, 82u },   // 34 / (20000/48000) = 81.6 -> 82
        { 20000.0, 48000.0, 34.0 / 20000.0, 34u },
        { 48000.0, 31250.0, 34.0 / 31250.0, 52u },   // 34 / (31250/48000) = 52.224 -> 52
    };
    for (const Case& item : cases)
    {
        SushiDSP::Resampler resampler;
        resampler.prepare(item.source, item.target, 256u, 20000.0 / 48000.0);
        EXPECT_NEAR(resampler.latency_seconds(), item.expected_seconds, 1e-12) << item.target;
        EXPECT_EQ(resampler.latency_samples(), item.expected_samples) << item.target;
    }
}

TEST(Unit_Dsp, ResamplerKeepsAToneAtItsFrequency)
{
    SushiDSP::Resampler resampler;
    resampler.prepare(48000.0, 20000.0, 256u, 20000.0 / 48000.0);
    const double frequency = SushiDSP::measure::bin_exact_frequency(1000.0, 20000.0, SushiDSP::measure::kFftSize);
    std::vector<float> input(400000u, 0.0f);
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        input[n] = 0.5f * static_cast<float>(std::sin(2.0 * M_PI * frequency * static_cast<double>(n) / 48000.0));
    }
    const std::vector<float> output = stream(resampler, input, 256u);
    ASSERT_GT(output.size(), 2u * SushiDSP::measure::kFftSize);
    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(
        output, output.size() - SushiDSP::measure::kFftSize);
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

TEST(Unit_Dsp, ResamplerRateChangeMidStreamIsContinuous)
{
    // The DD-3 usage: set_rates() every 8 frames while the target sweeps 82.5 kHz -> 20 kHz.
    // A phase reset or a bank rebuild shows up here as a step; a correct accumulator does not.
    // Reference: a 100 Hz unit sine sampled at 20 kHz moves at most 2*pi*100/20000 = 0.0314 per
    // sample, so 0.05 leaves room for the sweep's own rate change and none for a click.
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
            input[j] = static_cast<float>(std::sin(2.0 * M_PI * 100.0 * static_cast<double>(n) / 48000.0));
        }
        const std::uint32_t produced =
            resampler.process(input, 8u, scratch.data(), static_cast<std::uint32_t>(scratch.size()));
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

TEST(Unit_Dsp, ResamplerResetReturnsItToSilence)
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
