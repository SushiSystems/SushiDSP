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
 * @file test_dd3_clock.cpp
 * @brief Tests for the DD-3 variable-rate clock and its slew.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/dd3/dd3_clock.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>

TEST(Dd3Clock, Dd3ClockHitsThePublishedEndpointsAndTheDerivedMidpoint)
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

TEST(Dd3Clock, Dd3ClockIsMonotoneAndStaysInsideItsRange)
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

TEST(Dd3Clock, Dd3ClockDelayTimesLandOnThePublishedRanges)
{
    // The delay-time errors are printed, not asserted. See tests/nodes/dd3/README.md.
    using Clock = SushiDSP::dd3::Dd3Clock;
    struct Mode
    {
            const char* name;
            double n;
            double fast_ms;
            double slow_ms;
    };
    const Mode modes[3] = {
        {"S", 1024.0, 12.5, 50.0}, {"M", 4096.0, 50.0, 200.0}, {"L", 16384.0, 200.0, 800.0}};
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

TEST(Dd3Clock, Dd3ClockSlewMatchesTheR56C52Pole)
{
    // After one tau the clock reads 33681.4 Hz. See tests/nodes/dd3/README.md.
    using Clock = SushiDSP::dd3::Dd3Clock;
    EXPECT_NEAR(Clock::K_SLEW_TIME_CONSTANT_SECONDS, 5.94e-4, 1e-12);
    SushiDSP::dd3::Dd3Clock clock;
    clock.prepare(48000.0);
    clock.set_time(0.0f);
    clock.reset();
    ASSERT_NEAR(clock.sample_rate_hz(), 82500.0, 1.0);
    clock.set_time(1.0f);
    const double after_one_tau = clock.advance(28u); // 28.512 samples, rounded down
    std::printf("[ measure ] DD-3 clock after one R56/C52 time constant: %.1f Hz\n", after_one_tau);
    EXPECT_NEAR(after_one_tau, 33681.4, 400.0);
}

TEST(Dd3Clock, Dd3ClockAdvancesOneQuarterOfTheRemainingDistancePerSubBlock)
{
    // The pole closes 0.2446570 of the rest per sub-block. See tests/nodes/dd3/README.md.
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

TEST(Dd3Clock, Dd3ClockResetSnapsWithoutGliding)
{
    SushiDSP::dd3::Dd3Clock clock;
    clock.prepare(48000.0);
    clock.set_time(0.75f);
    clock.reset();
    EXPECT_NEAR(clock.sample_rate_hz(), SushiDSP::dd3::Dd3Clock::clock_for_time(0.75), 1.0);
}
