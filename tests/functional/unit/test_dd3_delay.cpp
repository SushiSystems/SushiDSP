/**************************************************************************/
/* test_dd3_delay.cpp                                                     */
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
 * @file test_dd3_delay.cpp
 * @brief Tests for the Dd3Delay node.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// which GCC/Clang's libstdc++/libc++ define unconditionally.
#define _USE_MATH_DEFINES

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "SushiDSP/dd3/Dd3Delay.hpp"
#include "measure/Render.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr std::uint32_t kBlock = 256u;

    /// @brief Streams a stimulus through an already-prepared node, block by block.
    /// @param node The node under test.
    /// @param stimulus Input samples; its length must be a whole number of blocks.
    /// @return The node's output.
    std::vector<float> stream(SushiDSP::IAudioNode& node, const std::vector<float>& stimulus)
    {
        std::vector<float> output(stimulus.size(), 0.0f);
        for (std::size_t offset = 0; offset + kBlock <= stimulus.size(); offset += kBlock)
        {
            const float* inputs[1] = { stimulus.data() + offset };
            float* outputs[1] = { output.data() + offset };
            SushiDSP::ProcessContext context{ outputs, inputs, kBlock, 0.0 };
            node.process(context);
        }
        return output;
    }

    /// @brief Root mean square of a half-open window.
    /// @param samples The signal.
    /// @param begin First sample of the window.
    /// @param length Window length; the window is clipped to the signal.
    /// @return The RMS.
    double window_rms(const std::vector<float>& samples, std::size_t begin, std::size_t length)
    {
        double energy = 0.0;
        std::size_t counted = 0;
        for (std::size_t n = begin; n < begin + length && n < samples.size(); ++n)
        {
            energy += static_cast<double>(samples[n]) * samples[n];
            ++counted;
        }
        return counted > 0 ? std::sqrt(energy / static_cast<double>(counted)) : 0.0;
    }

    /// @brief A burst of 3 kHz followed by silence, the stimulus every echo test uses.
    /// @param length Total length in samples.
    /// @return The stimulus.
    std::vector<float> burst_stimulus(std::size_t length)
    {
        std::vector<float> input(length, 0.0f);
        for (std::size_t n = 0; n < 480; ++n)      // 10 ms, well inside the 7 kHz band
        {
            input[n] = 0.6f * static_cast<float>(
                std::sin(2.0 * M_PI * 3000.0 * static_cast<double>(n) / kSampleRate));
        }
        return input;
    }
}

TEST(Unit_Dsp, Dd3DelayDeclaresItselfCorrectly)
{
    SushiDSP::Dd3Delay node;
    node.prepare({ kSampleRate, kBlock, 1u });
    EXPECT_EQ(node.ports().input_channels, 1u);
    EXPECT_EQ(node.ports().output_channels, 1u);
    EXPECT_STREQ(node.type_id(), "sushidsp.dd3");
    // Ruling 5: the dry path is sample-aligned, so there is nothing for a host to compensate.
    EXPECT_EQ(node.latency_samples(), 0u);
}

TEST(Unit_Dsp, Dd3DelayAtZeroLevelIsAWire)
{
    // Ruling 9: the dry branch carries no shelf and no trim. With E.LEVEL down, in == out.
    SushiDSP::Dd3Delay node;
    node.set_level(0.0f);
    node.set_feedback(0.0f);
    const auto input = SushiDSP::measure::sine_stimulus(440.0, kSampleRate, 48000u, 0.5f);
    const auto output = SushiDSP::measure::render(node, input, kSampleRate, kBlock);
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        ASSERT_NEAR(output[n], input[n], 1e-5f) << n;
    }
}

TEST(Unit_Dsp, Dd3DelayWetDelayMatchesThePlansArithmetic)
{
    // N2, with the resampler round trip read off the committed prototype rather than off the
    // plan's draft: Task 1.1 grew the half width from 32 to 34 zero crossings, so the two
    // conversions cost 68 / min(fs, 48000) between them, not 64.
    //   Mode L at TIME = 0: 16384/82500 + 68/48000 + 8/48000
    //     = 0.198593939 + 0.001416667 + 0.000166667 = 0.200177273 s = 9608.51 samples.
    //   Mode S at TIME = 0: 1024/82500 + 68/48000 + 8/48000 = 0.013995455 s = 671.78 samples.
    struct Case { SushiDSP::dd3::Dd3Mode mode; double expected_seconds; };
    const Case cases[2] = {
        { SushiDSP::dd3::Dd3Mode::Short, 0.013995455 },
        { SushiDSP::dd3::Dd3Mode::Long,  0.200177273 },
    };
    for (const Case& item : cases)
    {
        SushiDSP::Dd3Delay node;
        node.set_mode(item.mode);
        node.set_time(0.0f);
        node.set_feedback(0.0f);
        node.set_level(1.0f);
        node.prepare({ kSampleRate, kBlock, 1u });
        EXPECT_NEAR(node.wet_delay_seconds(), item.expected_seconds, 1e-6);

        // And the measured delay agrees: an impulse comes back once, near that time.
        std::vector<float> input(static_cast<std::size_t>(0.5 * kSampleRate), 0.0f);
        input[0] = 1.0f;
        const auto output = SushiDSP::measure::render(node, input, kSampleRate, kBlock);
        std::size_t peak_at = 0;
        float peak = 0.0f;
        for (std::size_t n = 8; n < output.size(); ++n)
        {
            if (std::abs(output[n]) > peak) { peak = std::abs(output[n]); peak_at = n; }
        }
        const double expected_samples = item.expected_seconds * kSampleRate;
        std::printf("[ measure ] DD-3 wet echo at %zu samples, predicted %.2f\n",
                    peak_at, expected_samples);
        // The echo has been through two 7 kHz cascades, so its peak spreads over a few samples
        // and lands about 13 late; 32 samples is tight enough to catch a wrong N, a wrong fs or
        // a missed round trip.
        EXPECT_NEAR(static_cast<double>(peak_at), expected_samples, 32.0);
    }
}

TEST(Unit_Dsp, Dd3DelayFillsEveryBlockSizeAndStaysFinite)
{
    for (const std::uint32_t block : { 1u, 7u, 8u, 9u, 64u, 255u, 256u })
    {
        SushiDSP::Dd3Delay node;
        node.set_level(0.7f);
        node.set_feedback(0.6f);
        node.set_time(0.4f);
        const auto input = SushiDSP::measure::sine_stimulus(220.0, kSampleRate, 48000u, 0.8f);
        const auto output = SushiDSP::measure::render(node, input, kSampleRate, block);
        ASSERT_EQ(output.size(), input.size()) << block;
        for (const float sample : output)
        {
            ASSERT_TRUE(std::isfinite(sample)) << block;
        }
    }
}

TEST(Unit_Dsp, Dd3DelayResetReturnsItToSilence)
{
    SushiDSP::Dd3Delay node;
    node.set_level(1.0f);
    node.set_feedback(0.8f);
    node.prepare({ kSampleRate, kBlock, 1u });

    std::vector<float> loud(24000u, 0.0f);
    for (std::size_t n = 0; n < loud.size(); ++n)
    {
        loud[n] = 0.9f * static_cast<float>(
            std::sin(2.0 * M_PI * 300.0 * static_cast<double>(n) / kSampleRate));
    }
    stream(node, loud);

    node.reset();
    const std::vector<float> silence(48000u, 0.0f);
    const auto after = stream(node, silence);
    for (const float sample : after)
    {
        EXPECT_NEAR(sample, 0.0f, 1e-6f);
    }
}

TEST(Unit_Dsp, Dd3DelayRepeatsDecayWithFeedback)
{
    // Dossier 3, "Self-oscillation": every repeat is re-band-limited by both 7 kHz cascades and
    // re-quantised. Assert the loop is a decaying one below the top of the knob: the second
    // echo carries less energy than the first.
    SushiDSP::Dd3Delay node;
    node.set_mode(SushiDSP::dd3::Dd3Mode::Short);
    node.set_time(0.0f);
    node.set_feedback(0.7f);
    node.set_level(1.0f);
    const auto input = burst_stimulus(static_cast<std::size_t>(1.0 * kSampleRate));
    const auto output = SushiDSP::measure::render(node, input, kSampleRate, kBlock);
    // Echoes land near 672 and 1344 samples (N2's 13.995 ms, twice).
    const double first = window_rms(output, 600u, 600u);
    const double second = window_rms(output, 1272u, 600u);
    EXPECT_GT(first, 0.0);
    EXPECT_LT(second, first);
    std::printf("[ measure ] DD-3 echo levels: first %.6e, second %.6e (ratio %.3f)\n",
                first, second, second / first);
}

TEST(Unit_Dsp, Dd3DelaySelfOscillatesOnlyAtTheTopOfTheKnob)
{
    // kFeedbackPathGain exists so that kMaximumFeedback is the loop gain it claims to be: the
    // loop's forward gain peaks at 100.511 at 5804 Hz, and without the trim the node would
    // regenerate from F.BACK 0.01 upward. Below the top of the knob the tail dies; at the top
    // it sustains.
    const auto input = burst_stimulus(static_cast<std::size_t>(2.0 * kSampleRate));
    double tails[2] = { 0.0, 0.0 };
    const float positions[2] = { 0.7f, 1.0f };
    for (int i = 0; i < 2; ++i)
    {
        SushiDSP::Dd3Delay node;
        node.set_mode(SushiDSP::dd3::Dd3Mode::Short);
        node.set_time(0.0f);
        node.set_feedback(positions[i]);
        node.set_level(1.0f);
        const auto output = SushiDSP::measure::render(node, input, kSampleRate, kBlock);
        tails[i] = window_rms(output, 48000u, 48000u);
    }
    std::printf("[ measure ] DD-3 tail rms in the second second: F.BACK 0.7 %.3e, 1.0 %.3e\n",
                tails[0], tails[1]);
    EXPECT_LT(tails[0], 1e-4);
    EXPECT_GT(tails[1], 5e-3);
}

TEST(Unit_Dsp, Dd3DelayHoldKeepsRepeatingWithNoInput)
{
    SushiDSP::Dd3Delay node;
    node.set_mode(SushiDSP::dd3::Dd3Mode::Long);
    node.set_time(0.0f);
    node.set_feedback(0.0f);
    node.set_level(1.0f);
    node.prepare({ kSampleRate, kBlock, 1u });

    std::vector<float> tone(static_cast<std::size_t>(0.25 * kSampleRate), 0.0f);
    for (std::size_t n = 0; n < tone.size(); ++n)
    {
        tone[n] = 0.5f * static_cast<float>(
            std::sin(2.0 * M_PI * 500.0 * static_cast<double>(n) / kSampleRate));
    }
    stream(node, tone);

    node.set_mode(SushiDSP::dd3::Dd3Mode::Hold);   // Ruling 14: same N, so the loop survives
    const std::vector<float> silence(48000u, 0.0f);
    const auto held = stream(node, silence);
    double energy = 0.0;
    for (std::size_t n = 24000; n < held.size(); ++n)
    {
        energy += static_cast<double>(held[n]) * held[n];
    }
    EXPECT_GT(energy, 1e-4);
}

TEST(Unit_Dsp, Dd3DelayHitsCheckPointFive)
{
    // Service note ladder, 1 kHz at 100 mVp-p in -> [5] 140 mVp-p of delay signal (dossier 1.0).
    // In this tree's units: 0.025 full scale in, 0.035 full scale of wet out.
    SushiDSP::Dd3Delay wet_node, dry_node;
    for (SushiDSP::Dd3Delay* node : { &wet_node, &dry_node })
    {
        node->set_mode(SushiDSP::dd3::Dd3Mode::Middle);
        node->set_time(0.5f);
        node->set_feedback(0.0f);
    }
    wet_node.set_level(1.0f);
    dry_node.set_level(0.0f);
    const auto input = SushiDSP::measure::sine_stimulus(1000.0, kSampleRate, 96000u, 0.025f);
    const auto with_wet = SushiDSP::measure::render(wet_node, input, kSampleRate, kBlock);
    const auto without = SushiDSP::measure::render(dry_node, input, kSampleRate, kBlock);
    double peak = 0.0;
    for (std::size_t n = 72000; n < with_wet.size(); ++n)
    {
        peak = std::max(peak, std::abs(static_cast<double>(with_wet[n] - without[n])));
    }
    std::printf("[ measure ] DD-3 check point [5]: %.5f full scale (target 0.035)\n", peak);
    EXPECT_NEAR(peak, 0.035, 0.05 * 0.035);
}

TEST(Unit_Dsp, Dd3DelayParametersRoundTripByNameAndByIndex)
{
    SushiDSP::Dd3Delay node;
    node.set_parameter("time", 0.8f);
    node.set_parameter("feedback", 0.25f);
    node.set_parameter("level", 0.6f);
    node.set_parameter("mode", 3.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("time"), 0.8f);
    EXPECT_FLOAT_EQ(node.get_parameter("feedback"), 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("level"), 0.6f);
    EXPECT_FLOAT_EQ(node.get_parameter("mode"), 3.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("nonsense"), 0.0f);
    node.set_parameter(static_cast<std::uint16_t>(0), 0.1f);
    EXPECT_FLOAT_EQ(node.get_parameter("time"), 0.1f);
    node.set_parameter(static_cast<std::uint16_t>(99), 0.1f);   // out of range: a no-op
    EXPECT_FLOAT_EQ(node.get_parameter("time"), 0.1f);
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::dd3_descriptor();
    EXPECT_STREQ(descriptor.id, "sushidsp.dd3");
    EXPECT_EQ(descriptor.parameter_count, 4u);
    EXPECT_EQ(descriptor.ports.input_channels, 1u);
    EXPECT_EQ(descriptor.ports.output_channels, 1u);
}
