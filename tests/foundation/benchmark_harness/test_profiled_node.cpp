/**************************************************************************/
/* test_profiled_node.cpp                                                 */
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
 * @file test_profiled_node.cpp
 * @brief Tests that profile_node() runs one block per period and hands back the record.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/profiled_node.hpp>
#include <SushiDSP/foundation/core/in_memory_node.hpp>
#include <SushiDSP/foundation/profile/profile.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

namespace
{
    constexpr std::uint32_t K_WARMUP = 2;
    constexpr std::uint32_t K_ACTIVE = 4;

    /// @brief Skips a test when the build carries no profiler, since a run then records nothing.
    /// It gives each test a mono pass-through node that writes its calls into a record.
    class Medium_ProfiledNode : public ::testing::Test
    {
        protected:
            void SetUp() override
            {
                if (!SushiDSP::profile::K_PROFILER_BUILT)
                    GTEST_SKIP() << "this build records nothing";
            }

            SushiDSP::InMemoryNodeRecord record;
            SushiDSP::InMemoryNode node{SushiDSP::InMemoryNodeShape{}, SushiDSP::InMemoryNodeSignal{}, record};
    };
} // namespace

TEST_F(Medium_ProfiledNode, ProfileNodeProcessesOneBlockPerWarmupAndActivePeriod)
{
    const SushiDSP::profile::ProfiledRun run = SushiDSP::bench::profile_node(node, K_WARMUP, K_ACTIVE);

    ASSERT_TRUE(run.start.started());
    EXPECT_EQ(record.process_calls, K_WARMUP + K_ACTIVE);
    EXPECT_EQ(record.last_frames, SushiDSP::bench::K_BLOCK_SIZE);
}

TEST_F(Medium_ProfiledNode, ProfileNodeRecordsTheWarmupAndActivePeriodsOfItsTimeline)
{
    const SushiDSP::profile::ProfiledRun run = SushiDSP::bench::profile_node(node, K_WARMUP, K_ACTIVE);

    ASSERT_TRUE(run.start.started());
    EXPECT_EQ(run.record.warmup_periods(), K_WARMUP);
    EXPECT_EQ(run.record.active_periods(), K_ACTIVE);
    EXPECT_EQ(run.record.summarise(SushiDSP::profile::names::K_BENCH_TIMELINE).periods, K_ACTIVE);
}
