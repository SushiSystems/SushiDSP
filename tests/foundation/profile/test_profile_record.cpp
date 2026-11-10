/**************************************************************************/
/* test_profile_record.cpp                                                 */
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
 * @file test_profile_record.cpp
 * @brief Tests the questions a finished run answers: checks, counts and subjects.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/profile/profile.hpp>
#include <SushiDSP/foundation/profile/profile_run.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include "foundation/profile/recording_build_test.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using SushiDSP::profile::ProfileRecord;
    using SushiDSP::profile::ProfileSubject;

    /// @brief Holds the warm-up periods of every run below.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of every run below.
    constexpr std::uint32_t K_ACTIVE = 5;

    /// @brief Holds the timeline every run below follows.
    constexpr const char* K_TIMELINE = SushiDSP::profile::names::K_BENCH_TIMELINE;

    /// @brief Holds a check that no run can meet: the longest block under one nanosecond.
    constexpr const char* K_TIGHT_CHECK =
        R"([{"subject": "timeline:bench.block", "statistic": "max", "at_most_ns": 1}])";

    /// @brief Holds a check that any run meets: the longest block under a minute.
    constexpr const char* K_LOOSE_CHECK =
        R"([{"subject": "timeline:bench.block", "statistic": "max", "at_most_ns": 60000000000}])";

    /// @brief Builds a counter subject named @p name with one sample, @p value.
    ProfileSubject counter_subject(const std::string& name, double value)
    {
        return ProfileSubject{name, "count", {value}};
    }

    /// @brief Builds a record whose counter `test.shared` has two sites and `test.alone` has one.
    ProfileRecord record_with_shared_and_single_sites()
    {
        return ProfileRecord::from_subjects({counter_subject("counter:test.shared", 1.0),
                                             counter_subject("counter:test.shared", 2.0),
                                             counter_subject("counter:test.alone", 3.0)});
    }

    /// @brief Runs a window whose body writes one zone and returns the run.
    SushiDSP::profile::ProfiledRun run_one_zone_window()
    {
        return SushiDSP::profile::run_profiled(K_TIMELINE, K_WARMUP, K_ACTIVE, 0U,
                                               []
                                               {
                                                   SD_ZONE("test.checked");
                                               });
    }

    /// @brief Runs a window whose body does nothing and returns the run.
    SushiDSP::profile::ProfiledRun run_empty_window()
    {
        return SushiDSP::profile::run_profiled(K_TIMELINE, K_WARMUP, K_ACTIVE, 0U, [] {});
    }

    class Medium_ProfileRecord : public SushiDSP::profile::test::RecordingBuildTest
    {
    };
} // namespace

TEST_F(Medium_ProfileRecord, ATightBoundFailsWithItsReason)
{
    const SushiDSP::profile::ProfiledRun run = run_one_zone_window();
    ASSERT_TRUE(run.start.started()) << run.start.message;

    const SushiDSP::profile::ProfileCheckReport report = run.record.check(K_TIGHT_CHECK);

    ASSERT_EQ(report.verdicts.size(), 1u) << report.error;
    EXPECT_FALSE(report.all_passed());
    EXPECT_NE(report.verdicts[0].reason.find("above at most 1 ns"), std::string::npos)
        << report.verdicts[0].reason;
}

TEST_F(Medium_ProfileRecord, ALooseBoundPasses)
{
    const SushiDSP::profile::ProfiledRun run = run_one_zone_window();
    ASSERT_TRUE(run.start.started()) << run.start.message;

    const SushiDSP::profile::ProfileCheckReport report = run.record.check(K_LOOSE_CHECK);

    EXPECT_TRUE(report.all_passed()) << report.error;
}

TEST_F(Medium_ProfileRecord, TheActivePeriodsOfTheWindowAreCounted)
{
    const SushiDSP::profile::ProfiledRun run = run_empty_window();
    ASSERT_TRUE(run.start.started()) << run.start.message;

    EXPECT_EQ(run.record.active_periods(), K_ACTIVE);
}

TEST_F(Medium_ProfileRecord, TheWarmUpPeriodsOfTheWindowAreCountedApartFromTheActiveOnes)
{
    const SushiDSP::profile::ProfiledRun run = run_empty_window();
    ASSERT_TRUE(run.start.started()) << run.start.message;

    EXPECT_EQ(run.record.warmup_periods(), K_WARMUP);
}

TEST(Small_ProfileRecord, AChecksFileThatIsNotValidIsRefusedWithAReason)
{
    const ProfileRecord record;

    const SushiDSP::profile::ProfileCheckReport report = record.check("{}");

    EXPECT_FALSE(report.error.empty());
    EXPECT_FALSE(report.all_passed());
}

TEST(Small_ProfileRecord, AnEmptyRecordAnswersEveryQuestionWithNothing)
{
    const ProfileRecord record;

    EXPECT_TRUE(record.empty());
    EXPECT_EQ(record.active_periods(), 0u);
    EXPECT_EQ(record.warmup_periods(), 0u);
    EXPECT_TRUE(record.subjects().empty());
    EXPECT_EQ(record.find("timeline:bench.block"), nullptr);
    EXPECT_EQ(record.lookup("timeline:bench.block").status,
              SushiDSP::profile::ProfileLookupStatus::Missing);
    EXPECT_EQ(record.zone_calls("anything"), 0u);
    EXPECT_TRUE(record.lanes().empty());
    EXPECT_EQ(record.summarise("bench.block").periods, 0u);
}

TEST(Small_ProfileRecord, AReportWithNoVerdictsIsNotAPass)
{
    const SushiDSP::profile::ProfileCheckReport report;

    EXPECT_FALSE(report.all_passed());
}

TEST(Small_ProfileRecord, TwoSitesSharingANameAreAmbiguousAndNeverReadAsOne)
{
    const ProfileRecord record = record_with_shared_and_single_sites();

    const SushiDSP::profile::ProfileLookup shared = record.lookup("counter:test.shared");

    EXPECT_EQ(shared.status, SushiDSP::profile::ProfileLookupStatus::Ambiguous);
    EXPECT_EQ(shared.matches, 2u);
    EXPECT_EQ(shared.subject, &record.subjects()[0]);
    EXPECT_EQ(record.find("counter:test.shared"), nullptr);
}

TEST(Small_ProfileRecord, ASingleSiteIsFound)
{
    const ProfileRecord record = record_with_shared_and_single_sites();

    const SushiDSP::profile::ProfileLookup alone = record.lookup("counter:test.alone");

    EXPECT_EQ(alone.status, SushiDSP::profile::ProfileLookupStatus::Found);
    EXPECT_EQ(alone.matches, 1u);
    EXPECT_EQ(alone.subject, &record.subjects()[2]);
    EXPECT_EQ(record.find("counter:test.alone"), alone.subject);
}

TEST(Small_ProfileRecord, AnUnknownNameIsMissing)
{
    const ProfileRecord record = record_with_shared_and_single_sites();

    const SushiDSP::profile::ProfileLookup none = record.lookup("counter:test.nothing");

    EXPECT_EQ(none.status, SushiDSP::profile::ProfileLookupStatus::Missing);
    EXPECT_EQ(none.matches, 0u);
    EXPECT_EQ(none.subject, nullptr);
    EXPECT_EQ(record.find("counter:test.nothing"), nullptr);
}
