/**************************************************************************/
/* test_profile_runtime.cpp                                               */
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
 * @file test_profile_runtime.cpp
 * @brief Tests the block timeline mark and the name and thread calls.
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

TEST(Medium_ProfileRuntime, MarkBlockOpensPeriodsWithTheBudgetOfOneBlock)
{
    SushiDSP::profile::ProfileSession session;
    const SushiDSP::profile::ProfileStart started =
        session.start_window(SushiDSP::profile::names::K_BLOCK_TIMELINE, K_WARMUP, K_ACTIVE);
    if (!started.started())
        GTEST_SKIP() << started.message;

    for (std::uint32_t block = 0; block < K_WARMUP + K_ACTIVE + 1; ++block)
        SushiDSP::profile::mark_block(256U, 48000.0);
    const SushiDSP::profile::ProfileFinish finished = session.finish();

    const SushiDSP::profile::TimelineSummary summary =
        finished.record.summarise(SushiDSP::profile::names::K_BLOCK_TIMELINE);
    EXPECT_EQ(summary.periods, K_ACTIVE);
    EXPECT_DOUBLE_EQ(summary.budget_ns, 5333333.0);
}

TEST(Small_ProfileRuntime, MarkBlockWithNoSessionDoesNothing)
{
    SushiDSP::profile::mark_block(256U, 48000.0);
    SUCCEED();
}

TEST(Small_ProfileRuntime, InternNameGivesTheSameIdForTheSameTextAndNoneForNone)
{
    if (!SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records nothing";

    const SushiDSP::profile::NameID first = SushiDSP::profile::intern_name("test.interned");
    const SushiDSP::profile::NameID again = SushiDSP::profile::intern_name("test.interned");

    EXPECT_NE(first, 0u);
    EXPECT_EQ(first, again);
    EXPECT_EQ(SushiDSP::profile::intern_name(""), 0u);
}
