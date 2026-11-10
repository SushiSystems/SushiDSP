/**************************************************************************/
/* test_rt_safety.cpp                                                     */
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
 * @file test_rt_safety.cpp
 * @brief Tests that the audio-thread paths of RigGraph perform no heap allocation.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/foundation/profile/profile_runtime.hpp>
#include <SushiDSP/foundation/profile/profile_session.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include "support/in_memory_nodes.hpp"


namespace
{
    /// @brief Holds the periods a profiled graph loop runs with the window open.
    constexpr std::uint32_t K_WARMUP_PERIODS = 3;

    /// @brief Holds the periods a profiled graph loop records as active.
    constexpr std::uint32_t K_ACTIVE_PERIODS = 8;

    /// @brief Holds the parameter changes pushed before each block of the parameter test.
    constexpr std::uint16_t K_PUSHES_PER_BLOCK = 100;

    /// @brief Processes @p blocks blocks, pushing parameter changes before each and arming @p trap around process().
    void process_with_parameter_pushes(SushiDSP::RigGraph& graph, float* const* output_channels, int blocks,
                                       SushiDSP::test::AllocationTrap& trap)
    {
        for (int block = 0; block < blocks; ++block)
        {
            for (std::uint16_t index = 0; index < K_PUSHES_PER_BLOCK; ++index)
            {
                graph.push_parameter_change(SushiDSP::ParameterChange{0, index, 0.5f});
            }
            trap.arm();
            SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
            graph.process(context);
            trap.disarm();
        }
    }

    /// @brief Processes @p blocks blocks of @p frames frames, arming @p trap around each process().
    void process_trapped(SushiDSP::RigGraph& graph, float* const* output_channels, std::uint32_t frames, int blocks,
                         SushiDSP::test::AllocationTrap& trap)
    {
        for (int block = 0; block < blocks; ++block)
        {
            trap.arm();
            SushiDSP::ProcessContext context{output_channels, nullptr, frames, 0.0};
            graph.process(context);
            trap.disarm();
        }
    }

    /// @brief Processes @p blocks blocks of @p frames frames with no trap.
    void process_plain(SushiDSP::RigGraph& graph, float* const* output_channels, std::uint32_t frames, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
        {
            SushiDSP::ProcessContext context{output_channels, nullptr, frames, 0.0};
            graph.process(context);
        }
    }

    /// @brief Marks one period, then processes one block, for @p periods periods; arms @p trap from period @p arm_from.
    void run_periods(SushiDSP::profile::ProfileSession& session, SushiDSP::RigGraph& graph,
                     float* const* output_channels, std::uint32_t periods, std::uint32_t arm_from,
                     SushiDSP::test::AllocationTrap& trap)
    {
        for (std::uint32_t period = 0; period < periods; ++period)
        {
            session.mark(SushiDSP::profile::names::K_BENCH_TIMELINE, 0U);
            if (period >= arm_from)
            {
                trap.arm();
            }
            SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
            graph.process(context);
            trap.disarm();
        }
    }

    /// @brief Builds a source, two pass-through nodes and the graph that joins them.
    void build_three_node_chain(SushiDSP::RigGraph& graph)
    {
        const auto source = graph.add_node(TestSupport::named_node({0, 1}, "test.source"));
        const auto first = graph.add_node(TestSupport::named_node({1, 1}, "test.pass"));
        const auto second = graph.add_node(TestSupport::named_node({1, 1}, "test.pass"));
        graph.connect(source, first);
        graph.connect(first, second);
        graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});
    }
} // namespace

/// @brief Skips every profile test of the suite in a build that records nothing.
class Medium_RigGraphProfile : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            if (!SushiDSP::profile::K_PROFILER_BUILT)
            {
                GTEST_SKIP() << "this build records nothing";
            }
        }
};

TEST(Medium_RigGraph, ProcessPerformsNoHeapAllocationAcrossManyBlocks)
{
    constexpr int K_BLOCKS = 7000;
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(TestSupport::source_node(1.0f));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});
    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::test::AllocationTrap trap;

    process_with_parameter_pushes(graph, output_channels, K_BLOCKS, trap);

    EXPECT_EQ(trap.count(), 0);
}

TEST(Medium_RigGraph, StereoProcessPerformsNoHeapAllocation)
{
    constexpr int K_BLOCKS = 2000;
    // The graph covers every path of the edge adaptation. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::RigGraph graph(256, 4);
    const auto first_source = graph.add_node(TestSupport::stereo_source_node(1.0f, -1.0f));
    const auto second_source = graph.add_node(TestSupport::stereo_source_node(1.0f, -1.0f));
    const auto sink = graph.add_node(TestSupport::gain_node(1.0f));
    graph.connect(first_source, sink);
    graph.connect(second_source, sink);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 2});
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::test::AllocationTrap trap;

    process_trapped(graph, output_channels, 256, K_BLOCKS, trap);

    EXPECT_EQ(trap.count(), 0);
}

TEST(Medium_RigGraph, ProcessInPiecesPerformsNoHeapAllocation)
{
    constexpr int K_BLOCKS = 500;
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(TestSupport::source_node(1.0f));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});
    std::vector<float> output_buffer(1000, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::test::AllocationTrap trap;

    process_trapped(graph, output_channels, 1000, K_BLOCKS, trap);

    EXPECT_EQ(trap.count(), 0);
}

TEST(Small_RigGraph, ProcessInPiecesCountsOneOversizeBlockPerCall)
{
    constexpr int K_BLOCKS = 500;
    SushiDSP::RigGraph::Counters counts;
    SushiDSP::RigGraph graph(256, 4, counts);
    graph.add_node(TestSupport::source_node(1.0f));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});
    std::vector<float> output_buffer(1000, 0.0f);
    float* output_channels[1] = {output_buffer.data()};

    process_plain(graph, output_channels, 1000, K_BLOCKS);

    EXPECT_EQ(counts.oversize_block.value(), static_cast<std::uint64_t>(K_BLOCKS));
}

TEST_F(Medium_RigGraphProfile, ProcessWritesOneZonePerNodeNamedByItsTypeIdAndOneGatherPerNode)
{
    SushiDSP::RigGraph graph(256, 8);
    build_three_node_chain(graph);
    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::test::AllocationTrap unarmed;

    SushiDSP::profile::ProfileSession session;
    ASSERT_TRUE(session
                    .start_window(SushiDSP::profile::names::K_BENCH_TIMELINE, K_WARMUP_PERIODS,
                                  K_ACTIVE_PERIODS)
                    .started());
    run_periods(session, graph, output_channels, K_WARMUP_PERIODS + K_ACTIVE_PERIODS,
                K_WARMUP_PERIODS + K_ACTIVE_PERIODS, unarmed);
    session.mark(SushiDSP::profile::names::K_BENCH_TIMELINE, 0U);
    const SushiDSP::profile::ProfileFinish finished = session.finish();

    EXPECT_EQ(finished.record.active_periods(), K_ACTIVE_PERIODS);
    EXPECT_EQ(finished.record.zone_calls("test.source"), K_ACTIVE_PERIODS);
    EXPECT_EQ(finished.record.zone_calls("test.pass"), 2U * K_ACTIVE_PERIODS);
    EXPECT_EQ(finished.record.zone_calls(SushiDSP::profile::names::K_GATHER_ZONE),
              3U * K_ACTIVE_PERIODS);
    EXPECT_EQ(finished.record.zone_calls(SushiDSP::profile::names::K_GRAPH_PROCESS_ZONE),
              K_ACTIVE_PERIODS);
}

TEST_F(Medium_RigGraphProfile, ProcessWritesNothingWhileTheGateIsClosed)
{
    constexpr int K_EXTRA_BLOCKS = 5;
    SushiDSP::RigGraph graph(256, 8);
    build_three_node_chain(graph);
    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::test::AllocationTrap unarmed;

    SushiDSP::profile::ProfileSession session;
    ASSERT_TRUE(session
                    .start_window(SushiDSP::profile::names::K_BENCH_TIMELINE, 0U,
                                  K_ACTIVE_PERIODS)
                    .started());
    run_periods(session, graph, output_channels, K_ACTIVE_PERIODS, K_ACTIVE_PERIODS, unarmed);
    session.mark(SushiDSP::profile::names::K_BENCH_TIMELINE, 0U);
    process_plain(graph, output_channels, 256, K_EXTRA_BLOCKS);
    const SushiDSP::profile::ProfileFinish finished = session.finish();

    EXPECT_EQ(finished.record.zone_calls("test.pass"), 2U * K_ACTIVE_PERIODS);
}

TEST_F(Medium_RigGraphProfile, ProcessPerformsNoHeapAllocationWithASessionOpenAfterWarmUp)
{
    SushiDSP::RigGraph graph(256, 8);
    build_three_node_chain(graph);
    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};

    SushiDSP::profile::ProfileSession session;
    ASSERT_TRUE(session
                    .start_window(SushiDSP::profile::names::K_BENCH_TIMELINE, K_WARMUP_PERIODS,
                                  K_ACTIVE_PERIODS)
                    .started());
    SushiDSP::profile::register_this_thread();
    SushiDSP::test::AllocationTrap trap;
    run_periods(session, graph, output_channels, K_WARMUP_PERIODS + K_ACTIVE_PERIODS, K_WARMUP_PERIODS, trap);
    session.mark(SushiDSP::profile::names::K_BENCH_TIMELINE, 0U);
    static_cast<void>(session.finish());

    EXPECT_EQ(trap.count(), 0);
}
