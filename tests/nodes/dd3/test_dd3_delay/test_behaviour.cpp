/**************************************************************************/
/* test_behaviour.cpp                                                     */
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
 * @file test_behaviour.cpp
 * @brief Tests for the Dd3Delay node.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <SushiDSP/nodes/dd3/dd3_delay.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>

#include "support/tolerance.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    using Clock = SushiDSP::dd3::Dd3Clock;
    using Mode = SushiDSP::dd3::Dd3Mode;

    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr std::uint32_t K_BLOCK = 256u;

    /// @brief Unit roundoff of the float that carries the clock's control in the log domain.
    constexpr double K_FLOAT_ROUNDOFF = TestSupport::unit_roundoff<float>();

    /// @brief Half the resampler prototype's width in zero crossings; the two conversions cost
    /// twice this over min(clock, host). Restated, not read off the node, so a prototype that
    /// moves without this file moving fails here.
    constexpr double K_RESAMPLER_HALF_WIDTH = 34.0;

    /// @brief How far the argmax of an echo may sit from its prediction at TIME 0, in host
    /// samples. Characterized, not derived: the two 7 kHz cascades land the peak about 13 samples
    /// late and the resampler kernels spread it over 34 samples at this clock.
    constexpr double K_ECHO_PEAK_SLACK_SAMPLES = 32.0;

    /// @brief The least ratio of the third repeat's spectral centroid to the first's.
    /// Characterized: each pass through the two 7 kHz cascades darkens the repeat, and 0.95 is
    /// the margin the measured fall was accepted at.
    constexpr double K_CENTROID_FALL_RATIO = 0.95;

    /// @brief The tail rms below which a feedback setting under the top of the knob counts as
    /// dead. Characterized: 0.77 per round trip leaves 1e-8 of the burst after a second, so the
    /// bound sits far above the true tail.
    constexpr double K_DEAD_TAIL_RMS = 1e-4;

    /// @brief The tail rms above which the top of the knob counts as sustained. Characterized.
    constexpr double K_SUSTAINED_TAIL_RMS = 5e-3;

    /// @brief The energy above which a held loop counts as still repeating. Characterized.
    constexpr double K_HELD_ENERGY_FLOOR = 1e-4;

    /// @brief The relative band around check point [5]'s 0.035 full scale. Characterized: it holds
    /// the sampling of the peak (1 - cos(pi f / fs) = 0.2 %), the converter's half step and the
    /// compander's envelope ripple, whose sum was not derived to a single figure.
    constexpr double K_CHECK_POINT_FIVE_BAND = 0.05;

    /// @brief The service note's reading at check point [5], 1 kHz at 100 mVp-p in: 140 mVp-p out,
    /// i.e. 0.035 full scale of wet against the 0.025 full scale put in.
    constexpr double K_CHECK_POINT_FIVE_FULL_SCALE = 0.035;

    /// @brief Streams a stimulus through an already-prepared node, block by block.
    /// @param node The node under test.
    /// @param stimulus Input samples; its length must be a whole number of blocks.
    /// @return The node's output.
    std::vector<float> stream(SushiDSP::AudioNode& node, const std::vector<float>& stimulus)
    {
        std::vector<float> output(stimulus.size(), 0.0f);
        for (std::size_t offset = 0; offset + K_BLOCK <= stimulus.size(); offset += K_BLOCK)
        {
            const float* inputs[1] = {stimulus.data() + offset};
            float* outputs[1] = {output.data() + offset};
            SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK, 0.0};
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

    /// @brief Energy of the samples from `begin` to the end.
    double energy_from(const std::vector<float>& samples, std::size_t begin)
    {
        double energy = 0.0;
        for (std::size_t n = begin; n < samples.size(); ++n)
        {
            energy += static_cast<double>(samples[n]) * samples[n];
        }
        return energy;
    }

    /// @brief Sample index of the largest magnitude from `begin` to the end of the signal.
    std::size_t peak_index_from(const std::vector<float>& samples, std::size_t begin)
    {
        std::size_t found = begin;
        float largest = 0.0f;
        for (std::size_t n = begin; n < samples.size(); ++n)
        {
            if (std::abs(samples[n]) > largest)
            {
                largest = std::abs(samples[n]);
                found = n;
            }
        }
        return found;
    }

    /// @brief Largest magnitude of the difference of two signals from `begin` to the end of `a`.
    double peak_difference_from(const std::vector<float>& a, const std::vector<float>& b, std::size_t begin)
    {
        double peak = 0.0;
        for (std::size_t n = begin; n < a.size(); ++n)
        {
            peak = std::max(peak, std::abs(static_cast<double>(a[n] - b[n])));
        }
        return peak;
    }

    /// @brief A burst of 3 kHz followed by silence, the stimulus the decay tests use.
    /// @param length Total length in samples.
    /// @return The stimulus.
    std::vector<float> burst_stimulus(std::size_t length)
    {
        std::vector<float> input(length, 0.0f);
        for (std::size_t n = 0; n < 480; ++n) // 10 ms, well inside the 7 kHz band
        {
            input[n] = 0.6f * static_cast<float>(std::sin(2.0 * M_PI * 3000.0 *
                                                          static_cast<double>(n) / K_SAMPLE_RATE));
        }
        return input;
    }

    /// @brief A 10 ms burst of eight equal tones spanning 400 Hz to 8.1 kHz, then silence.
    ///
    /// Stationary and broadband, so a window on any repeat sees the same spectrum shaped by
    /// however many passes it has taken; a chirp would move under the window instead.
    /// @param length Total length in samples.
    /// @return The stimulus.
    std::vector<float> multitone_stimulus(std::size_t length)
    {
        const double tones[8] = {400.0, 900.0, 1600.0, 2500.0, 3600.0, 4900.0, 6400.0, 8100.0};
        std::vector<float> input(length, 0.0f);
        for (std::size_t n = 0; n < 480; ++n)
        {
            double sum = 0.0;
            for (const double frequency : tones)
            {
                sum += 0.15 *
                       std::sin(2.0 * M_PI * frequency * static_cast<double>(n) / K_SAMPLE_RATE);
            }
            input[n] = static_cast<float>(sum);
        }
        return input;
    }

    /// @brief Energy-weighted mean frequency of a window, over 250 Hz to 12 kHz.
    /// @param samples The signal.
    /// @param begin First sample of the window.
    /// @param length Window length, Hann-tapered before the transform.
    /// @return The centroid in Hz, or 0 for a silent window.
    double window_centroid(const std::vector<float>& samples, std::size_t begin, std::size_t length)
    {
        double total = 0.0;
        double weighted = 0.0;
        for (int bin = 1; bin <= 48; ++bin)
        {
            const double frequency = 250.0 * bin;
            double real = 0.0;
            double imaginary = 0.0;
            for (std::size_t n = 0; n < length && begin + n < samples.size(); ++n)
            {
                const double taper = 0.5 - 0.5 * std::cos(2.0 * M_PI * static_cast<double>(n) /
                                                          static_cast<double>(length - 1));
                const double value = static_cast<double>(samples[begin + n]) * taper;
                const double angle =
                    2.0 * M_PI * frequency * static_cast<double>(n) / K_SAMPLE_RATE;
                real += value * std::cos(angle);
                imaginary -= value * std::sin(angle);
            }
            const double power = real * real + imaginary * imaginary;
            total += power;
            weighted += frequency * power;
        }
        return total > 0.0 ? weighted / total : 0.0;
    }

    /// @brief The wet delay of a node whose TIME is 0, from the plan's N2 arithmetic.
    ///
    /// The ring at the fast clock, plus the two conversions at 34 zero crossings each over
    /// min(clock, host), plus the wet prime.
    /// @param ring_length The converter's ring length for the mode.
    /// @return Seconds.
    double wet_delay_at_fast_clock(double ring_length)
    {
        const double round_trip = 2.0 * K_RESAMPLER_HALF_WIDTH / std::min(Clock::K_FAST_RATE_HZ, K_SAMPLE_RATE);
        const double prime = static_cast<double>(SushiDSP::Dd3Delay::K_WET_PRIME_FRAMES) / K_SAMPLE_RATE;
        return ring_length / Clock::K_FAST_RATE_HZ + round_trip + prime;
    }

    /// @brief Passes when a wet delay equals the N2 arithmetic for a ring, to the float that
    /// holds log(fs): the ring term moves by u * ln(fs) of itself, and the sums add three double
    /// roundings.
    ::testing::AssertionResult is_wet_delay_of(double delay_seconds, double ring_length)
    {
        const double expected = wet_delay_at_fast_clock(ring_length);
        const double ring_term = ring_length / Clock::K_FAST_RATE_HZ;
        const double bound = ring_term * std::expm1(K_FLOAT_ROUNDOFF * std::log(Clock::K_FAST_RATE_HZ)) +
                             3.0 * TestSupport::unit_roundoff<double>() * expected;
        return TestSupport::is_within(TestSupport::measure_error(delay_seconds, expected, bound));
    }

    /// @brief A node prepared at TIME 0 in a mode, no feedback and full level.
    void set_up_fast_wet_node(SushiDSP::Dd3Delay& node, Mode mode)
    {
        node.set_mode(mode);
        node.set_time(0.0f);
        node.set_feedback(0.0f);
        node.set_level(1.0f);
    }

    /// @brief Renders an impulse through a node and returns the index of the echo's peak.
    std::size_t echo_peak(SushiDSP::Dd3Delay& node, double seconds)
    {
        std::vector<float> input(static_cast<std::size_t>(seconds * K_SAMPLE_RATE), 0.0f);
        input[0] = 1.0f;
        const auto output = SushiDSP::measure::render(node, input, K_SAMPLE_RATE, K_BLOCK);
        return peak_index_from(output, 8u);
    }

    /// @brief Names a mode and the ring length it holds.
    struct RingRow
    {
            const char* name;
            Mode mode;
            double ring_length;
    };

    const RingRow K_RING_ROWS[] = {{"S", Mode::Short, 1024.0}, {"L", Mode::Long, 16384.0}};

    /// @brief Fails for every sample of two signals that differs in a single bit.
    void expect_identical(const std::vector<float>& actual, const std::vector<float>& reference)
    {
        ASSERT_EQ(actual.size(), reference.size());
        for (std::size_t n = 0; n < actual.size(); ++n)
        {
            ASSERT_EQ(actual[n], reference[n]) << n;
        }
    }

    /// @brief Fails for every sample that is not a finite number.
    void expect_all_finite(const std::vector<float>& samples)
    {
        for (const float sample : samples)
        {
            ASSERT_TRUE(std::isfinite(sample));
        }
    }

    /// @brief Renders the glide of D.TIME from 0 to 1 over a sine at one block size.
    ///
    /// prepare() snaps the clock to D.TIME 0, so the set_time after it is what makes it glide.
    std::vector<float> glide_at_block_size(const std::vector<float>& input, std::uint32_t block)
    {
        SushiDSP::Dd3Delay node;
        node.set_mode(Mode::Middle);
        node.set_time(0.0f);
        node.set_feedback(0.6f);
        node.set_level(0.7f);
        node.prepare({K_SAMPLE_RATE, block, 1u});
        node.set_time(1.0f);
        std::vector<float> output(input.size(), 0.0f);
        for (std::size_t offset = 0; offset < input.size(); offset += block)
        {
            const std::uint32_t frames =
                static_cast<std::uint32_t>(std::min<std::size_t>(block, input.size() - offset));
            const float* inputs[1] = {input.data() + offset};
            float* outputs[1] = {output.data() + offset};
            SushiDSP::ProcessContext context{outputs, inputs, frames, 0.0};
            node.process(context);
        }
        return output;
    }

    /// @brief Streams a loud 300 Hz sine, so the node holds state worth resetting.
    void charge_with_a_loud_sine(SushiDSP::Dd3Delay& node)
    {
        node.prepare({K_SAMPLE_RATE, K_BLOCK, 1u});
        stream(node, SushiDSP::measure::sine_stimulus(300.0, K_SAMPLE_RATE, 4800u, 0.9f));
    }
} // namespace

TEST(Small_Dd3Delay, Dd3DelayAtZeroLevelIsAWire)
{
    // Ruling 9: the dry branch carries no shelf and no trim. See tests/nodes/dd3/test_dd3_delay/README.md.
    SushiDSP::Dd3Delay node;
    node.set_level(0.0f);
    node.set_feedback(0.0f);
    const auto input = SushiDSP::measure::sine_stimulus(440.0, K_SAMPLE_RATE, 4800u, 0.5f);
    expect_identical(SushiDSP::measure::render(node, input, K_SAMPLE_RATE, K_BLOCK), input);
}

TEST(Medium_Dd3Delay, Dd3DelayWetDelayMatchesThePlansArithmetic)
{
    // The two conversions cost 68 / min(fs, 48000), not 64. See tests/nodes/dd3/test_dd3_delay/README.md.
    for (const RingRow& row : K_RING_ROWS)
    {
        SushiDSP::Dd3Delay node;
        set_up_fast_wet_node(node, row.mode);
        node.prepare({K_SAMPLE_RATE, K_BLOCK, 1u});
        EXPECT_TRUE(is_wet_delay_of(node.wet_delay_seconds(), row.ring_length)) << row.name;
    }
}

TEST(Medium_Dd3Delay, Dd3DelayEchoLandsAtTheWetDelay)
{
    // 32 samples catch a wrong N, fs or round trip. See tests/nodes/dd3/test_dd3_delay/README.md.
    for (const RingRow& row : K_RING_ROWS)
    {
        SushiDSP::Dd3Delay node;
        set_up_fast_wet_node(node, row.mode);
        const double expected_samples = wet_delay_at_fast_clock(row.ring_length) * K_SAMPLE_RATE;
        EXPECT_NEAR(static_cast<double>(echo_peak(node, 0.5)), expected_samples, K_ECHO_PEAK_SLACK_SAMPLES)
            << row.name;
    }
}

TEST(Small_Dd3Delay, Dd3DelayFillsEveryBlockSizeAndStaysFinite)
{
    for (const std::uint32_t block : {1u, 7u, 8u, 9u, 64u, 255u, 256u})
    {
        SushiDSP::Dd3Delay node;
        node.set_level(0.7f);
        node.set_feedback(0.6f);
        node.set_time(0.4f);
        const auto input = SushiDSP::measure::sine_stimulus(220.0, K_SAMPLE_RATE, 1200u, 0.8f);
        const auto output = SushiDSP::measure::render(node, input, K_SAMPLE_RATE, block);
        ASSERT_EQ(output.size(), input.size()) << block;
        expect_all_finite(output);
    }
}

TEST(Medium_Dd3Delay, Dd3DelayGlidesIdenticallyAtEveryBlockSize)
{
    // Splitting the input must not move a sample while the clock glides. See tests/nodes/dd3/test_dd3_delay/README.md.
    const auto input = SushiDSP::measure::sine_stimulus(220.0, K_SAMPLE_RATE, 24000u, 0.8f);
    const auto reference = glide_at_block_size(input, K_BLOCK);
    for (const std::uint32_t block : {1u, 7u, 8u, 9u, 64u, 255u})
    {
        expect_identical(glide_at_block_size(input, block), reference);
    }
}

TEST(Small_Dd3Delay, Dd3DelayResetReturnsItToTheStateOfAFreshNode)
{
    SushiDSP::Dd3Delay used;
    used.set_level(1.0f);
    used.set_feedback(0.8f);
    charge_with_a_loud_sine(used);
    used.reset();

    SushiDSP::Dd3Delay fresh;
    fresh.set_level(1.0f);
    fresh.set_feedback(0.8f);
    fresh.prepare({K_SAMPLE_RATE, K_BLOCK, 1u});

    const std::vector<float> silence(4800u, 0.0f);
    expect_identical(stream(used, silence), stream(fresh, silence));
}

TEST(Small_Dd3Delay, Dd3DelayRepeatsDecayAndDarkenWithFeedback)
{
    // Each repeat loses level and treble together. See tests/nodes/dd3/test_dd3_delay/README.md.
    SushiDSP::Dd3Delay node;
    set_up_fast_wet_node(node, Mode::Short);
    node.set_feedback(0.7f);
    const auto input = multitone_stimulus(2600u);
    const auto output = SushiDSP::measure::render(node, input, K_SAMPLE_RATE, K_BLOCK);

    // Echoes land near 672, 1344 and 2016 samples (N2's 13.995 ms, three times).
    const double first = window_rms(output, 660u, 512u);
    const double third = window_rms(output, 2004u, 512u);
    EXPECT_GT(first, 0.0);
    EXPECT_LT(third, first);
    EXPECT_LT(window_centroid(output, 2004u, 512u),
              K_CENTROID_FALL_RATIO * window_centroid(output, 660u, 512u));
}

TEST(Medium_Dd3Delay, Dd3DelayTailDiesBelowTheTopOfTheKnob)
{
    // The tail sustains only at the top of the knob. See tests/nodes/dd3/test_dd3_delay/README.md.
    SushiDSP::Dd3Delay node;
    set_up_fast_wet_node(node, Mode::Short);
    node.set_feedback(0.7f);
    const auto output = SushiDSP::measure::render(
        node, burst_stimulus(static_cast<std::size_t>(2.0 * K_SAMPLE_RATE)), K_SAMPLE_RATE, K_BLOCK);
    EXPECT_LT(window_rms(output, 48000u, 48000u), K_DEAD_TAIL_RMS);
}

TEST(Medium_Dd3Delay, Dd3DelaySelfOscillatesAtTheTopOfTheKnob)
{
    SushiDSP::Dd3Delay node;
    set_up_fast_wet_node(node, Mode::Short);
    node.set_feedback(1.0f);
    const auto output = SushiDSP::measure::render(
        node, burst_stimulus(static_cast<std::size_t>(2.0 * K_SAMPLE_RATE)), K_SAMPLE_RATE, K_BLOCK);
    EXPECT_GT(window_rms(output, 48000u, 48000u), K_SUSTAINED_TAIL_RMS);
}

TEST(Medium_Dd3Delay, Dd3DelayHoldKeepsRepeatingWithNoInput)
{
    SushiDSP::Dd3Delay node;
    set_up_fast_wet_node(node, Mode::Long);
    node.prepare({K_SAMPLE_RATE, K_BLOCK, 1u});
    stream(node, SushiDSP::measure::sine_stimulus(500.0, K_SAMPLE_RATE, 12000u, 0.5f));

    node.set_mode(Mode::Hold); // Ruling 14: same N, so the loop survives
    const std::vector<float> silence(48000u, 0.0f);
    EXPECT_GT(energy_from(stream(node, silence), 24000u), K_HELD_ENERGY_FLOOR);
}

TEST(Medium_Dd3Delay, Dd3DelayHitsCheckPointFive)
{
    // 0.025 full scale in gives 0.035 full scale of wet out. See tests/nodes/dd3/test_dd3_delay/README.md.
    SushiDSP::Dd3Delay wet_node;
    SushiDSP::Dd3Delay dry_node;
    for (SushiDSP::Dd3Delay* node : {&wet_node, &dry_node})
    {
        node->set_mode(Mode::Middle);
        node->set_time(0.5f);
        node->set_feedback(0.0f);
    }
    wet_node.set_level(1.0f);
    dry_node.set_level(0.0f);
    const auto input = SushiDSP::measure::sine_stimulus(1000.0, K_SAMPLE_RATE, 96000u, 0.025f);
    const auto with_wet = SushiDSP::measure::render(wet_node, input, K_SAMPLE_RATE, K_BLOCK);
    const auto without = SushiDSP::measure::render(dry_node, input, K_SAMPLE_RATE, K_BLOCK);
    EXPECT_NEAR(peak_difference_from(with_wet, without, 72000u), K_CHECK_POINT_FIVE_FULL_SCALE,
                K_CHECK_POINT_FIVE_BAND * K_CHECK_POINT_FIVE_FULL_SCALE);
}

TEST(Small_Dd3Delay, Dd3DelayParametersRoundTripByName)
{
    SushiDSP::Dd3Delay node;
    node.set_parameter_for_control_thread("time", 0.8f);
    node.set_parameter_for_control_thread("feedback", 0.25f);
    node.set_parameter_for_control_thread("level", 0.6f);
    node.set_parameter_for_control_thread("mode", 3.0f);
    EXPECT_EQ(node.get_parameter("time"), 0.8f);
    EXPECT_EQ(node.get_parameter("feedback"), 0.25f);
    EXPECT_EQ(node.get_parameter("level"), 0.6f);
    EXPECT_EQ(node.get_parameter("mode"), 3.0f);
    EXPECT_EQ(node.get_parameter("nonsense"), 0.0f);
}

TEST(Small_Dd3Delay, Dd3DelayAppliesAParameterWrittenByIndex)
{
    SushiDSP::Dd3Delay node;
    node.set_parameter_for_audio_thread(static_cast<std::uint16_t>(0), 0.1f);
    EXPECT_EQ(node.get_parameter("time"), 0.1f);
}

TEST(Small_Dd3Delay, Dd3DelayIgnoresAParameterIndexOutOfRange)
{
    SushiDSP::Dd3Delay node;
    node.set_parameter_for_control_thread("time", 0.1f);
    node.set_parameter_for_audio_thread(static_cast<std::uint16_t>(99), 0.9f);
    EXPECT_EQ(node.get_parameter("time"), 0.1f);
}

TEST(Small_Dd3Delay, Dd3DelayDefersAModeChangeMadeByIndex)
{
    // Swapping N clears up to 16384 samples, so the index overload only records the request.
    SushiDSP::Dd3Delay node;
    node.set_mode(Mode::Long);
    node.set_time(0.0f);
    node.prepare({K_SAMPLE_RATE, K_BLOCK, 1u});
    const double before = node.wet_delay_seconds();
    EXPECT_TRUE(is_wet_delay_of(before, 16384.0));

    node.set_parameter_for_audio_thread(static_cast<std::uint16_t>(3), 0.0f); // Short, by index
    EXPECT_EQ(node.get_parameter("mode"), 0.0f);                              // the target reads back at once
    EXPECT_EQ(node.wet_delay_seconds(), before);                              // the ring has not moved yet

    const std::vector<float> silence(K_BLOCK, 0.0f);
    const float* inputs[1] = {silence.data()};
    std::vector<float> scratch(K_BLOCK, 0.0f);
    float* outputs[1] = {scratch.data()};
    SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK, 0.0};
    node.process(context);
    EXPECT_TRUE(is_wet_delay_of(node.wet_delay_seconds(), 1024.0)); // block start applied it
}

TEST(Small_Dd3Delay, Dd3DelayAppliesAModeChangeMadeByNameAtOnce)
{
    // The string overload is the control thread's, so it applies without waiting for a block.
    SushiDSP::Dd3Delay node;
    node.set_mode(Mode::Short);
    node.set_time(0.0f);
    node.prepare({K_SAMPLE_RATE, K_BLOCK, 1u});
    node.set_parameter_for_control_thread("mode", 2.0f);
    EXPECT_TRUE(is_wet_delay_of(node.wet_delay_seconds(), 16384.0));
}

TEST(Small_Dd3Delay, Dd3DelayControlThreadWritesOfOtherIdsLeaveTheRingWhereItIs)
{
    SushiDSP::Dd3Delay node;
    node.set_mode(Mode::Long);
    node.set_time(0.0f);
    node.prepare({K_SAMPLE_RATE, K_BLOCK, 1u});
    node.set_parameter_for_audio_thread(static_cast<std::uint16_t>(3), 0.0f); // Short, recorded only
    node.set_parameter_for_control_thread("time", 0.0f);
    EXPECT_TRUE(is_wet_delay_of(node.wet_delay_seconds(), 16384.0));
    node.set_parameter_for_control_thread("nonsense", 1.0f);
    EXPECT_TRUE(is_wet_delay_of(node.wet_delay_seconds(), 16384.0));
}
