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
 * @brief Tests that run_profiled runs its body once per period and hands back the record.
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

TEST(Medium_ProfileRun, TheBodyRunsOncePerWarmUpAndActivePeriod)
{
    int calls = 0;

    const SushiDSP::profile::ProfiledRun run = SushiDSP::profile::run_profiled(
        K_TIMELINE, K_WARMUP, K_ACTIVE, 0U, [&] { ++calls; });

    if (!run.start.started())
        GTEST_SKIP() << "this build records nothing";
    EXPECT_EQ(calls, static_cast<int>(K_WARMUP + K_ACTIVE));
    EXPECT_EQ(run.record.active_periods(), K_ACTIVE);
}

TEST(Medium_ProfileRun, EveryPeriodCarriesTheBudgetTheRunWasGiven)
{
    const SushiDSP::profile::ProfiledRun run = SushiDSP::profile::run_profiled(
        K_TIMELINE, K_WARMUP, K_ACTIVE, 1000000U, [] {});
    if (!run.start.started())
        GTEST_SKIP() << "this build records nothing";

    EXPECT_DOUBLE_EQ(run.record.summarise(K_TIMELINE).budget_ns, 1000000.0);
}

TEST(Small_ProfileRun, ABodyDoesNotRunWhenTheBuildRecordsNothing)
{
    if (SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records";
    int calls = 0;

    const SushiDSP::profile::ProfiledRun run = SushiDSP::profile::run_profiled(
        K_TIMELINE, K_WARMUP, K_ACTIVE, 0U, [&] { ++calls; });

    EXPECT_EQ(calls, 0);
    EXPECT_EQ(run.start.status, SushiDSP::profile::ProfileStartStatus::NotBuilt);
}
