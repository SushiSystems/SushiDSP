/**************************************************************************/
/* test_dd3_clock.cpp                                                     */
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
 * @file test_dd3_clock.cpp
 * @brief Tests for the DD-3 variable-rate clock and its slew.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/dd3/dd3_clock.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "support/tolerance.hpp"

namespace
{
    using Clock = SushiDSP::dd3::Dd3Clock;

    /// @brief The published clock at TIME 0, in Hz (N5).
    constexpr double K_FAST_RATE_HZ = 82500.0;

    /// @brief The published clock at TIME 1, in Hz (N5).
    constexpr double K_SLOW_RATE_HZ = 20000.0;

    /// @brief R56 times C52 in seconds, the pole that slews the control voltage.
    constexpr double K_SLEW_TAU_SECONDS = 22000.0 * 2.7e-8;

    /// @brief Sample rate the slew tests run at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Unit roundoff of the float that carries the control in the log domain.
    constexpr double K_FLOAT_ROUNDOFF = TestSupport::unit_roundoff<float>();

    /// @brief Unit roundoff of double.
    constexpr double K_DOUBLE_ROUNDOFF = TestSupport::unit_roundoff<double>();

    /// @brief Steps the monotone sweep takes across TIME 0 to 1.
    constexpr int K_SWEEP_STEPS = 1000;

    /// @brief Names a time and the clock the geometric law gives there.
    struct TimeRow
    {
        const char* name;
        double time;
        double expected_hz;
    };

    /// @brief The clock is fast * (slow/fast)^t, so t = 0.5 is the geometric mean of the ends and
    /// t = 0.25 the geometric mean of the fast end and that mean.
    const TimeRow K_DERIVED_TIMES[] = {
        {"Half", 0.5, std::sqrt(K_FAST_RATE_HZ * K_SLOW_RATE_HZ)},
        {"Quarter", 0.25, std::sqrt(K_FAST_RATE_HZ * std::sqrt(K_FAST_RATE_HZ * K_SLOW_RATE_HZ))},
    };

    /// @brief Names a time-knob end of the published curve and its rate.
    struct EndpointRow
    {
        const char* name;
        double time;
        double rate_hz;
    };

    const EndpointRow K_ENDPOINTS[] = {{"Fast", 0.0, K_FAST_RATE_HZ}, {"Slow", 1.0, K_SLOW_RATE_HZ}};

    /// @brief Names a delay mode: its ring length, the length the published times assume and the
    /// published times at the fast and slow ends.
    struct ModeRow
    {
        const char* name;
        double n;
        double published_n;
        double published_fast_ms;
        double published_slow_ms;
    };

    const ModeRow K_MODES[] = {{"S", 1024.0, 1000.0, 12.5, 50.0},
                               {"M", 4096.0, 4000.0, 50.0, 200.0},
                               {"L", 16384.0, 16000.0, 200.0, 800.0}};

    /// @brief What a sweep of the time knob found.
    struct SweepAudit
    {
        bool strictly_decreasing;
        double lowest_hz;
        double highest_hz;
    };

    /// @brief Reads the clock at K_SWEEP_STEPS + 1 evenly spaced times from 0 to 1.
    SweepAudit sweep_time_knob()
    {
        SweepAudit audit{true, 1e300, 0.0};
        double previous = 1e300;
        for (int step = 0; step <= K_SWEEP_STEPS; ++step)
        {
            const double rate = Clock::clock_for_time(static_cast<double>(step) / K_SWEEP_STEPS);
            audit.strictly_decreasing = audit.strictly_decreasing && rate < previous;
            audit.lowest_hz = std::fmin(audit.lowest_hz, rate);
            audit.highest_hz = std::fmax(audit.highest_hz, rate);
            previous = rate;
        }
        return audit;
    }

    /// @brief Passes true when `computed` is within `operations` double roundings of `reference`.
    ::testing::AssertionResult within_double_roundings(double computed, double reference,
                                                       std::size_t operations)
    {
        return TestSupport::is_within(
            TestSupport::measure_relative_error<double>(computed, reference, operations));
    }

    /// @brief A clock prepared at K_SAMPLE_RATE and snapped to the fast end.
    Clock fast_clock()
    {
        Clock clock;
        clock.prepare(K_SAMPLE_RATE);
        clock.set_time(0.0f);
        clock.reset();
        return clock;
    }

    /// @brief The relative error of a rate read back from a float log: the log is rounded to float
    /// once, which moves the log by at most u times its size, and the exponential turns that into
    /// a relative error of the same size.
    double log_rounding_relative_error(double rate_hz)
    {
        return std::expm1(K_FLOAT_ROUNDOFF * std::log(rate_hz));
    }

    /// @brief The relative error, in the rate, of a control that decayed `frames` samples of the
    /// one-pole slew from `from_hz` towards `to_hz`.
    ///
    /// The control holds float logs. Three roundings move the logs of the start, the target and the
    /// result by at most u times their size; the decay is the coefficient to the power `frames`, so
    /// the float coefficient's rounding is multiplied by `frames` and the product and two sums add
    /// three more. Each acts on the distance travelled, scaled by the decay.
    double decay_relative_error(double from_hz, double to_hz, double result_hz, std::uint32_t frames,
                                double decay)
    {
        const double distance = std::fabs(std::log(from_hz) - std::log(to_hz));
        const double log_error = K_FLOAT_ROUNDOFF * (std::log(from_hz) + std::log(to_hz) + std::log(result_hz) +
                                                     static_cast<double>(frames + 3u) * distance * decay);
        return std::expm1(log_error);
    }
} // namespace

TEST(Small_Dd3Clock, Dd3ClockHitsThePublishedEndpoints)
{
    // N5: fs(TIME) = 82500 * (20000/82500)^TIME. See tests/nodes/dd3/README.md.
    for (const EndpointRow& row : K_ENDPOINTS)
    {
        EXPECT_TRUE(within_double_roundings(Clock::clock_for_time(row.time), row.rate_hz, 4)) << row.name;
    }
}

TEST(Small_Dd3Clock, Dd3ClockHitsTheGeometricMeansBetweenTheEndpoints)
{
    // Half way along the knob the clock is the geometric mean of the ends. See tests/nodes/dd3/README.md.
    for (const TimeRow& row : K_DERIVED_TIMES)
    {
        EXPECT_TRUE(within_double_roundings(Clock::clock_for_time(row.time), row.expected_hz, 10))
            << row.name;
    }
}

TEST(Small_Dd3Clock, Dd3ClockIsMonotoneAndStaysInsideItsRange)
{
    // The sweep ends on the published rates, up to the four roundings of the endpoint test.
    const SweepAudit audit = sweep_time_knob();

    EXPECT_TRUE(audit.strictly_decreasing);
    EXPECT_GE(audit.lowest_hz, K_SLOW_RATE_HZ * (1.0 - 4.0 * K_DOUBLE_ROUNDOFF));
    EXPECT_LE(audit.highest_hz, K_FAST_RATE_HZ * (1.0 + 4.0 * K_DOUBLE_ROUNDOFF));
}

TEST(Small_Dd3Clock, Dd3ClockDelayTimesLandOnThePublishedRanges)
{
    // The delay times follow N over the clock. See tests/nodes/dd3/README.md.
    for (const ModeRow& mode : K_MODES)
    {
        const double fast = 1000.0 * mode.n / Clock::clock_for_time(0.0);
        const double slow = 1000.0 * mode.n / Clock::clock_for_time(1.0);
        const double published_fast_hz = 1000.0 * mode.published_n / mode.published_fast_ms;
        const double published_slow_hz = 1000.0 * mode.published_n / mode.published_slow_ms;

        EXPECT_TRUE(within_double_roundings(
            fast, mode.published_fast_ms * (mode.n / mode.published_n) * (published_fast_hz / K_FAST_RATE_HZ), 12))
            << mode.name;
        EXPECT_TRUE(within_double_roundings(
            slow, mode.published_slow_ms * (mode.n / mode.published_n) * (published_slow_hz / K_SLOW_RATE_HZ), 12))
            << mode.name;
    }
}

TEST(Small_Dd3Clock, Dd3ClockSlewConstantsFollowFromR56AndC52)
{
    // tau = 22000 * 2.7e-8 = 5.94e-4 s, held as seconds and as a float of milliseconds.
    EXPECT_TRUE(within_double_roundings(Clock::K_SLEW_TIME_CONSTANT_SECONDS, K_SLEW_TAU_SECONDS, 4));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(
        static_cast<double>(Clock::K_SLEW_TIME_CONSTANT_MS), 1000.0 * K_SLEW_TAU_SECONDS, 4)));
}

TEST(Small_Dd3Clock, Dd3ClockSlewMatchesTheR56C52Pole)
{
    // After one tau the clock reads 33681.4 Hz. See tests/nodes/dd3/README.md.
    const std::uint32_t frames = 28u; // tau is 28.512 samples; the clock advances whole samples
    const double decay = std::exp(-static_cast<double>(frames) / (K_SAMPLE_RATE * K_SLEW_TAU_SECONDS));
    const double expected = std::exp(std::log(K_SLOW_RATE_HZ) +
                                     (std::log(K_FAST_RATE_HZ) - std::log(K_SLOW_RATE_HZ)) * decay);
    Clock clock = fast_clock();
    ASSERT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        clock.sample_rate_hz(), K_FAST_RATE_HZ, K_FAST_RATE_HZ * log_rounding_relative_error(K_FAST_RATE_HZ), 1.0)));
    clock.set_time(1.0f);

    const double after = clock.advance(frames);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        after, expected, expected * decay_relative_error(K_FAST_RATE_HZ, K_SLOW_RATE_HZ, expected, frames, decay))));
}

TEST(Small_Dd3Clock, Dd3ClockClosesTheDerivedFractionOfTheRemainingDistancePerSubBlock)
{
    // The pole closes 0.2446570 of the rest per sub-block. See tests/nodes/dd3/README.md.
    const std::uint32_t frames = 8u;
    const double decay = std::exp(-static_cast<double>(frames) / (K_SAMPLE_RATE * K_SLEW_TAU_SECONDS));
    Clock clock = fast_clock();
    clock.set_time(1.0f);
    const double start_log = std::log(K_FAST_RATE_HZ);
    const double end_log = std::log(K_SLOW_RATE_HZ);

    const double after_log = std::log(clock.advance(frames));

    const double distance = start_log - end_log;
    const double log_error = K_FLOAT_ROUNDOFF * (start_log + end_log + after_log +
                                                 static_cast<double>(frames + 3u) * distance * decay);
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error((start_log - after_log) / distance, 1.0 - decay, log_error / distance)));
}

TEST(Small_Dd3Clock, Dd3ClockResetSnapsWithoutGliding)
{
    // Before reset() the control still sits at the fast end, and reset() puts it on the target.
    const double target = Clock::clock_for_time(0.75);
    Clock clock = fast_clock();
    clock.set_time(0.75f);
    const double before = clock.sample_rate_hz();

    clock.reset();

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        before, K_FAST_RATE_HZ, K_FAST_RATE_HZ * log_rounding_relative_error(K_FAST_RATE_HZ), 1.0)));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        clock.sample_rate_hz(), target, target * log_rounding_relative_error(target), 1.0)));
}
