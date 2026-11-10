/**************************************************************************/
/* test_node_benchmark.cpp                                                */
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
 * @file test_node_benchmark.cpp
 * @brief Tests that measure_node() prepares, configures and runs the node it times.
 * @author Mustafa Garip
 */

#include <cstdint>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/node_benchmark.hpp>
#include <SushiDSP/foundation/core/in_memory_node.hpp>

namespace
{
    /// @brief Holds the repetitions measure() runs: the warm-up periods and the active ones.
    constexpr std::uint32_t K_REPETITIONS = SushiDSP::bench::K_WARMUP_PERIODS + SushiDSP::bench::K_ACTIVE_PERIODS;

    /// @brief Gives each test a mono pass-through node that writes its calls into a record.
    class Medium_NodeBenchmark : public ::testing::Test
    {
        protected:
            SushiDSP::InMemoryNodeRecord record;
            SushiDSP::InMemoryNode node{SushiDSP::InMemoryNodeShape{}, SushiDSP::InMemoryNodeSignal{}, record};
    };
} // namespace

TEST_F(Medium_NodeBenchmark, NodeBenchmarkPreparesTheNodeOnceAtTheReferenceBlockBeforeConfiguringIt)
{
    std::uint32_t prepares_seen_by_configure = 0;

    SushiDSP::bench::measure_node(node, "recording",
                                  [&](SushiDSP::InMemoryNode& prepared)
                                  { prepares_seen_by_configure = prepared.record().prepare_calls; });

    EXPECT_EQ(prepares_seen_by_configure, 1u);
    EXPECT_EQ(record.prepare_calls, 1u);
    EXPECT_EQ(record.last_prepare.max_block, SushiDSP::bench::K_BLOCK_SIZE);
}

TEST_F(Medium_NodeBenchmark, NodeBenchmarkProcessesOneBlockPerRepetition)
{
    SushiDSP::bench::measure_node(node, "recording", [](SushiDSP::InMemoryNode&) {});

    EXPECT_EQ(record.process_calls, K_REPETITIONS);
    EXPECT_EQ(record.last_frames, SushiDSP::bench::K_BLOCK_SIZE);
}

TEST_F(Medium_NodeBenchmark, NodeBenchmarkNamesTheRowItReturns)
{
    const SushiDSP::bench::Result result =
        SushiDSP::bench::measure_node(node, "recording", [](SushiDSP::InMemoryNode&) {});

    EXPECT_STREQ(result.name, "recording");
}
