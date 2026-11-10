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
 * @file test_profile_session.cpp
 * @brief Tests how a profiling session starts, refuses and finishes.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/profile/environment_reader.hpp>
#include <SushiDSP/foundation/profile/profile_run.hpp>
#include <SushiDSP/foundation/profile/profile_session.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include "foundation/profile/recording_build_test.hpp"

#include <cstdint>
#include <string>

namespace
{
    using SushiDSP::profile::ProfileSession;
    using SushiDSP::profile::ProfileStart;

    /// @brief Holds the warm-up periods of every run below.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of every run below.
    constexpr std::uint32_t K_ACTIVE = 5;

    /// @brief Holds the budget the budget test gives every period, in nanoseconds.
    constexpr std::uint64_t K_BUDGET_NS = 1000000;

    /// @brief Holds the timeline every run below follows.
    constexpr const char* K_TIMELINE = SushiDSP::profile::names::K_BENCH_TIMELINE;

    /// @brief Starts a second window on the session that already runs.
    ProfileStart start_on_the_running_object(ProfileSession& running)
    {
        return running.start_window(K_TIMELINE, 0U, 1U);
    }

    /// @brief Starts a window on a second session while @p running holds the process's one.
    ProfileStart start_on_another_object(ProfileSession& running)
    {
        static_cast<void>(running);
        ProfileSession another;
        return another.start_window(K_TIMELINE, 0U, 1U);
    }

    /// @brief One way to start a second session while one runs.
    struct SecondStartRow
    {
            const char* name;
            ProfileStart (*start_second)(ProfileSession& running);
    };

    /// @brief Names a second-start row by its name.
    std::string second_start_row_name(const ::testing::TestParamInfo<SecondStartRow>& info)
    {
        return info.param.name;
    }

    class Small_ProfileSessionConfig : public SushiDSP::profile::test::RecordingBuildTest
    {
    };

    class Medium_ProfileSession : public SushiDSP::profile::test::RecordingBuildTest
    {
    };

    class Medium_ProfileSessionSecondStart
        : public SushiDSP::profile::test::RecordingBuildTest,
          public ::testing::WithParamInterface<SecondStartRow>
    {
    };
} // namespace

TEST(Small_ProfileSession, ASessionThatIsNotStartedReturnsAnEmptyRecord)
{
    ProfileSession session;

    const SushiDSP::profile::ProfileFinish finished = session.finish();

    EXPECT_TRUE(finished.record.empty());
    EXPECT_FALSE(session.running());
}

TEST(Small_ProfileSession, NoConfigurationVariableMeansNothingIsRequested)
{
    const SushiDSP::profile::InMemoryEnvironment environment;
    ProfileSession session;

    const ProfileStart started = session.start_from_environment(environment);

    EXPECT_EQ(started.status, SushiDSP::profile::ProfileStartStatus::NotRequested);
}

TEST(Small_ProfileSession, AnEmptyConfigurationVariableMeansNothingIsRequested)
{
    SushiDSP::profile::InMemoryEnvironment environment;
    environment.set(ProfileSession::K_CONFIG_VARIABLE, "");
    ProfileSession session;

    const ProfileStart started = session.start_from_environment(environment);

    EXPECT_EQ(started.status, SushiDSP::profile::ProfileStartStatus::NotRequested);
}

TEST_F(Small_ProfileSessionConfig, AConfigurationThatIsNotValidIsRefusedWithItsReason)
{
    ProfileSession session;

    const ProfileStart started = session.start("{\"nonsense\": 1}");

    EXPECT_EQ(started.status, SushiDSP::profile::ProfileStartStatus::ConfigRefused);
    EXPECT_FALSE(started.message.empty());
    EXPECT_FALSE(session.running());
}

TEST_P(Medium_ProfileSessionSecondStart, ASecondStartWhileOneRunsIsRefused)
{
    ProfileSession first;
    ASSERT_TRUE(first.start_window(K_TIMELINE, K_WARMUP, K_ACTIVE).started());

    const ProfileStart second = GetParam().start_second(first);

    EXPECT_EQ(second.status, SushiDSP::profile::ProfileStartStatus::AlreadyRunning);
    EXPECT_TRUE(first.running());
}

INSTANTIATE_TEST_SUITE_P(
    Starts, Medium_ProfileSessionSecondStart,
    ::testing::Values(SecondStartRow{"OnTheObjectThatRuns", start_on_the_running_object},
                      SecondStartRow{"OnAnotherObject", start_on_another_object}),
    second_start_row_name);

TEST_F(Medium_ProfileSession, AMarkCarriesItsBudgetIntoTheRecord)
{
    ProfileSession session;
    ASSERT_TRUE(session.start_window(K_TIMELINE, K_WARMUP, K_ACTIVE).started());

    SushiDSP::profile::run_periods(session, K_TIMELINE, K_WARMUP, K_ACTIVE, K_BUDGET_NS, [] {});
    const SushiDSP::profile::ProfileFinish finished = session.finish();

    EXPECT_EQ(finished.record.summarise(K_TIMELINE).budget_ns, static_cast<double>(K_BUDGET_NS));
}
