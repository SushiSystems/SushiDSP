/**************************************************************************/
/* test_zones.cpp                                                         */
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
 * @file test_zones.cpp
 * @brief Tests the zone and counter macros against a running session.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/profile/profile.hpp>
#include <SushiDSP/foundation/profile/profile_run.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include <cstdint>
#include <string>

namespace
{
    /// @brief Holds the warm-up periods of every run below.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of every run below.
    constexpr std::uint32_t K_ACTIVE = 5;

    /// @brief Holds the timeline every run below follows.
    constexpr const char* K_TIMELINE = SushiDSP::profile::names::K_BENCH_TIMELINE;
} // namespace

TEST(Medium_Profile, AZoneInsideTheWindowIsRecordedOncePerActivePeriod)
{
    const SushiDSP::profile::ProfiledRun run = SushiDSP::profile::run_profiled(
        K_TIMELINE, K_WARMUP, K_ACTIVE, 0U,
        []
        {
            SD_ZONE("test.zone");
        });
    if (!run.start.started())
        GTEST_SKIP() << "this build records nothing";

    EXPECT_EQ(run.record.active_periods(), K_ACTIVE);
    EXPECT_EQ(run.record.warmup_periods(), K_WARMUP);
    EXPECT_EQ(run.record.zone_calls("test.zone"), K_ACTIVE);
}

TEST(Medium_Profile, APeriodTheWindowSkipsWritesNothing)
{
    SushiDSP::profile::ProfileSession session;
    const SushiDSP::profile::ProfileStart started = session.start(
        R"({"sushiprofiler": 1, "timeline": "bench.block",
            "window": {"skip": 2, "wait": 0, "warmup": 0, "active": 3, "repeat": 1}})");
    if (!started.started())
        GTEST_SKIP() << started.message;

    for (int period = 0; period < 6; ++period)
    {
        session.mark(K_TIMELINE, 0U);
        SD_ZONE("test.skipped");
    }
    session.mark(K_TIMELINE, 0U);
    const SushiDSP::profile::ProfileFinish finished = session.finish();

    EXPECT_EQ(finished.record.zone_calls("test.skipped"), 3U);
}

TEST(Medium_Profile, ACounterFoldsByItsOperationInEachPeriod)
{
    const SushiDSP::profile::ProfiledRun run = SushiDSP::profile::run_profiled(
        K_TIMELINE, K_WARMUP, K_ACTIVE, 0U,
        []
        {
            SD_COUNTER("test.sum", "count", Add, 2.0);
            SD_COUNTER("test.sum", "count", Add, 2.0);
            SD_COUNTER("test.peak", "count", Max, 1.0);
            SD_COUNTER("test.peak", "count", Max, 5.0);
            SD_COUNTER("test.peak", "count", Max, 3.0);
        });
    if (!run.start.started())
        GTEST_SKIP() << "this build records nothing";

    const SushiDSP::profile::ProfileSubject* const sum = run.record.find("counter:test.sum");
    const SushiDSP::profile::ProfileSubject* const peak = run.record.find("counter:test.peak");
    ASSERT_NE(sum, nullptr);
    ASSERT_NE(peak, nullptr);
    ASSERT_EQ(sum->samples.size(), K_ACTIVE);
    for (const double value : sum->samples)
        EXPECT_DOUBLE_EQ(value, 4.0);
    for (const double value : peak->samples)
        EXPECT_DOUBLE_EQ(value, 5.0);
}
