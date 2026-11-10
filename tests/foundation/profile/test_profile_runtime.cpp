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
 * @file test_profile_runtime.cpp
 * @brief Tests the block budget and the block mark behind the runtime calls.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/profile/period_marker.hpp>
#include <SushiDSP/foundation/profile/profile_run.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include "foundation/profile/recording_build_test.hpp"

#include <cstdint>
#include <string>

namespace
{
    /// @brief Holds the warm-up periods of the session test.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of the session test.
    constexpr std::uint32_t K_ACTIVE = 5;

    /// @brief Holds the frames of one block in the tests below.
    constexpr std::uint32_t K_FRAMES = 256;

    /// @brief Holds the rate the tests below play at, in hertz.
    constexpr double K_RATE = 48000.0;

    /// @brief Holds 256 frames at 48 kHz in nanoseconds: 256 / 48000 s is 5333333.33 ns, which
    /// the budget truncates.
    constexpr std::uint64_t K_ONE_BLOCK_NS = 5333333;

    /// @brief One block budget to compute: @p frames at @p sample_rate give @p budget_ns.
    struct BudgetRow
    {
            const char* name;
            std::uint32_t frames;
            double sample_rate;
            std::uint64_t budget_ns;
    };

    /// @brief Names a budget row by its name.
    std::string budget_row_name(const ::testing::TestParamInfo<BudgetRow>& info)
    {
        return info.param.name;
    }

    /// @brief One sample rate that opens no period.
    struct RateRow
    {
            const char* name;
            double sample_rate;
    };

    /// @brief Names a rate row by its name.
    std::string rate_row_name(const ::testing::TestParamInfo<RateRow>& info)
    {
        return info.param.name;
    }

    /// @brief Calls mark_block for @p blocks blocks of K_FRAMES frames at K_RATE.
    void mark_blocks(std::uint32_t blocks)
    {
        for (std::uint32_t block = 0; block < blocks; ++block)
            SushiDSP::profile::mark_block(K_FRAMES, K_RATE);
    }

    class Medium_ProfileRuntime : public SushiDSP::profile::test::RecordingBuildTest
    {
    };

    class Small_BlockBudget : public ::testing::TestWithParam<BudgetRow>
    {
    };

    class Small_ProfileRuntimeRate : public ::testing::TestWithParam<RateRow>
    {
    };
} // namespace

TEST_P(Small_BlockBudget, BudgetIsTheBlockDurationInNanosecondsTruncated)
{
    const BudgetRow& row = GetParam();

    EXPECT_EQ(SushiDSP::profile::block_budget_ns(row.frames, row.sample_rate), row.budget_ns);
}

// 480 frames at 48 kHz are exactly 10 ms; 64 frames at 44.1 kHz are 1451247.1655 ns.
INSTANTIATE_TEST_SUITE_P(
    ProfileRuntime, Small_BlockBudget,
    ::testing::Values(BudgetRow{"OneBlockAt48k", K_FRAMES, K_RATE, K_ONE_BLOCK_NS},
                      BudgetRow{"TenMilliseconds", 480U, 48000.0, 10000000U},
                      BudgetRow{"TruncatesAt44k1", 64U, 44100.0, 1451247U},
                      BudgetRow{"NoFramesTakeNoTime", 0U, K_RATE, 0U},
                      BudgetRow{"ZeroRateGivesNone", K_FRAMES, 0.0, 0U},
                      BudgetRow{"NegativeRateGivesNone", K_FRAMES, -48000.0, 0U}),
    budget_row_name);

TEST(Small_ProfileRuntime, MarkBlockOpensAPeriodOfTheBlockTimelineWithTheBudgetOfOneBlock)
{
    SushiDSP::profile::InMemoryPeriodMarker marker;

    SushiDSP::profile::mark_block(marker, K_FRAMES, K_RATE);

    ASSERT_EQ(marker.marks().size(), 1u);
    EXPECT_EQ(marker.marks()[0].timeline, SushiDSP::profile::names::K_BLOCK_TIMELINE);
    EXPECT_EQ(marker.marks()[0].budget_ns, K_ONE_BLOCK_NS);
}

TEST_P(Small_ProfileRuntimeRate, MarkBlockOpensNoPeriodForARateThatIsNotPositive)
{
    SushiDSP::profile::InMemoryPeriodMarker marker;

    SushiDSP::profile::mark_block(marker, K_FRAMES, GetParam().sample_rate);

    EXPECT_TRUE(marker.marks().empty());
}

INSTANTIATE_TEST_SUITE_P(ProfileRuntime, Small_ProfileRuntimeRate,
                         ::testing::Values(RateRow{"Zero", 0.0}, RateRow{"Negative", -48000.0}),
                         rate_row_name);

TEST_F(Medium_ProfileRuntime, MarkBlockOpensThePeriodsOfARunningSessionWithTheBudgetOfOneBlock)
{
    SushiDSP::profile::ProfileSession session;
    const SushiDSP::profile::ProfileStart started =
        session.start_window(SushiDSP::profile::names::K_BLOCK_TIMELINE, K_WARMUP, K_ACTIVE);
    ASSERT_TRUE(started.started()) << started.message;

    mark_blocks(K_WARMUP + K_ACTIVE + 1U);
    const SushiDSP::profile::ProfileFinish finished = session.finish();

    const SushiDSP::profile::TimelineSummary summary =
        finished.record.summarise(SushiDSP::profile::names::K_BLOCK_TIMELINE);
    EXPECT_EQ(summary.periods, K_ACTIVE);
    EXPECT_EQ(summary.budget_ns, static_cast<double>(K_ONE_BLOCK_NS));
}

TEST_F(Medium_ProfileRuntime, MarkBlockWithNoSessionLeavesNoPeriodForTheNextSession)
{
    mark_blocks(1U);
    SushiDSP::profile::ProfileSession session;
    const SushiDSP::profile::ProfileStart started =
        session.start_window(SushiDSP::profile::names::K_BLOCK_TIMELINE, 0U, 1U);
    ASSERT_TRUE(started.started()) << started.message;

    const SushiDSP::profile::ProfileFinish finished = session.finish();

    EXPECT_EQ(finished.record.summarise(SushiDSP::profile::names::K_BLOCK_TIMELINE).periods, 0u);
}
