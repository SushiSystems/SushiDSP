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
 * @file test_zones.cpp
 * @brief Tests the zone and counter macros against a running session.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/profile/profile.hpp>
#include <SushiDSP/foundation/profile/profile_run.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include "foundation/profile/recording_build_test.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace
{
    /// @brief Holds the warm-up periods of every run below.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of every run below.
    constexpr std::uint32_t K_ACTIVE = 5;

    /// @brief Holds the periods the skip window passes over before it records.
    constexpr std::uint32_t K_SKIPPED = 2;

    /// @brief Holds the periods the skip window records.
    constexpr std::uint32_t K_RECORDED = 3;

    /// @brief Holds the timeline every run below follows.
    constexpr const char* K_TIMELINE = SushiDSP::profile::names::K_BENCH_TIMELINE;

    /// @brief Holds a window that skips K_SKIPPED periods and then records K_RECORDED.
    constexpr const char* K_SKIPPING_WINDOW =
        R"({"sushiprofiler": 1, "timeline": "bench.block",
            "window": {"skip": 2, "wait": 0, "warmup": 0, "active": 3, "repeat": 1}})";

    /// @brief Opens @p periods periods on @p session, each with one zone, and one more to close.
    void mark_periods_with_a_zone(SushiDSP::profile::ProfileSession& session,
                                  std::uint32_t periods)
    {
        SushiDSP::profile::run_periods(session, K_TIMELINE, 0U, periods, 0U,
                                       []
                                       {
                                           SD_ZONE("test.skipped");
                                       });
    }

    /// @brief Adds every value of @p values to the counter test.sum in the open period.
    void add_to_the_sum(const std::vector<double>& values)
    {
        for (const double value : values)
            SD_COUNTER("test.sum", "count", Add, value);
    }

    /// @brief Offers every value of @p values to the counter test.peak in the open period.
    void offer_to_the_peak(const std::vector<double>& values)
    {
        for (const double value : values)
            SD_COUNTER("test.peak", "count", Max, value);
    }

    /// @brief Runs K_ACTIVE periods whose body is @p body and returns the run.
    template <typename Body>
    SushiDSP::profile::ProfiledRun run_window(Body&& body)
    {
        return SushiDSP::profile::run_profiled(K_TIMELINE, K_WARMUP, K_ACTIVE, 0U,
                                               std::forward<Body>(body));
    }

    class Medium_ProfileZones : public SushiDSP::profile::test::RecordingBuildTest
    {
    };

    class Medium_ProfileCounters : public SushiDSP::profile::test::RecordingBuildTest
    {
    };
} // namespace

TEST_F(Medium_ProfileZones, AZoneInsideTheWindowIsRecordedOncePerActivePeriod)
{
    const SushiDSP::profile::ProfiledRun run = run_window(
        []
        {
            SD_ZONE("test.zone");
        });
    ASSERT_TRUE(run.start.started()) << run.start.message;

    EXPECT_EQ(run.record.zone_calls("test.zone"), K_ACTIVE);
}

TEST_F(Medium_ProfileZones, APeriodTheWindowSkipsWritesNothing)
{
    SushiDSP::profile::ProfileSession session;
    const SushiDSP::profile::ProfileStart started = session.start(K_SKIPPING_WINDOW);
    ASSERT_TRUE(started.started()) << started.message;

    mark_periods_with_a_zone(session, K_SKIPPED + K_RECORDED + 1U);
    const SushiDSP::profile::ProfileFinish finished = session.finish();

    EXPECT_EQ(finished.record.zone_calls("test.skipped"), K_RECORDED);
}

TEST_F(Medium_ProfileCounters, AnAddCounterSumsItsValuesInEachPeriod)
{
    const SushiDSP::profile::ProfiledRun run = run_window([] { add_to_the_sum({2.0, 2.0}); });
    ASSERT_TRUE(run.start.started()) << run.start.message;

    const SushiDSP::profile::ProfileSubject* const sum = run.record.find("counter:test.sum");

    ASSERT_NE(sum, nullptr);
    EXPECT_EQ(sum->samples, std::vector<double>(K_ACTIVE, 4.0));
}

TEST_F(Medium_ProfileCounters, AMaxCounterKeepsItsLargestValueInEachPeriod)
{
    const SushiDSP::profile::ProfiledRun run =
        run_window([] { offer_to_the_peak({1.0, 5.0, 3.0}); });
    ASSERT_TRUE(run.start.started()) << run.start.message;

    const SushiDSP::profile::ProfileSubject* const peak = run.record.find("counter:test.peak");

    ASSERT_NE(peak, nullptr);
    EXPECT_EQ(peak->samples, std::vector<double>(K_ACTIVE, 5.0));
}
