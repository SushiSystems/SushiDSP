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
 * @file test_rt_safety.cpp
 * @brief Tests that the audio-thread paths of RigGraph perform no heap allocation.
 * @author Mustafa Garip
 */

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/foundation/profile/profile_runtime.hpp>
#include <SushiDSP/foundation/profile/profile_session.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

namespace
{
    class SumNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] = 1.0f;
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {0, 1};
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
    };

    /// @brief A source that names itself, so its zone carries a type id.
    class NamedSourceNode final : public SumNode
    {
        public:
            const char* type_id() const override
            {
                return "test.source";
            }
    };

    /// @brief A pass-through that names itself.
    class NamedPassNode final : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] = context.inputs[0][frame];
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}

            const char* type_id() const override
            {
                return "test.pass";
            }
    };

    class StereoSourceNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] = 1.0f;
                    context.outputs[1][frame] = -1.0f;
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {0, 2};
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
    };

    class MonoPassThroughNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] =
                        context.inputs[0] != nullptr ? context.inputs[0][frame] : 0.0f;
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
    };
} // namespace

TEST(Small_RigGraph, ProcessPerformsNoHeapAllocationAcrossManyBlocks)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(std::make_unique<SumNode>());

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};

    SushiDSP::test::AllocationTrap trap;

    for (int block = 0; block < 7000; ++block)
    {
        for (std::uint16_t index = 0; index < 100; ++index)
        {
            graph.push_parameter_change(SushiDSP::ParameterChange{0, index, 0.5f});
        }

        trap.arm();

        SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
        graph.process(context);

        trap.disarm();
    }

    EXPECT_EQ(trap.count(), 0);
}

TEST(Small_RigGraph, StereoProcessPerformsNoHeapAllocation)
{
    // The graph covers every path of the edge adaptation. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::RigGraph graph(256, 4);
    const auto first_source = graph.add_node(std::make_unique<StereoSourceNode>());
    const auto second_source = graph.add_node(std::make_unique<StereoSourceNode>());
    const auto sink = graph.add_node(std::make_unique<MonoPassThroughNode>());
    graph.connect(first_source, sink);
    graph.connect(second_source, sink);

    SushiDSP::PrepareInfo info{48000.0, 256, 2};
    graph.compile(info);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* output_channels[2] = {left.data(), right.data()};

    SushiDSP::test::AllocationTrap trap;

    for (int block = 0; block < 2000; ++block)
    {
        trap.arm();

        SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
        graph.process(context);

        trap.disarm();
    }

    EXPECT_EQ(trap.count(), 0);
}

TEST(Small_RigGraph, ProcessInPiecesPerformsNoHeapAllocation)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(std::make_unique<SumNode>());
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> output_buffer(1000, 0.0f);
    float* output_channels[1] = {output_buffer.data()};

    const std::uint64_t before = SushiDSP::RigGraph::counters().oversize_block.value();
    SushiDSP::test::AllocationTrap trap;
    for (int block = 0; block < 500; ++block)
    {
        trap.arm();

        SushiDSP::ProcessContext context{output_channels, nullptr, 1000, 0.0};
        graph.process(context);

        trap.disarm();
    }

    EXPECT_EQ(trap.count(), 0);
    EXPECT_EQ(SushiDSP::RigGraph::counters().oversize_block.value(), before + 500U);
}

namespace
{
    /// @brief Holds the periods a profiled graph loop runs with the window open.
    constexpr std::uint32_t K_WARMUP_PERIODS = 3;

    /// @brief Holds the periods a profiled graph loop records as active.
    constexpr std::uint32_t K_ACTIVE_PERIODS = 8;

    /// @brief Builds a source, two pass-through nodes and the graph that joins them.
    void build_three_node_chain(SushiDSP::RigGraph& graph)
    {
        const auto source = graph.add_node(std::make_unique<NamedSourceNode>());
        const auto first = graph.add_node(std::make_unique<NamedPassNode>());
        const auto second = graph.add_node(std::make_unique<NamedPassNode>());
        graph.connect(source, first);
        graph.connect(first, second);
        graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});
    }
} // namespace

TEST(Medium_RigGraphProfile, ProcessWritesOneZonePerNodeNamedByItsTypeIdAndOneGatherPerNode)
{
    if (!SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records nothing";
    SushiDSP::RigGraph graph(256, 8);
    build_three_node_chain(graph);
    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};

    SushiDSP::profile::ProfileSession session;
    ASSERT_TRUE(session
                    .start_window(SushiDSP::profile::names::K_BENCH_TIMELINE, K_WARMUP_PERIODS,
                                  K_ACTIVE_PERIODS)
                    .started());
    for (std::uint32_t period = 0; period < K_WARMUP_PERIODS + K_ACTIVE_PERIODS; ++period)
    {
        session.mark(SushiDSP::profile::names::K_BENCH_TIMELINE, 0U);
        SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
        graph.process(context);
    }
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

TEST(Medium_RigGraphProfile, ProcessWritesNothingWhileTheGateIsClosed)
{
    if (!SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records nothing";
    SushiDSP::RigGraph graph(256, 8);
    build_three_node_chain(graph);
    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};

    SushiDSP::profile::ProfileSession session;
    ASSERT_TRUE(session
                    .start_window(SushiDSP::profile::names::K_BENCH_TIMELINE, 0U,
                                  K_ACTIVE_PERIODS)
                    .started());
    for (std::uint32_t period = 0; period < K_ACTIVE_PERIODS; ++period)
    {
        session.mark(SushiDSP::profile::names::K_BENCH_TIMELINE, 0U);
        SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
        graph.process(context);
    }
    session.mark(SushiDSP::profile::names::K_BENCH_TIMELINE, 0U);
    for (int extra = 0; extra < 5; ++extra)
    {
        SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
        graph.process(context);
    }
    const SushiDSP::profile::ProfileFinish finished = session.finish();

    EXPECT_EQ(finished.record.zone_calls("test.pass"), 2U * K_ACTIVE_PERIODS);
}

TEST(Medium_RigGraphProfile, ProcessPerformsNoHeapAllocationWithASessionOpenAfterWarmUp)
{
    if (!SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records nothing";
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
    for (std::uint32_t period = 0; period < K_WARMUP_PERIODS + K_ACTIVE_PERIODS; ++period)
    {
        session.mark(SushiDSP::profile::names::K_BENCH_TIMELINE, 0U);
        if (period >= K_WARMUP_PERIODS)
            trap.arm();
        SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
        graph.process(context);
        trap.disarm();
    }
    session.mark(SushiDSP::profile::names::K_BENCH_TIMELINE, 0U);
    static_cast<void>(session.finish());

    EXPECT_EQ(trap.count(), 0);
}
