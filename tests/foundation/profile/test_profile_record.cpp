/**************************************************************************/
/* test_profile_record.cpp                                                */
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
 * @file test_profile_record.cpp
 * @brief Tests the questions a finished run answers: checks, summaries and subjects.
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

TEST(Medium_ProfileRecord, ATightBoundFailsWithItsReasonAndALooseOnePasses)
{
    const SushiDSP::profile::ProfiledRun run = SushiDSP::profile::run_profiled(
        K_TIMELINE, K_WARMUP, K_ACTIVE, 0U,
        []
        {
            SD_ZONE("test.checked");
        });
    if (!run.start.started())
        GTEST_SKIP() << "this build records nothing";

    const SushiDSP::profile::ProfileCheckReport tight = run.record.check(
        R"([{"subject": "timeline:bench.block", "statistic": "max", "at_most_ns": 1}])");
    ASSERT_EQ(tight.verdicts.size(), 1u) << tight.error;
    EXPECT_FALSE(tight.all_passed());
    EXPECT_NE(tight.verdicts[0].reason.find("above at most 1 ns"), std::string::npos)
        << tight.verdicts[0].reason;

    const SushiDSP::profile::ProfileCheckReport loose = run.record.check(
        R"([{"subject": "timeline:bench.block", "statistic": "max", "at_most_ns": 60000000000}])");
    EXPECT_TRUE(loose.all_passed()) << loose.error;
}

TEST(Medium_ProfileRecord, AChecksFileThatIsNotValidIsRefusedWithAReason)
{
    const SushiDSP::profile::ProfiledRun run = SushiDSP::profile::run_profiled(
        K_TIMELINE, K_WARMUP, K_ACTIVE, 0U,
        []
        {
            SD_INSTANT("test.noop");
        });
    if (!run.start.started())
        GTEST_SKIP() << "this build records nothing";

    const SushiDSP::profile::ProfileCheckReport report = run.record.check("{}");

    EXPECT_FALSE(report.error.empty());
    EXPECT_FALSE(report.all_passed());
}

TEST(Small_ProfileRecord, AnEmptyRecordAnswersEveryQuestionWithNothing)
{
    const SushiDSP::profile::ProfileRecord record;

    EXPECT_TRUE(record.empty());
    EXPECT_EQ(record.active_periods(), 0u);
    EXPECT_EQ(record.warmup_periods(), 0u);
    EXPECT_TRUE(record.subjects().empty());
    EXPECT_EQ(record.find("timeline:bench.block"), nullptr);
    EXPECT_EQ(record.zone_calls("anything"), 0u);
    EXPECT_TRUE(record.lanes().empty());
    EXPECT_EQ(record.summarise("bench.block").periods, 0u);
}

TEST(Small_ProfileRecord, AReportWithNoVerdictsIsNotAPass)
{
    const SushiDSP::profile::ProfileCheckReport report;

    EXPECT_FALSE(report.all_passed());
}
