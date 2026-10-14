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
 * @file test_acceptance.cpp
 * @brief Acceptance measurements for the Dd3Delay node.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <gtest/gtest.h>
#include <SushiDSP/foundation/regression_test/regression_test.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <SushiDSP/nodes/dd3/dd3_delay.hpp>
#include <SushiDSP/foundation/signal_measure/alias_floor.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Every case renders seconds, hence SUSHIDSP_REGRESSION_TEST. See tests/nodes/dd3/test_dd3_delay/README.md.

namespace
{
    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr std::uint32_t K_BLOCK = 256u;

    /// @brief The stimulus the DD-3's own service note measures at, in full-scale units.
    ///
    /// Page 2 of the notes takes its whole check-point ladder at 1 kHz and 100 mVp-p in, i.e.
    /// 0.05 V peak; against the core's 2 V peak full scale that is 0.025 of full scale (dossier
    /// docs/archive/research/2026-08-26-boss-dd3-design-dossier.md, check-point levels).
    constexpr double K_SERVICE_NOTE_STIMULUS = 0.025;

    /// @brief Half the resampler prototype's width, counted in zero crossings, so the two
    /// conversions cost twice this between them (Task 1.1 committed 34, not the plan's 32).
    ///
    /// The literal restates what Resampler::latency_seconds() documents rather than calling it,
    /// so that a prototype whose half width moves without this file moving fails here instead
    /// of agreeing with itself. Reading the number off the node would make the row an identity.
    constexpr double K_RESAMPLER_HALF_WIDTH = 34.0;

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

    constexpr double K_BATTERY_SWEEP_SECONDS = 2.0;
    constexpr double K_BATTERY_BURST_SECONDS = 0.5;
    constexpr double K_BATTERY_STEP_SECONDS = 0.75;
    constexpr double K_BATTERY_SILENCE_SECONDS = 0.75;
    constexpr double K_BATTERY_SECONDS = K_BATTERY_SWEEP_SECONDS + 3.0 * K_BATTERY_BURST_SECONDS +
                                         K_BATTERY_STEP_SECONDS + K_BATTERY_SILENCE_SECONDS;

    /// @brief The D6b stimulus battery, in full-scale sample units rather than volts.
    ///
    /// Copied in shape from tests/foundation/circuits/stages/test_triode_stage/test_dk_netlist.cpp's convergence case:
    /// a 2 s logarithmic 20 Hz to 20 kHz sweep at -12 dBFS, three 1 kHz bursts at 0.05, 0.5 and
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
} // namespace

SUSHIDSP_REGRESSION_TEST(Dd3Delay, Dd3AcceptanceClockCurveMatchesThePublishedRange)
{
    // Row 1 checks fs, which the service note publishes. See tests/nodes/dd3/test_dd3_delay/README.md.
    using Clock = SushiDSP::dd3::Dd3Clock;
    EXPECT_LE(std::abs(Clock::clock_for_time(0.0) - 82500.0) / 82500.0, 0.02);
    EXPECT_LE(std::abs(Clock::clock_for_time(1.0) - 20000.0) / 20000.0, 0.02);

    struct Mode
    {
            const char* name;
            SushiDSP::dd3::Dd3Mode mode;
            double ring_length;
    };
    const Mode modes[3] = {{"S", SushiDSP::dd3::Dd3Mode::Short, 1024.0},
                           {"M", SushiDSP::dd3::Dd3Mode::Middle, 4096.0},
                           {"L", SushiDSP::dd3::Dd3Mode::Long, 16384.0}};
    for (const Mode& item : modes)
    {
        for (const float time : {0.0f, 0.5f, 1.0f})
        {
            SushiDSP::Dd3Delay node;
            node.set_mode(item.mode);
            node.set_time(time);
            node.set_feedback(0.0f);
            node.set_level(1.0f);
            node.prepare({K_SAMPLE_RATE, K_BLOCK, 1u});

            const double clock_hz = Clock::clock_for_time(static_cast<double>(time));
            const double predicted =
                predicted_wet_seconds(item.ring_length, clock_hz, K_SAMPLE_RATE);
            // The only slack is the float that holds log(fs). See tests/nodes/dd3/test_dd3_delay/README.md.
            EXPECT_NEAR(node.wet_delay_seconds(), predicted, 1e-4 * predicted)
                << item.name << " " << time;
            std::printf("[ measure ] DD-3 mode %s, TIME %.2f: fs %.1f Hz, ring %.3f ms, "
                        "wet %.3f ms\n",
                        item.name, static_cast<double>(time), clock_hz,
                        1000.0 * item.ring_length / clock_hz, 1000.0 * node.wet_delay_seconds());
        }
    }
}

SUSHIDSP_REGRESSION_TEST(Dd3Delay, Dd3AcceptanceMeasuredEchoLandsWhereTheClockCurveSaysItWill)
{
    // An impulse's echo cannot agree with a wrong fs. See tests/nodes/dd3/test_dd3_delay/README.md.
    struct Mode
    {
            const char* name;
            SushiDSP::dd3::Dd3Mode mode;
            double ring_length;
    };
    const Mode modes[3] = {{"S", SushiDSP::dd3::Dd3Mode::Short, 1024.0},
                           {"M", SushiDSP::dd3::Dd3Mode::Middle, 4096.0},
                           {"L", SushiDSP::dd3::Dd3Mode::Long, 16384.0}};
    const double clock_hz = SushiDSP::dd3::Dd3Clock::clock_for_time(0.5);
    for (const Mode& item : modes)
    {
        SushiDSP::Dd3Delay node;
        node.set_mode(item.mode);
        node.set_time(0.5f);
        node.set_feedback(0.0f);
        node.set_level(1.0f);

        std::vector<float> input(static_cast<std::size_t>(0.6 * K_SAMPLE_RATE), 0.0f);
        input[0] = 1.0f;
        const auto output = SushiDSP::measure::render(node, input, K_SAMPLE_RATE, K_BLOCK);

        const double predicted = predicted_wet_seconds(item.ring_length, clock_hz, K_SAMPLE_RATE);
        const double predicted_samples = predicted * K_SAMPLE_RATE;
        // The search starts past the dry impulse; with F.BACK down there is only one echo.
        const double measured = static_cast<double>(peak_index(output, 8u, output.size()));
        std::printf("[ measure ] DD-3 mode %s echo at TIME 0.50: %.0f samples, predicted %.2f\n",
                    item.name, measured, predicted_samples);
        // The floor is absolute: 48 samples below 4800. See tests/nodes/dd3/test_dd3_delay/README.md.
        EXPECT_NEAR(measured, predicted_samples, std::max(48.0, 0.01 * predicted_samples))
            << item.name;
    }
}

SUSHIDSP_REGRESSION_TEST(Dd3Delay, Dd3AcceptancePitchSlursRatherThanJumping)
{
    // HOLD freezes the ring so only the clock moves the pitch. See tests/nodes/dd3/test_dd3_delay/README.md.
    using Converter = SushiDSP::dd3::Dd3Converter;
    // 397 cycles per ring pass join the loop without a step. See tests/nodes/dd3/test_dd3_delay/README.md.
    constexpr double K_RING_CYCLES = 397.0;
    constexpr double K_PROBE_HZ = K_RING_CYCLES * SushiDSP::dd3::Dd3Clock::K_FAST_RATE_HZ /
                                  static_cast<double>(Converter::K_LONG_LENGTH);
    constexpr double K_PROBE_AMPLITUDE = K_SERVICE_NOTE_STIMULUS;
    constexpr std::uint32_t K_SUB_BLOCK = SushiDSP::Dd3Delay::K_CLOCK_UPDATE_FRAMES;
    constexpr double K_FILL_SECONDS = 0.6; // over three passes of the 199 ms ring
    constexpr double K_SETTLE_SECONDS = 0.02;
    constexpr double K_SWEEP_SECONDS = 0.35; // 15474 clock ticks, one ring pass of sweep

    SushiDSP::Dd3Delay node;
    node.set_mode(SushiDSP::dd3::Dd3Mode::Long);
    node.set_time(0.0f);
    node.set_feedback(0.0f);
    node.set_level(1.0f);
    node.prepare({K_SAMPLE_RATE, K_SUB_BLOCK, 1u});

    const std::size_t fill_samples = static_cast<std::size_t>(K_FILL_SECONDS * K_SAMPLE_RATE);
    std::vector<float> fill(fill_samples, 0.0f);
    for (std::size_t n = 0; n < fill_samples; ++n)
    {
        fill[n] = static_cast<float>(
            K_PROBE_AMPLITUDE *
            std::sin(2.0 * M_PI * K_PROBE_HZ * static_cast<double>(n) / K_SAMPLE_RATE));
    }
    std::vector<float> filled(fill_samples, 0.0f);
    stream_range(node, fill, 0u, fill_samples, K_SUB_BLOCK, filled);

    // Long and Hold share N, so the switch keeps what the ring holds instead of clearing it.
    node.set_mode(SushiDSP::dd3::Dd3Mode::Hold);

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

    const std::vector<float> swept(held.begin() + static_cast<std::ptrdiff_t>(settle_samples),
                                   held.end());
    const std::vector<double> periods = upward_crossing_periods(swept);
    ASSERT_GT(periods.size(), 250u);

    double worst_step = 1.0;
    for (std::size_t i = 1; i < periods.size(); ++i)
    {
        worst_step = std::max(worst_step, periods[i] / periods[i - 1]);
        // One crossing sample is 4 %, so the bound is 0.90. See tests/nodes/dd3/test_dd3_delay/README.md.
        EXPECT_GE(periods[i], periods[i - 1] * 0.90) << i;
        // The glide's own largest step is 1.042, an ungliding clock's would be 4.125.
        EXPECT_LE(periods[i] / periods[i - 1], 1.20) << i;
    }
    std::printf("[ measure ] DD-3 pitch slur: period %.0f -> %.0f samples over %zu crossings, "
                "worst step %.4f (a jump would be 4.125)\n",
                periods.front(), periods.back(), periods.size(), worst_step);
    // The glide really did travel: the knob's own range is 4.125 to 1.
    EXPECT_GT(periods.back(), 3.5 * periods.front());
}

SUSHIDSP_REGRESSION_TEST(Dd3Delay, Dd3AcceptanceProducesNoNonFiniteSampleUnderTheStimulusBattery)
{
    // HOLD runs twice: from a cold ring and entered halfway. See tests/nodes/dd3/test_dd3_delay/README.md.
    struct Case
    {
            const char* name;
            SushiDSP::dd3::Dd3Mode initial;
            SushiDSP::dd3::Dd3Mode later;
    };
    const Case cases[5] = {
        {"S", SushiDSP::dd3::Dd3Mode::Short, SushiDSP::dd3::Dd3Mode::Short},
        {"M", SushiDSP::dd3::Dd3Mode::Middle, SushiDSP::dd3::Dd3Mode::Middle},
        {"L", SushiDSP::dd3::Dd3Mode::Long, SushiDSP::dd3::Dd3Mode::Long},
        {"H", SushiDSP::dd3::Dd3Mode::Hold, SushiDSP::dd3::Dd3Mode::Hold},
        {"L to H", SushiDSP::dd3::Dd3Mode::Long, SushiDSP::dd3::Dd3Mode::Hold},
    };
    const std::vector<float> stimulus = battery_stimulus(K_SAMPLE_RATE);
    const std::size_t switch_at = static_cast<std::size_t>(K_BATTERY_SWEEP_SECONDS * K_SAMPLE_RATE);

    double loudest = 0.0;
    for (const Case& item : cases)
    {
        for (const float feedback : {0.0f, 0.5f, 1.0f})
        {
            SushiDSP::Dd3Delay node;
            node.set_mode(item.initial);
            node.set_feedback(feedback);
            node.set_level(1.0f);
            node.set_time(0.35f);
            node.prepare({K_SAMPLE_RATE, K_BLOCK, 1u});

            std::vector<float> output(stimulus.size(), 0.0f);
            stream_range(node, stimulus, 0u, switch_at, K_BLOCK, output);
            node.set_mode(item.later);
            stream_range(node, stimulus, switch_at, stimulus.size(), K_BLOCK, output);

            for (std::size_t n = 0; n < output.size(); ++n)
            {
                ASSERT_TRUE(std::isfinite(output[n])) << item.name << " " << feedback << " " << n;
                // The bound is a tripwire for divergence. See tests/nodes/dd3/test_dd3_delay/README.md.
                ASSERT_LT(std::abs(output[n]), 32.0f) << item.name << " " << feedback << " " << n;
                loudest = std::max(loudest, static_cast<double>(std::abs(output[n])));
            }
        }
    }
    std::printf("[ measure ] DD-3 battery: %zu samples over 5 mode cases x 3 F.BACK settings, "
                "0 non-finite, loudest %.4f full scale\n",
                stimulus.size() * 15u, loudest);
}

SUSHIDSP_REGRESSION_TEST(Dd3Delay, Dd3AcceptanceReportsZeroLatencyAndDelaysOnlyItsWetPath)
{
    // The dry path is sample-aligned; only the wet one delays. See tests/nodes/dd3/test_dd3_delay/README.md.
    SushiDSP::Dd3Delay node;
    node.prepare({K_SAMPLE_RATE, K_BLOCK, 1u});
    EXPECT_EQ(node.latency_samples(), 0u);
    EXPECT_GT(node.wet_delay_seconds(), 0.0);
}

SUSHIDSP_REGRESSION_TEST(Dd3Delay, Dd3AcceptanceAliasFloorIsMeasuredNotAssumed)
{
    // The sweep runs at the service note's stimulus level. See tests/nodes/dd3/test_dd3_delay/README.md.
    SushiDSP::Dd3Delay node;
    node.set_mode(SushiDSP::dd3::Dd3Mode::Middle);
    node.set_time(0.5f);
    node.set_feedback(0.0f);
    node.set_level(1.0f);
    const double floor_db = SushiDSP::measure::alias_floor(
        node, K_SAMPLE_RATE, static_cast<float>(K_SERVICE_NOTE_STIMULUS));
    std::printf("[ measure ] DD-3 alias floor at %.3f full scale: %.2f dB "
                "(Ruling 7 revisit threshold -60 dB)\n",
                K_SERVICE_NOTE_STIMULUS, floor_db);
    EXPECT_LE(floor_db, -40.0);
}
