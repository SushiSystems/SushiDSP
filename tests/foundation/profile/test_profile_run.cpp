/**************************************************************************/
/* test_profile_run.cpp                                                   */
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
 * @file test_profile_run.cpp
 * @brief Tests that run_periods opens each period on its marker and runs the body there.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/profile/period_marker.hpp>
#include <SushiDSP/foundation/profile/profile_run.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

namespace
{
    using SushiDSP::profile::InMemoryPeriodMarker;

    /// @brief Holds the warm-up periods of every run below.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of every run below.
    constexpr std::uint32_t K_ACTIVE = 5;

    /// @brief Holds the periods opened by a window: each body period and the one that closes it.
    constexpr std::size_t K_MARKS = K_WARMUP + K_ACTIVE + 1;

    /// @brief Holds the budget every period of the budget test carries.
    constexpr std::uint64_t K_BUDGET_NS = 1000000;

    /// @brief Holds the timeline every run below follows.
    constexpr const char* K_TIMELINE = SushiDSP::profile::names::K_BENCH_TIMELINE;

    /// @brief Returns 1, 2, up to @p last.
    std::vector<std::size_t> counting_up(std::size_t last)
    {
        std::vector<std::size_t> counts(last);
        std::iota(counts.begin(), counts.end(), std::size_t{1});
        return counts;
    }

    /// @brief Expects every period of @p marker to be on @p timeline with budget @p budget_ns.
    void expect_every_mark_is(const InMemoryPeriodMarker& marker, const std::string& timeline,
                              std::uint64_t budget_ns)
    {
        for (const SushiDSP::profile::MarkedPeriod& mark : marker.marks())
        {
            EXPECT_EQ(mark.timeline, timeline);
            EXPECT_EQ(mark.budget_ns, budget_ns);
        }
    }
} // namespace

TEST(Small_ProfileRun, TheBodyRunsOncePerWarmUpAndActivePeriod)
{
    InMemoryPeriodMarker marker;
    int calls = 0;

    SushiDSP::profile::run_periods(marker, K_TIMELINE, K_WARMUP, K_ACTIVE, 0U, [&] { ++calls; });

    EXPECT_EQ(calls, static_cast<int>(K_WARMUP + K_ACTIVE));
}

TEST(Small_ProfileRun, EveryBodyRunsAfterItsPeriodIsOpened)
{
    InMemoryPeriodMarker marker;
    std::vector<std::size_t> marks_seen;

    SushiDSP::profile::run_periods(marker, K_TIMELINE, K_WARMUP, K_ACTIVE, 0U,
                                   [&] { marks_seen.push_back(marker.marks().size()); });

    EXPECT_EQ(marks_seen, counting_up(K_WARMUP + K_ACTIVE));
}

TEST(Small_ProfileRun, OnePeriodIsOpenedAfterTheLastBodySoTheLastOneCloses)
{
    InMemoryPeriodMarker marker;

    SushiDSP::profile::run_periods(marker, K_TIMELINE, K_WARMUP, K_ACTIVE, 0U, [] {});

    EXPECT_EQ(marker.marks().size(), K_MARKS);
}

TEST(Small_ProfileRun, EveryPeriodCarriesTheBudgetTheRunWasGiven)
{
    InMemoryPeriodMarker marker;

    SushiDSP::profile::run_periods(marker, K_TIMELINE, K_WARMUP, K_ACTIVE, K_BUDGET_NS, [] {});

    ASSERT_EQ(marker.marks().size(), K_MARKS);
    expect_every_mark_is(marker, K_TIMELINE, K_BUDGET_NS);
}

// Only a program built with SD_ENABLE_PROFILER off holds this test.
#if !SD_ENABLE_PROFILER
TEST(Small_ProfileRun, ABodyDoesNotRunWhenTheBuildRecordsNothing)
{
    int calls = 0;

    const SushiDSP::profile::ProfiledRun run = SushiDSP::profile::run_profiled(
        K_TIMELINE, K_WARMUP, K_ACTIVE, 0U, [&] { ++calls; });

    EXPECT_EQ(calls, 0);
    EXPECT_EQ(run.start.status, SushiDSP::profile::ProfileStartStatus::NotBuilt);
}
#endif
