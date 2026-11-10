/**************************************************************************/
/* test_profile.cpp                                                       */
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
 * @file test_profile.cpp
 * @brief Tests that TubeScreamer writes its region zones and flushes its solver counters.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/profiled_node.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    /// @brief Holds the warm-up periods of the run below.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of the run below.
    constexpr std::uint32_t K_ACTIVE = 4;

    /// @brief One clip-stage solve per oversampled sample: 256 frames at 4x.
    constexpr double K_SOLVES_PER_BLOCK = 256.0 * 4.0;

    /// @brief Fails for every count that is not the solves one block makes.
    void expect_all_equal_solves_per_block(const std::vector<double>& counts)
    {
        for (const double count : counts)
        {
            EXPECT_DOUBLE_EQ(count, K_SOLVES_PER_BLOCK);
        }
    }
} // namespace

/// @brief Runs the profiled node once for a test and skips the test in a build that records nothing.
class Medium_TubeScreamerProfile : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            run_ = SushiDSP::bench::profile_node<SushiDSP::TubeScreamer>(K_WARMUP, K_ACTIVE);
            if (!run_->start.started())
            {
                GTEST_SKIP() << "this build records nothing";
            }
        }

        std::optional<SushiDSP::profile::ProfiledRun> run_;
};

TEST_F(Medium_TubeScreamerProfile, ProcessWritesItsRegionsAndFlushesItsSolverCountersEachBlock)
{
    namespace regions = SushiDSP::profile::names::tubescreamer;
    const SushiDSP::profile::ProfiledRun& run = *run_;

    EXPECT_EQ(run.record.zone_calls(regions::K_INPUT_ZONE), K_ACTIVE);
    EXPECT_EQ(run.record.zone_calls(regions::K_UPSAMPLE_ZONE), K_ACTIVE);
    EXPECT_EQ(run.record.zone_calls(regions::K_LOOP_ZONE), K_ACTIVE);
    EXPECT_EQ(run.record.zone_calls(regions::K_DOWNSAMPLE_ZONE), K_ACTIVE);
    EXPECT_EQ(run.record.zone_calls(regions::K_OUTPUT_ZONE), K_ACTIVE);
    const SushiDSP::profile::ProfileSubject* const solves = run.record.find("counter:solver.solves");
    ASSERT_NE(solves, nullptr);
    ASSERT_EQ(solves->samples.size(), K_ACTIVE);
    expect_all_equal_solves_per_block(solves->samples);
}
