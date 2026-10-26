/**************************************************************************/
/* test_profile_session.cpp                                               */
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
 * @file test_profile_session.cpp
 * @brief Tests how a profiling session starts, refuses and finishes.
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

TEST(Small_ProfileSession, ASessionThatIsNotStartedReturnsAnEmptyRecord)
{
    SushiDSP::profile::ProfileSession session;

    const SushiDSP::profile::ProfileFinish finished = session.finish();

    EXPECT_TRUE(finished.record.empty());
    EXPECT_FALSE(session.running());
}

TEST(Small_ProfileSession, NoConfigurationVariableMeansNothingIsRequested)
{
    SushiDSP::profile::ProfileSession session;
    const SushiDSP::profile::ProfileStart started = session.start_from_environment();

    // The test environment does not set SD_PROFILE_CONFIG.
    EXPECT_EQ(started.status, SushiDSP::profile::ProfileStartStatus::NotRequested);
}

TEST(Medium_ProfileSession, AConfigurationThatIsNotValidIsRefusedWithItsReason)
{
    if (!SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records nothing";
    SushiDSP::profile::ProfileSession session;

    const SushiDSP::profile::ProfileStart started = session.start("{\"nonsense\": 1}");

    EXPECT_EQ(started.status, SushiDSP::profile::ProfileStartStatus::ConfigRefused);
    EXPECT_FALSE(started.message.empty());
    EXPECT_FALSE(session.running());
}

TEST(Medium_ProfileSession, ASecondSessionInTheProcessIsRefused)
{
    if (!SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records nothing";
    SushiDSP::profile::ProfileSession first;
    ASSERT_TRUE(first.start_window(K_TIMELINE, K_WARMUP, K_ACTIVE).started());
    SushiDSP::profile::ProfileSession second;

    const SushiDSP::profile::ProfileStart started = second.start_window(K_TIMELINE, 0U, 1U);

    EXPECT_EQ(started.status, SushiDSP::profile::ProfileStartStatus::AlreadyRunning);
    EXPECT_TRUE(first.running());
}

TEST(Medium_ProfileSession, AnObjectThatRunsRefusesASecondStart)
{
    if (!SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records nothing";
    SushiDSP::profile::ProfileSession session;
    ASSERT_TRUE(session.start_window(K_TIMELINE, K_WARMUP, K_ACTIVE).started());

    const SushiDSP::profile::ProfileStart again = session.start_window(K_TIMELINE, 0U, 1U);

    EXPECT_EQ(again.status, SushiDSP::profile::ProfileStartStatus::AlreadyRunning);
}
