/**************************************************************************/
/* test_acceptance.cpp                                                    */
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
 * @file test_acceptance.cpp
 * @brief Acceptance measurements for the Dd3Delay node.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <SushiDSP/nodes/dd3/dd3_delay.hpp>
#include <SushiDSP/foundation/signal_measure/alias_floor.hpp>
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

    /// @brief The stimulus the DD-3's own service note measures at, in full-scale units.
    ///
    /// Page 2 of the notes takes its whole check-point ladder at 1 kHz and 100 mVp-p in, i.e.
    /// 0.05 V peak; against the core's 2 V peak full scale that is 0.025 of full scale (dossier
    /// the boss dd3 design dossier research note in the version history, check-point levels).
    constexpr double K_SERVICE_NOTE_STIMULUS = 0.025;

    /// @brief Half the resampler prototype's width, counted in zero crossings, so the two
    /// conversions cost twice this between them (Task 1.1 committed 34, not the plan's 32).
    ///
    /// The literal restates what Resampler::latency_seconds() documents rather than calling it,
    /// so that a prototype whose half width moves without this file moving fails here instead
    /// of agreeing with itself.
    constexpr double K_RESAMPLER_HALF_WIDTH = 34.0;

    /// @brief Unit roundoff of the float that carries the clock's control in the log domain.
    constexpr double K_FLOAT_ROUNDOFF = TestSupport::unit_roundoff<float>();

    /// @brief How far the argmax of an echo may sit from the centre of its prediction, in host
    /// samples. Characterized, not derived: the two 7 kHz cascades land the peak about 13 samples
    /// late, and the resampler kernels spread it over 34 / min(clock, host) seconds, 40 samples at
    /// TIME 0.5. Backlog entry to follow from the orchestrator.
    constexpr double K_ECHO_PEAK_SLACK_SAMPLES = 48.0;

    /// @brief The worst alias line the node may show at the service-note stimulus, in dB below the
    /// fundamental. Characterized: the loose bound the node was accepted at; the measured floor
    /// is -55.32 dB and the revisit threshold of Ruling 7 is -60 dB, which it misses.
    constexpr double K_ALIAS_FLOOR_BOUND_DB = -40.0;

    /// @brief The largest output magnitude the stimulus battery may reach, in full-scale units.
    /// Characterized: a tripwire for divergence. The converter's rails hold the loop near 1.15
    /// full scale, so the bound is far above anything the model approaches.
    constexpr float K_DIVERGENCE_TRIPWIRE = 32.0f;

    /// @brief Render length that holds the short ring and the start of its echo at TIME 0:
    /// the echo is predicted at 672 samples.
    constexpr double K_WET_WINDOW_SECONDS = 760.0 / K_SAMPLE_RATE;

    /// @brief Names a delay mode and the ring length the published times assume of it.
    struct ModeRow
    {
            const char* name;
            Mode mode;
            double ring_length;
    };

    const ModeRow K_RING_MODES[] = {{"S", Mode::Short, 1024.0},
                                    {"M", Mode::Middle, 4096.0},
                                    {"L", Mode::Long, 16384.0}};

    /// @brief The wet path's delay from the plan's N2 arithmetic, recomputed here rather than
    /// read off the node, so a wrong N or a wrong clock has nothing to hide behind.
    /// @param ring_length The converter's buffer length for this mode.
    /// @param clock_hz The sample clock the TIME setting asks for.
    /// @param sample_rate The host rate.
    /// @return Ring time, plus both conversions' availability latency, plus the wet prime.
    double predicted_wet_seconds(double ring_length, double clock_hz, double sample_rate)
    {
        const double round_trip = 2.0 * K_RESAMPLER_HALF_WIDTH / std::min(clock_hz, sample_rate);
        const double prime =
            static_cast<double>(SushiDSP::Dd3Delay::K_WET_PRIME_FRAMES) / sample_rate;
        return ring_length / clock_hz + round_trip + prime;
    }

    /// @brief Streams a half-open range of a stimulus through an already-prepared node.
    /// @param node The node under test; prepare() must already have run at this block size.
    /// @param stimulus The whole stimulus.
    /// @param begin First sample of the range.
    /// @param end One past the last sample of the range.
    /// @param block Frames per process() call; the final call carries the remainder.
    /// @param output Destination, at least end samples long.
    void stream_range(SushiDSP::AudioNode& node, const std::vector<float>& stimulus,
                      std::size_t begin, std::size_t end, std::uint32_t block,
                      std::vector<float>& output)
    {
        for (std::size_t offset = begin; offset < end; offset += block)
        {
            const std::uint32_t frames =
                static_cast<std::uint32_t>(std::min<std::size_t>(block, end - offset));
            const float* inputs[1] = {stimulus.data() + offset};
            float* outputs[1] = {output.data() + offset};
            SushiDSP::ProcessContext context{outputs, inputs, frames,
                                             static_cast<double>(offset) / K_SAMPLE_RATE};
            node.process(context);
        }
    }

    /// @brief Sample index of the largest magnitude in a half-open window.
    /// @param samples The signal.
    /// @param begin First sample searched.
    /// @param end One past the last sample searched.
    /// @return The index of the peak, or begin for an all-zero window.
    std::size_t peak_index(const std::vector<float>& samples, std::size_t begin, std::size_t end)
    {
        std::size_t found = begin;
        float largest = 0.0f;
        for (std::size_t n = begin; n < end && n < samples.size(); ++n)
        {
            if (std::abs(samples[n]) > largest)
            {
                largest = std::abs(samples[n]);
                found = n;
            }
        }
        return found;
    }

    /// @brief Sample counts between successive upward zero crossings.
    /// @param samples The signal.
    /// @return One entry per crossing pair; the instantaneous period, quantised to whole samples.
    std::vector<double> upward_crossing_periods(const std::vector<float>& samples)
    {
        std::vector<double> periods;
        std::size_t previous = 0;
        for (std::size_t n = 1; n < samples.size(); ++n)
        {
            if (samples[n - 1] <= 0.0f && samples[n] > 0.0f)
            {
                if (previous != 0)
                {
                    periods.push_back(static_cast<double>(n - previous));
                }
                previous = n;
            }
        }
        return periods;
    }

    /// @brief Renders a unit impulse through a mono node with no feedback and full level.
    /// @param node The node; its latency is read by the caller afterwards.
    /// @param mode The delay range.
    /// @param time D.TIME in [0, 1].
    /// @param seconds Length of the render.
    /// @return The output, seconds long.
    std::vector<float> impulse_response(SushiDSP::Dd3Delay& node, Mode mode, float time, double seconds)
    {
        node.set_mode(mode);
        node.set_time(time);
        node.set_feedback(0.0f);
        node.set_level(1.0f);
        std::vector<float> input(static_cast<std::size_t>(seconds * K_SAMPLE_RATE), 0.0f);
        input[0] = 1.0f;
        return SushiDSP::measure::render(node, input, K_SAMPLE_RATE, K_BLOCK);
    }

    /// @brief Returns whether every sample of a half-open range is exactly zero.
    bool is_silent_between(const std::vector<float>& samples, std::size_t begin, std::size_t end)
    {
        return std::all_of(samples.begin() + static_cast<std::ptrdiff_t>(begin),
                           samples.begin() + static_cast<std::ptrdiff_t>(end),
                           [](float sample) { return sample == 0.0f; });
    }

    /// @brief The largest magnitude in a half-open range.
    float peak_magnitude(const std::vector<float>& samples, std::size_t begin, std::size_t end)
    {
        float largest = 0.0f;
        for (std::size_t n = begin; n < end; ++n)
        {
            largest = std::max(largest, std::abs(samples[n]));
        }
        return largest;
    }

    /// @brief The frequency of the HOLD probe tone: 397 cycles per pass of the long ring at the
    /// fast clock, so the frozen loop joins its own tail without a phase step.
    constexpr double K_RING_CYCLES = 397.0;
    constexpr double K_PROBE_HZ = K_RING_CYCLES * Clock::K_FAST_RATE_HZ /
                                  static_cast<double>(SushiDSP::dd3::Dd3Converter::K_LONG_LENGTH);
    constexpr std::uint32_t K_SUB_BLOCK = SushiDSP::Dd3Delay::K_CLOCK_UPDATE_FRAMES;
    constexpr double K_FILL_SECONDS = 0.6;
    constexpr double K_SETTLE_SECONDS = 0.02;
    constexpr double K_SWEEP_SECONDS = 0.35;

    /// @brief The probe tone at the service-note level.
    /// @param samples Length of the tone.
    /// @return The tone at K_PROBE_HZ.
    std::vector<float> probe_tone(std::size_t samples)
    {
        std::vector<float> tone(samples, 0.0f);
        for (std::size_t n = 0; n < samples; ++n)
        {
            tone[n] = static_cast<float>(
                K_SERVICE_NOTE_STIMULUS *
                std::sin(2.0 * M_PI * K_PROBE_HZ * static_cast<double>(n) / K_SAMPLE_RATE));
        }
        return tone;
    }

    /// @brief Fills the long ring with the probe tone, freezes it with HOLD, then turns D.TIME
    /// from 0 to 1 across the sweep and returns the wet signal of the sweep alone.
    std::vector<float> held_trace_under_a_time_sweep()
    {
        SushiDSP::Dd3Delay node;
        node.set_mode(Mode::Long);
        node.set_time(0.0f);
        node.set_feedback(0.0f);
        node.set_level(1.0f);
        node.prepare({K_SAMPLE_RATE, K_SUB_BLOCK, 1u});

        const std::size_t fill_samples = static_cast<std::size_t>(K_FILL_SECONDS * K_SAMPLE_RATE);
        std::vector<float> filled(fill_samples, 0.0f);
        stream_range(node, probe_tone(fill_samples), 0u, fill_samples, K_SUB_BLOCK, filled);

        // Long and Hold share N, so the switch keeps what the ring holds instead of clearing it.
        node.set_mode(Mode::Hold);

        const std::size_t settle_samples = static_cast<std::size_t>(K_SETTLE_SECONDS * K_SAMPLE_RATE);
        const std::size_t sweep_samples = static_cast<std::size_t>(K_SWEEP_SECONDS * K_SAMPLE_RATE);
        const std::vector<float> silence(settle_samples + sweep_samples, 0.0f);
        std::vector<float> held(silence.size(), 0.0f);
        stream_range(node, silence, 0u, settle_samples, K_SUB_BLOCK, held);
        for (std::size_t offset = settle_samples; offset < held.size(); offset += K_SUB_BLOCK)
        {
            node.set_time(static_cast<float>(static_cast<double>(offset - settle_samples) /
                                             static_cast<double>(sweep_samples)));
            stream_range(node, silence, offset, std::min(offset + K_SUB_BLOCK, held.size()),
                         K_SUB_BLOCK, held);
        }
        return std::vector<float>(held.begin() + static_cast<std::ptrdiff_t>(settle_samples), held.end());
    }

    /// @brief Smallest period of the trace, in whole samples: the probe at the fast clock.
    const double K_FIRST_PERIOD = std::floor(K_SAMPLE_RATE / K_PROBE_HZ);

    /// @brief The knob's range of clock, 82.5 kHz to 20 kHz, as the ratio of the periods it makes.
    constexpr double K_PERIOD_RANGE = Clock::K_FAST_RATE_HZ / Clock::K_SLOW_RATE_HZ;

    /// @brief The lowest ratio of one measured period to the one before it.
    ///
    /// The true period never shrinks while the clock only falls. A reading is a difference of two
    /// crossing indices, each at most one sample past the true crossing, so it lies within one sample
    /// of the true period. The worst ratio of two readings of equal true periods is
    /// (P - 1) / (P + 1), and it rises with P, so the first period gives the floor.
    double lowest_period_ratio()
    {
        return (K_FIRST_PERIOD - 1.0) / (K_FIRST_PERIOD + 1.0);
    }

    /// @brief The true growth of the period over one longest period of the sweep: the log of the
    /// clock is linear in the knob, so the log of the period grows by ln(range) per sweep.
    double true_period_growth_per_period()
    {
        const double longest = std::ceil(K_FIRST_PERIOD * K_PERIOD_RANGE) + 1.0;
        const double sweep_samples = K_SWEEP_SECONDS * K_SAMPLE_RATE;
        return std::exp(std::log(K_PERIOD_RANGE) * longest / sweep_samples);
    }

    /// @brief The highest ratio of one measured period to the one before it: the true growth with
    /// one sample of quantisation either way, worst at the shortest period.
    double highest_period_ratio()
    {
        return (K_FIRST_PERIOD * true_period_growth_per_period() + 1.0) / (K_FIRST_PERIOD - 1.0);
    }

    /// @brief The least ratio of the last period to the first that proves the glide travelled.
    ///
    /// The clock follows the knob through the R56/C52 pole, so it trails the knob by tau, and the
    /// last reading is centred about one and a half periods before the end of the trace. Both
    /// shorten the distance travelled in the log domain; the readings are within one sample.
    double least_travelled_ratio()
    {
        const double last_period = std::ceil(K_FIRST_PERIOD * K_PERIOD_RANGE);
        const double lag = Clock::K_SLEW_TIME_CONSTANT_SECONDS * K_SAMPLE_RATE + 1.5 * last_period;
        const double fraction = 1.0 - lag / (K_SWEEP_SECONDS * K_SAMPLE_RATE);
        const double reached = std::pow(K_PERIOD_RANGE, fraction);
        return (K_FIRST_PERIOD * reached - 1.0) / (K_FIRST_PERIOD + 1.0);
    }

    /// @brief Fails for every period that shrinks or grows by more than the bounds allow.
    void expect_glide_without_a_step(const std::vector<double>& periods)
    {
        for (std::size_t i = 1; i < periods.size(); ++i)
        {
            EXPECT_GE(periods[i], periods[i - 1] * lowest_period_ratio()) << i;
            EXPECT_LE(periods[i], periods[i - 1] * highest_period_ratio()) << i;
        }
    }

    constexpr double K_BATTERY_SWEEP_SECONDS = 2.0;
    constexpr double K_BATTERY_BURST_SECONDS = 0.5;
    constexpr double K_BATTERY_STEP_SECONDS = 0.75;
    constexpr double K_BATTERY_SILENCE_SECONDS = 0.75;
    constexpr double K_BATTERY_SECONDS = K_BATTERY_SWEEP_SECONDS + 3.0 * K_BATTERY_BURST_SECONDS +
                                         K_BATTERY_STEP_SECONDS + K_BATTERY_SILENCE_SECONDS;

    /// @brief The D6b stimulus battery, in full-scale sample units rather than volts.
    ///
    /// A 2 s logarithmic 20 Hz to 20 kHz sweep at -12 dBFS, three 1 kHz bursts at 0.05, 0.5 and
    /// 1.0 full scale, a DC step, and digital silence.
    /// @param sample_rate Rate the battery is generated at.
    /// @return K_BATTERY_SECONDS seconds of stimulus.
    std::vector<float> battery_stimulus(double sample_rate)
    {
        const std::size_t sweep_samples =
            static_cast<std::size_t>(K_BATTERY_SWEEP_SECONDS * sample_rate);
        const std::size_t burst_samples =
            static_cast<std::size_t>(K_BATTERY_BURST_SECONDS * sample_rate);
        const std::size_t step_samples =
            static_cast<std::size_t>(K_BATTERY_STEP_SECONDS * sample_rate);
        const std::size_t total = static_cast<std::size_t>(K_BATTERY_SECONDS * sample_rate);

        std::vector<float> stimulus(total, 0.0f);
        const double low = 20.0;
        const double high = 20000.0;
        const double rate = std::log(high / low);
        for (std::size_t n = 0; n < sweep_samples; ++n)
        {
            const double seconds = static_cast<double>(n) / sample_rate;
            const double phase = 2.0 * M_PI * low * K_BATTERY_SWEEP_SECONDS / rate *
                                 (std::exp(seconds * rate / K_BATTERY_SWEEP_SECONDS) - 1.0);
            stimulus[n] = 0.251189f * static_cast<float>(std::sin(phase));
        }

        std::size_t position = sweep_samples;
        for (const float amplitude : {0.05f, 0.5f, 1.0f})
        {
            for (std::size_t n = 0; n < burst_samples && position < total; ++n, ++position)
            {
                stimulus[position] =
                    amplitude * static_cast<float>(std::sin(2.0 * M_PI * 1000.0 *
                                                            static_cast<double>(n) / sample_rate));
            }
        }
        for (std::size_t n = 0; n < step_samples && position < total; ++n, ++position)
        {
            stimulus[position] = 1.0f;
        }
        // The tail is left at zero: it is both the step's falling edge and the silence case.
        return stimulus;
    }

    /// @brief Names one run of the battery: the mode it starts in, the mode it switches to after
    /// the sweep, and the F.BACK setting.
    struct BatteryRow
    {
            std::string name;
            Mode initial;
            Mode later;
            float feedback;
    };

    /// @brief HOLD runs twice: from a cold ring and entered halfway. Every mode meets the three
    /// F.BACK settings 0, 0.5 and 1, the last of which is past self-oscillation.
    std::vector<BatteryRow> battery_rows()
    {
        struct ModePair
        {
                const char* name;
                Mode initial;
                Mode later;
        };
        const ModePair pairs[] = {{"Short", Mode::Short, Mode::Short},
                                  {"Middle", Mode::Middle, Mode::Middle},
                                  {"Long", Mode::Long, Mode::Long},
                                  {"Hold", Mode::Hold, Mode::Hold},
                                  {"LongToHold", Mode::Long, Mode::Hold}};
        struct Feedback
        {
                const char* name;
                float value;
        };
        const Feedback settings[] = {{"Zero", 0.0f}, {"Half", 0.5f}, {"Full", 1.0f}};
        std::vector<BatteryRow> rows;
        for (const ModePair& pair : pairs)
        {
            for (const Feedback& setting : settings)
            {
                rows.push_back({std::string(pair.name) + "Feedback" + setting.name, pair.initial,
                                pair.later, setting.value});
            }
        }
        return rows;
    }

    /// @brief Fails for every sample that is not finite or reaches the divergence tripwire.
    void expect_finite_and_bounded(const std::vector<float>& output)
    {
        for (std::size_t n = 0; n < output.size(); ++n)
        {
            ASSERT_TRUE(std::isfinite(output[n])) << n;
            ASSERT_LT(std::abs(output[n]), K_DIVERGENCE_TRIPWIRE) << n;
        }
    }
} // namespace

TEST(Medium_Dd3Delay, Dd3AcceptanceMeasuredEchoLandsWhereTheClockCurveSaysItWill)
{
    // An impulse's echo cannot agree with a wrong fs. See tests/nodes/dd3/test_dd3_delay/README.md.
    const double clock_hz = Clock::clock_for_time(0.5);
    // The clock keeps log(fs) in a float, which moves the delay by u * ln(fs) of itself.
    const double clock_error = std::expm1(K_FLOAT_ROUNDOFF * std::log(clock_hz));
    for (const ModeRow& row : K_RING_MODES)
    {
        SushiDSP::Dd3Delay node;
        const std::vector<float> output = impulse_response(node, row.mode, 0.5f, 0.6);
        const double predicted_samples =
            predicted_wet_seconds(row.ring_length, clock_hz, K_SAMPLE_RATE) * K_SAMPLE_RATE;
        // The search starts past the dry impulse; with F.BACK down there is only one echo.
        const double measured = static_cast<double>(peak_index(output, 8u, output.size()));
        EXPECT_NEAR(measured, predicted_samples,
                    K_ECHO_PEAK_SLACK_SAMPLES + predicted_samples * clock_error)
            << row.name;
    }
}

TEST(Medium_Dd3Delay, Dd3AcceptancePitchSlursRatherThanJumping)
{
    // HOLD freezes the ring so only the clock moves the pitch. See tests/nodes/dd3/test_dd3_delay/README.md.
    const std::vector<double> periods = upward_crossing_periods(held_trace_under_a_time_sweep());
    ASSERT_GT(periods.size(), 250u);
    expect_glide_without_a_step(periods);
    // The glide really did travel: the knob's own range is 4.125 to 1.
    EXPECT_GT(periods.back(), least_travelled_ratio() * periods.front());
}

class Medium_Dd3DelayBattery : public ::testing::TestWithParam<BatteryRow>
{
};

TEST_P(Medium_Dd3DelayBattery, ProducesNoNonFiniteSampleUnderTheStimulusBattery)
{
    const BatteryRow& row = GetParam();
    const std::vector<float> stimulus = battery_stimulus(K_SAMPLE_RATE);
    const std::size_t switch_at = static_cast<std::size_t>(K_BATTERY_SWEEP_SECONDS * K_SAMPLE_RATE);

    SushiDSP::Dd3Delay node;
    node.set_mode(row.initial);
    node.set_feedback(row.feedback);
    node.set_level(1.0f);
    node.set_time(0.35f);
    node.prepare({K_SAMPLE_RATE, K_BLOCK, 1u});

    std::vector<float> output(stimulus.size(), 0.0f);
    stream_range(node, stimulus, 0u, switch_at, K_BLOCK, output);
    node.set_mode(row.later);
    stream_range(node, stimulus, switch_at, stimulus.size(), K_BLOCK, output);

    expect_finite_and_bounded(output);
}

INSTANTIATE_TEST_SUITE_P(Rows, Medium_Dd3DelayBattery, ::testing::ValuesIn(battery_rows()),
                         [](const ::testing::TestParamInfo<BatteryRow>& info)
                         { return info.param.name; });

TEST(Small_Dd3Delay, Dd3AcceptanceKeepsTheDryPathAtFrameZeroAndDelaysOnlyTheWetPath)
{
    // The dry path is sample-aligned; only the wet one delays. See tests/nodes/dd3/test_dd3_delay/README.md.
    SushiDSP::Dd3Delay node;
    const std::vector<float> output = impulse_response(node, Mode::Short, 0.0f, K_WET_WINDOW_SECONDS);
    // Every stage after the ring is causal. See tests/nodes/dd3/test_dd3_delay/README.md.
    const std::size_t silent_until = static_cast<std::size_t>(
        std::floor(1024.0 / Clock::K_FAST_RATE_HZ * K_SAMPLE_RATE)) - 1u;

    EXPECT_EQ(node.latency_samples(), 0u);
    EXPECT_EQ(output[0], 1.0f);
    EXPECT_TRUE(is_silent_between(output, 1u, silent_until));
    EXPECT_GT(peak_magnitude(output, silent_until, output.size()), 0.0f);
}

TEST(Medium_Dd3Delay, Dd3AcceptanceAliasFloorStaysUnderItsBoundAtTheServiceNoteStimulus)
{
    // The sweep runs at the service note's stimulus level. See tests/nodes/dd3/test_dd3_delay/README.md.
    SushiDSP::Dd3Delay node;
    node.set_mode(Mode::Middle);
    node.set_time(0.5f);
    node.set_feedback(0.0f);
    node.set_level(1.0f);
    const double floor_db = SushiDSP::measure::alias_floor(
        node, K_SAMPLE_RATE, static_cast<float>(K_SERVICE_NOTE_STIMULUS));
    EXPECT_LE(floor_db, K_ALIAS_FLOOR_BOUND_DB);
}
