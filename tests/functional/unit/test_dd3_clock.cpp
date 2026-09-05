/**************************************************************************/
/* test_dd3_clock.cpp                                                     */
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
 * @file test_dd3_clock.cpp
 * @brief Tests for the DD-3 variable-rate clock and its slew.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/dd3/Dd3Clock.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>

TEST(Unit_Dsp, Dd3ClockHitsThePublishedEndpointsAndTheDerivedMidpoint)
{
    // N5: fs(TIME) = 82500 * 4.125^(-TIME); 4.125^0.5 = 2.0310096, so fs(0.5) = 40620.4 Hz.
    using Clock = SushiDSP::dd3::Dd3Clock;
    EXPECT_NEAR(Clock::clock_for_time(0.0), 82500.0, 1e-6);
    EXPECT_NEAR(Clock::clock_for_time(1.0), 20000.0, 1e-6);
    EXPECT_NEAR(Clock::clock_for_time(0.5), 40620.4, 0.5);
    EXPECT_NEAR(Clock::clock_for_time(0.25), 57889.5, 0.5);
    // Acceptance row 1: within 2 % of the published 20-82.5 kHz curve at both endpoints.
    EXPECT_LE(std::abs(Clock::clock_for_time(0.0) - 82500.0) / 82500.0, 0.02);
    EXPECT_LE(std::abs(Clock::clock_for_time(1.0) - 20000.0) / 20000.0, 0.02);
}

TEST(Unit_Dsp, Dd3ClockIsMonotoneAndStaysInsideItsRange)
{
    using Clock = SushiDSP::dd3::Dd3Clock;
    double previous = 1e9;
    for (int step = 0; step <= 1000; ++step)
    {
        const double rate = Clock::clock_for_time(static_cast<double>(step) / 1000.0);
        EXPECT_LT(rate, previous) << step;
        EXPECT_GE(rate, 20000.0 - 1e-6);
        EXPECT_LE(rate, 82500.0 + 1e-6);
        previous = rate;
    }
}

TEST(Unit_Dsp, Dd3ClockDelayTimesLandOnThePublishedRanges)
{
    // N5's mode table, and the reason it is not exact: N = 1024/4096/16384 against published
    // times computed with N = 1000/4000/16000. The delay-time errors are printed, not asserted;
    // acceptance row 1 is a check on fs, which the case above makes.
    using Clock = SushiDSP::dd3::Dd3Clock;
    struct Mode { const char* name; double n; double fast_ms; double slow_ms; };
    const Mode modes[3] = { { "S", 1024.0, 12.5, 50.0 }, { "M", 4096.0, 50.0, 200.0 },
                            { "L", 16384.0, 200.0, 800.0 } };
    for (const Mode& mode : modes)
    {
        const double fast = 1000.0 * mode.n / Clock::clock_for_time(0.0);
        const double slow = 1000.0 * mode.n / Clock::clock_for_time(1.0);
        std::printf("[ measure ] DD-3 mode %s: %.6f - %.6f ms (published %.1f - %.1f, "
                    "error %+.2f %% / %+.2f %%)\n",
                    mode.name, fast, slow, mode.fast_ms, mode.slow_ms,
                    100.0 * (fast / mode.fast_ms - 1.0), 100.0 * (slow / mode.slow_ms - 1.0));
        EXPECT_NEAR(fast, mode.fast_ms, 0.12 * mode.fast_ms);
        EXPECT_NEAR(slow, mode.slow_ms, 0.12 * mode.slow_ms);
    }
    // The exact values this plan computed, so a change to the VCO law shows up here.
    EXPECT_NEAR(1000.0 * 1024.0 / Clock::clock_for_time(0.0), 12.412121, 1e-5);
    EXPECT_NEAR(1000.0 * 16384.0 / Clock::clock_for_time(1.0), 819.2, 1e-4);
}

TEST(Unit_Dsp, Dd3ClockSlewMatchesTheR56C52Pole)
{
    // tau = 22000 * 2.7e-8 = 5.94e-4 s = 0.594 ms = 28.512 samples at 48 kHz.
    // After one tau the control has covered 1 - e^-1 = 0.6321206 of the log distance, and
    // log2(4.125) = 2.0443847 octaves, so the pitch has moved 1.2922934 octaves:
    //   fs = 82500 * 2^-1.2922934 = 82500 * 0.408259 = 33681.4 Hz
    using Clock = SushiDSP::dd3::Dd3Clock;
    EXPECT_NEAR(Clock::kSlewTimeConstantSeconds, 5.94e-4, 1e-12);
    SushiDSP::dd3::Dd3Clock clock;
    clock.prepare(48000.0);
    clock.set_time(0.0f);
    clock.reset();
    ASSERT_NEAR(clock.sample_rate_hz(), 82500.0, 1.0);
    clock.set_time(1.0f);
    const double after_one_tau = clock.advance(28u);      // 28.512 samples, rounded down
    std::printf("[ measure ] DD-3 clock after one R56/C52 time constant: %.1f Hz\n", after_one_tau);
    EXPECT_NEAR(after_one_tau, 33681.4, 400.0);
}

TEST(Unit_Dsp, Dd3ClockAdvancesOneQuarterOfTheRemainingDistancePerSubBlock)
{
    // Ruling 11: 8 frames at 48 kHz is 8/28.512 = 0.28058305 time constants, so the pole closes
    // 1 - exp(-0.28058305) = 0.2446570 of what is left, every sub-block. That figure is what
    // makes the pitch trace a glide rather than a jump, so it is pinned here.
    using Clock = SushiDSP::dd3::Dd3Clock;
    SushiDSP::dd3::Dd3Clock clock;
    clock.prepare(48000.0);
    clock.set_time(0.0f);
    clock.reset();
    clock.set_time(1.0f);
    const double start_log = std::log(82500.0);
    const double end_log = std::log(20000.0);
    const double after = std::log(clock.advance(8u));
    EXPECT_NEAR((after - start_log) / (end_log - start_log), 0.2446570, 5e-3);
}

TEST(Unit_Dsp, Dd3ClockResetSnapsWithoutGliding)
{
    SushiDSP::dd3::Dd3Clock clock;
    clock.prepare(48000.0);
    clock.set_time(0.75f);
    clock.reset();
    EXPECT_NEAR(clock.sample_rate_hz(), SushiDSP::dd3::Dd3Clock::clock_for_time(0.75), 1.0);
}
