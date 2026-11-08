/**************************************************************************/
/* test_behaviour.cpp                                                     */
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
 * @file test_behaviour.cpp
 * @brief Tests the compilation and processing of RigGraph.
 * @author Mustafa Garip
 */

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/denormal_guard.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/node_descriptor.hpp>
#include <SushiDSP/foundation/core/in_memory_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>

#include "support/in_memory_nodes.hpp"

namespace
{
    /// @brief A denormal float: a source at this level writes a denormal unless the graph flushes it.
    constexpr float K_DENORMAL = 1e-40f;
} // namespace


TEST(Small_RigGraph, PortLayoutIsPlainOldData)
{
    SushiDSP::PortLayout layout{1, 2};
    EXPECT_EQ(layout.input_channels, 1u);
    EXPECT_EQ(layout.output_channels, 2u);
}

TEST(Small_RigGraph, RigGraphProcessesTwoNodeChainInOrder)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(TestSupport::source_node(2.0f));
    auto gain_handle = graph.add_node(TestSupport::gain_node(3.0f));
    graph.connect(source_handle, gain_handle);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};

    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 6.0f);
    }
}

TEST(Small_RigGraph, RigGraphProcessZeroesOutputWhenGraphIsEmpty)
{
    SushiDSP::RigGraph graph(256, 4);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    // The sentinel proves process() writes zero itself. See tests/foundation/core/test_rig_graph/README.md.
    std::vector<float> output_buffer(256, 12345.0f);
    float* output_channels[1] = {output_buffer.data()};

    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(Small_RigGraph, RigGraphRecompileAfterAddingNodeProducesUpdatedOutput)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(TestSupport::source_node(1.0f));
    auto gain_handle_a = graph.add_node(TestSupport::gain_node(2.0f));
    graph.connect(source_handle, gain_handle_a);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    auto gain_handle_b = graph.add_node(TestSupport::gain_node(5.0f));
    graph.connect(gain_handle_a, gain_handle_b);
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 10.0f);
    }
}

TEST(Small_RigGraph, RigGraphFeedsItsOwnInputToARootNodeWithNoUpstreamEdge)
{
    SushiDSP::RigGraph graph(256, 4);
    auto gain_handle = graph.add_node(TestSupport::gain_node(4.0f));
    // No connect() call: gain_handle has no upstream edge, so it is a root node.

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    std::vector<float> input_buffer(256, 2.0f);
    std::vector<float> output_buffer(256, 0.0f);
    const float* input_channels[1] = {input_buffer.data()};
    float* output_channels[1] = {output_buffer.data()};

    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 8.0f);
    }
}

TEST(Small_RigGraph, RigGraphRootNodeStillReceivesNullptrWhenGraphHasNoInput)
{
    // The graph passes nullptr through to a root node. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(TestSupport::source_node(5.0f));

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};

    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 5.0f);
    }
}

TEST(Small_RigGraph, RigGraphNodesReturnsAllLiveHandles)
{
    SushiDSP::RigGraph graph(256, 4);
    auto a = graph.add_node(TestSupport::source_node(1.0f));
    auto b = graph.add_node(TestSupport::gain_node(2.0f));
    graph.connect(a, b);

    const std::vector<SushiDSP::NodeHandle> handles = graph.nodes();
    ASSERT_EQ(handles.size(), 2u);
    EXPECT_EQ(handles[0], a);
    EXPECT_EQ(handles[1], b);
    EXPECT_EQ(graph.node(a)->ports().output_channels, 1u);
}

TEST(Small_RigGraph, RigGraphRemoveNodeDropsItFromNodesAndProcess)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(TestSupport::source_node(5.0f));
    auto gain = graph.add_node(TestSupport::gain_node(2.0f));
    graph.connect(source, gain);

    graph.remove_node(gain);

    const std::vector<SushiDSP::NodeHandle> handles = graph.nodes();
    ASSERT_EQ(handles.size(), 1u);
    EXPECT_EQ(handles[0], source);
    EXPECT_EQ(graph.node(gain), nullptr);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    std::vector<float> output(256, 0.0f);
    float* output_ptr[1] = {output.data()};
    SushiDSP::ProcessContext context{output_ptr, nullptr, 256, 0.0};
    graph.process(context);

    EXPECT_FLOAT_EQ(output[0], 5.0f);
}

TEST(Small_RigGraph, RigGraphDisconnectRemovesOnlyThatEdge)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(TestSupport::source_node(4.0f));
    auto gain_a = graph.add_node(TestSupport::gain_node(2.0f));
    auto gain_b = graph.add_node(TestSupport::gain_node(3.0f));
    graph.connect(source, gain_a);
    graph.connect(source, gain_b);
    graph.disconnect(source, gain_b);

    // A root node is fed context.inputs directly. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    std::vector<float> output(256, 0.0f);
    float* output_ptr[1] = {output.data()};
    std::vector<float> device_input(256, 1.0f);
    const float* device_input_ptr[1] = {device_input.data()};
    SushiDSP::ProcessContext context{output_ptr, device_input_ptr, 256, 0.0};
    graph.process(context);

    // 8.0 comes out only if exactly one edge was removed. See tests/foundation/core/test_rig_graph/README.md.
    EXPECT_FLOAT_EQ(output[0], 8.0f);
}


TEST(Small_RigGraph, RigGraphLatencyIsSumOfChainedNodeLatencies)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto a = graph.add_node(TestSupport::latency_node(7));
    const auto b = graph.add_node(TestSupport::latency_node(128));
    graph.connect(a, b);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});
    EXPECT_EQ(graph.latency_samples(), 135u);
}

TEST(Small_RigGraph, RigGraphLatencyIsMaxOverParallelBranchesNotSum)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(TestSupport::source_node(1.0f));
    auto short_branch = graph.add_node(TestSupport::latency_node(10));
    auto long_branch = graph.add_node(TestSupport::latency_node(200));
    auto sum = graph.add_node(TestSupport::sum_node(2));
    graph.connect(source, short_branch);
    graph.connect(source, long_branch);
    graph.connect(short_branch, sum);
    graph.connect(long_branch, sum);

    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    // sum itself has 0 latency; the graph's total is max(10, 200) + 0 = 200, not 10 + 200 = 210.
    EXPECT_EQ(graph.latency_samples(), 200u);
}


TEST(Small_RigGraph, RigGraphAppliesQueuedParameterChangesBeforeProcessing)
{
    SushiDSP::RigGraph graph(64, 4);
    SushiDSP::InMemoryNodeRecord record;
    const auto handle = graph.add_node(TestSupport::recording_source_node(0.0f, record));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});
    EXPECT_TRUE(graph.push_parameter_change(
        SushiDSP::ParameterChange{handle, SushiDSP::K_IN_MEMORY_NODE_LEVEL_INDEX, 0.25f}));
    std::vector<float> out(64);
    float* outs[1] = {out.data()};
    SushiDSP::ProcessContext context{outs, nullptr, 64, 0.0};
    graph.process(context);
    EXPECT_EQ(record.last_audio_parameter_index, SushiDSP::K_IN_MEMORY_NODE_LEVEL_INDEX);
    EXPECT_FLOAT_EQ(out[0], 0.25f);
}

TEST(Small_RigGraph, RigGraphIgnoresParameterChangeForRemovedNode)
{
    SushiDSP::RigGraph graph(64, 4);
    const auto a = graph.add_node(TestSupport::source_node(0.0f));
    graph.remove_node(a);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});
    graph.push_parameter_change(SushiDSP::ParameterChange{a, 0, 1.0f});
    std::vector<float> out(64);
    float* outs[1] = {out.data()};
    SushiDSP::ProcessContext context{outs, nullptr, 64, 0.0};
    graph.process(context); // must not crash
    EXPECT_FLOAT_EQ(out[0], 0.0f);
}


TEST(Small_RigGraph, RigGraphProcessFlushesDenormalsForItsNodes)
{
    if (!SushiDSP::DenormalGuard::is_supported())
    {
        GTEST_SKIP();
    }
    SushiDSP::RigGraph graph(64, 4);
    graph.add_node(TestSupport::source_node(K_DENORMAL));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});
    std::vector<float> out(64, 1.0f);
    float* outs[1] = {out.data()};
    SushiDSP::ProcessContext context{outs, nullptr, 64, 0.0};
    graph.process(context);
    EXPECT_EQ(out[0], 0.0f); // 0 + 1e-40 is a non-zero denormal without FTZ/DAZ
}

TEST(Small_RigGraph, RigGraphCompileThrowsOnCycle)
{
    SushiDSP::RigGraph graph(256, 4);
    auto a = graph.add_node(TestSupport::gain_node(1.0f));
    auto b = graph.add_node(TestSupport::gain_node(1.0f));
    graph.connect(a, b);
    graph.connect(b, a);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    EXPECT_THROW(graph.compile(info), std::runtime_error);
}

TEST(Small_RigGraph, RigGraphCompileThrowsOnMultipleSinkNodes)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(TestSupport::source_node(1.0f));
    auto sink_a = graph.add_node(TestSupport::gain_node(2.0f));
    auto sink_b = graph.add_node(TestSupport::gain_node(3.0f));
    graph.connect(source, sink_a);
    graph.connect(source, sink_b);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    EXPECT_THROW(graph.compile(info), std::runtime_error);
}

TEST(Small_RigGraph, RigGraphCompileThrowsWhenNoSinkNodeExists)
{
    // Two unconnected nodes form a graph with no sink. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(TestSupport::source_node(1.0f));
    graph.add_node(TestSupport::source_node(2.0f));

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    EXPECT_THROW(graph.compile(info), std::runtime_error);
}

TEST(Small_RigGraph, RigGraphCompileAcceptsSingleSinkGraph)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(TestSupport::source_node(1.0f));
    auto sink = graph.add_node(TestSupport::gain_node(2.0f));
    graph.connect(source, sink);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    EXPECT_NO_THROW(graph.compile(info));
}

TEST(Small_RigGraph, RigGraphConnectThrowsWhenFanOutExceedsLimit)
{
    SushiDSP::RigGraph graph(256, SushiDSP::RigGraph::K_MAX_FAN_IN_OUT + 2);
    auto source = graph.add_node(TestSupport::source_node(1.0f));
    std::vector<SushiDSP::NodeHandle> sinks;
    for (std::size_t i = 0; i < SushiDSP::RigGraph::K_MAX_FAN_IN_OUT; ++i)
    {
        sinks.push_back(graph.add_node(TestSupport::gain_node(1.0f)));
        graph.connect(source, sinks.back());
    }

    auto one_too_many = graph.add_node(TestSupport::gain_node(1.0f));
    EXPECT_THROW(graph.connect(source, one_too_many), std::runtime_error);
}

TEST(Small_RigGraph, RigGraphConnectThrowsWhenFanInExceedsLimit)
{
    SushiDSP::RigGraph graph(256, SushiDSP::RigGraph::K_MAX_FAN_IN_OUT + 2);
    auto sink = graph.add_node(TestSupport::gain_node(1.0f));
    std::vector<SushiDSP::NodeHandle> sources;
    for (std::size_t i = 0; i < SushiDSP::RigGraph::K_MAX_FAN_IN_OUT; ++i)
    {
        sources.push_back(graph.add_node(TestSupport::source_node(1.0f)));
        graph.connect(sources.back(), sink);
    }

    auto one_too_many = graph.add_node(TestSupport::source_node(1.0f));
    EXPECT_THROW(graph.connect(one_too_many, sink), std::runtime_error);
}

TEST(Small_RigGraph, RigGraphProcessGathersAllUpstreamInputsInConnectOrder)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_a = graph.add_node(TestSupport::source_node(3.0f));
    auto source_b = graph.add_node(TestSupport::source_node(4.0f));
    auto sum = graph.add_node(TestSupport::sum_node(2));
    graph.connect(source_a, sum);
    graph.connect(source_b, sum);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 7.0f);
    }
}

TEST(Small_RigGraph, RigGraphProcessFansOutOneNodeToTwoDownstreamNodes)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(TestSupport::source_node(2.0f));
    auto branch_a = graph.add_node(TestSupport::gain_node(3.0f));
    auto branch_b = graph.add_node(TestSupport::gain_node(5.0f));
    auto sum = graph.add_node(TestSupport::sum_node(2));
    graph.connect(source, branch_a);
    graph.connect(source, branch_b);
    graph.connect(branch_a, sum);
    graph.connect(branch_b, sum);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    // source(2.0) -> branch_a(*3=6.0), branch_b(*5=10.0) -> sum(6.0+10.0=16.0)
    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 16.0f);
    }
}

TEST(Small_RigGraph, GainNodeTypeIdDefaultsToEmptyString)
{
    const auto node = TestSupport::gain_node(1.0f);
    EXPECT_STREQ(node->type_id(), "");
}

TEST(Small_RigGraph, RigGraphUpstreamOfListsEdgesInConnectOrder)
{
    SushiDSP::RigGraph graph(256, 4);
    auto a = graph.add_node(TestSupport::source_node(1.0f));
    auto b = graph.add_node(TestSupport::source_node(2.0f));
    auto sink = graph.add_node(TestSupport::sum_node(2));
    graph.connect(a, sink);
    graph.connect(b, sink);

    const std::vector<SushiDSP::NodeHandle> upstream = graph.upstream_of(sink);
    ASSERT_EQ(upstream.size(), 2u);
    EXPECT_EQ(upstream[0], a);
    EXPECT_EQ(upstream[1], b);
    EXPECT_TRUE(graph.upstream_of(a).empty());
}


TEST(Small_RigGraph, RigGraphFeedsRootNodeExternalInputThroughAFullWidthGatherArray)
{
    // Every node receives a K_MAX_FAN_IN_OUT-sized array. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(TestSupport::sum_node(static_cast<std::uint32_t>(SushiDSP::RigGraph::K_MAX_FAN_IN_OUT)));
    graph.compile(info);

    // A realistic host-shaped call: exactly one input channel, as applications/host* all pass.
    std::vector<float> input_buffer(256, 0.25f);
    const float* input_channels[1] = {input_buffer.data()};
    std::vector<float> output_buffer(256, -1.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
    graph.process(context);

    // Unused slots are nullptr, never garbage. See tests/foundation/core/test_rig_graph/README.md.
    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 0.25f);
    }
}

TEST(Small_RigGraph, RigGraphIsEmptyBeforeAnyNodeIsAdded)
{
    SushiDSP::RigGraph graph(256, 4);
    EXPECT_TRUE(graph.is_empty());
}

TEST(Small_RigGraph, RigGraphStaysEmptyAfterCompilingAGraphWithNoNode)
{
    SushiDSP::RigGraph graph(256, 4);
    const SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    EXPECT_TRUE(graph.is_empty());
}

TEST(Small_RigGraph, RigGraphIsNotEmptyOnceItHoldsACompiledNode)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(TestSupport::gain_node(1.0f));
    const SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    EXPECT_FALSE(graph.is_empty());
}


TEST(Small_RigGraph, RigGraphCopiesAMonoProducerIntoAStereoConsumer)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto source = graph.add_node(TestSupport::source_node(0.5f));
    SushiDSP::InMemoryNodeRecord stereo_record;
    const auto stereo_handle = graph.add_node(TestSupport::stereo_gain_node(1.0f, stereo_record));
    graph.connect(source, stereo_handle);

    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 2});

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    EXPECT_FLOAT_EQ(stereo_record.first_input_samples[0], 0.5f);
    EXPECT_FLOAT_EQ(stereo_record.first_input_samples[1], 0.5f);
    EXPECT_FLOAT_EQ(left[0], 0.5f);
    EXPECT_FLOAT_EQ(right[0], 0.5f);
}

TEST(Small_RigGraph, RigGraphSumsAStereoProducerIntoAMonoConsumerAtHalfLevel)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto source = graph.add_node(TestSupport::stereo_source_node(1.0f, 0.0f));
    const auto mono = graph.add_node(TestSupport::gain_node(1.0f));
    graph.connect(source, mono);

    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 0.5f);
    }
}

TEST(Small_RigGraph, RigGraphCarriesTwoChannelsThroughAStereoChain)
{
    SushiDSP::RigGraph graph(256, 8);
    const auto source = graph.add_node(TestSupport::stereo_source_node(1.0f, -1.0f));
    const auto gain = graph.add_node(TestSupport::stereo_gain_node(2.0f));
    graph.connect(source, gain);

    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 2});

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    for (std::uint32_t frame = 0; frame < 256; ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 2.0f);
        EXPECT_FLOAT_EQ(right[frame], -2.0f);
    }
}

TEST(Small_RigGraph, RigGraphWritesEveryOutputChannelTheHostAsksFor)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(TestSupport::source_node(0.25f));

    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 2});

    std::vector<float> left(256, -1.0f);
    std::vector<float> right(256, -1.0f);
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    for (std::uint32_t frame = 0; frame < 256; ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 0.25f);
        EXPECT_FLOAT_EQ(right[frame], 0.25f);
    }
}

TEST(Small_RigGraph, RigGraphCompileRefusesAPortWiderThanTheRoute)
{
    // A port's buffers hold K_MAX_CHANNELS channels, so a wider port would write past them.
    SushiDSP::RigGraph graph(256, 8);
    const auto source = graph.add_node(TestSupport::silent_source_node(3));
    const auto stereo = graph.add_node(TestSupport::stereo_gain_node(1.0f));
    graph.connect(source, stereo);

    try
    {
        graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 2});
        FAIL() << "compile() accepted a port wider than the route";
    }
    catch (const std::runtime_error& refusal)
    {
        const std::string message = refusal.what();
        EXPECT_NE(message.find("node " + std::to_string(source)), std::string::npos) << message;
        EXPECT_NE(message.find("3 channels wide"), std::string::npos) << message;
        EXPECT_NE(message.find("the limit is 2"), std::string::npos) << message;
    }
}

TEST(Small_RigGraph, RigGraphSumsFanInPerChannel)
{
    // A surplus edge is summed into the slot per channel. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::RigGraph graph(256, 8);
    const auto first_source =
        graph.add_node(TestSupport::stereo_source_node(1.0f, 2.0f));
    const auto second_source =
        graph.add_node(TestSupport::stereo_source_node(0.25f, 0.5f));
    const auto sink = graph.add_node(TestSupport::stereo_gain_node(1.0f));
    graph.connect(first_source, sink);
    graph.connect(second_source, sink);

    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 2});

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    EXPECT_FLOAT_EQ(left[0], 1.25f);
    EXPECT_FLOAT_EQ(right[0], 2.5f);
}

TEST(Small_RigGraph, RigGraphEmptyGraphZeroesEveryOutputChannel)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 2});

    std::vector<float> left(256, 12345.0f);
    std::vector<float> right(256, 12345.0f);
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    for (std::uint32_t frame = 0; frame < 256; ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 0.0f);
        EXPECT_FLOAT_EQ(right[frame], 0.0f);
    }
}

TEST(Small_RigGraph, RigGraphCountsAParameterChangeForARemovedNode)
{
    SushiDSP::RigGraph graph(64, 4);
    const auto a = graph.add_node(TestSupport::source_node(0.0f));
    graph.remove_node(a);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});
    EXPECT_TRUE(graph.push_parameter_change(SushiDSP::ParameterChange{a, 0, 1.0f}));
    const std::uint64_t before = SushiDSP::RigGraph::counters().missing_node_change.value();

    std::vector<float> out(64);
    float* outs[1] = {out.data()};
    SushiDSP::ProcessContext context{outs, nullptr, 64, 0.0};
    graph.process(context);

    EXPECT_EQ(SushiDSP::RigGraph::counters().missing_node_change.value(), before + 1U);
}

TEST(Small_RigGraph, RigGraphProcessesABlockLongerThanItsBlockSizeInPiecesAndCountsIt)
{
    SushiDSP::RigGraph graph(64, 4);
    auto source_handle = graph.add_node(TestSupport::source_node(2.0f));
    auto gain_handle = graph.add_node(TestSupport::gain_node(3.0f));
    graph.connect(source_handle, gain_handle);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});

    std::vector<float> output_buffer(200, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 200, 0.0};
    const std::uint64_t before = SushiDSP::RigGraph::counters().oversize_block.value();
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 6.0f);
    }
    EXPECT_EQ(SushiDSP::RigGraph::counters().oversize_block.value(), before + 1U);
    EXPECT_GE(SushiDSP::RigGraph::counters().oversize_block.peak(), 200U);
}

TEST(Small_RigGraph, RigGraphOffsetsTheHostInputForEachPieceOfALongBlock)
{
    SushiDSP::RigGraph graph(64, 4);
    graph.add_node(TestSupport::gain_node(3.0f));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});

    std::vector<float> input_buffer(150);
    for (std::size_t index = 0; index < input_buffer.size(); ++index)
    {
        input_buffer[index] = static_cast<float>(index);
    }
    std::vector<float> output_buffer(150, 0.0f);
    const float* input_channels[1] = {input_buffer.data()};
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, 150, 0.0};
    graph.process(context);

    for (std::size_t index = 0; index < output_buffer.size(); ++index)
    {
        EXPECT_FLOAT_EQ(output_buffer[index], 3.0f * static_cast<float>(index)) << index;
    }
}

TEST(Small_RigGraph, RigGraphCountsNothingForABlockInsideItsBlockSize)
{
    SushiDSP::RigGraph graph(64, 4);
    graph.add_node(TestSupport::source_node(1.0f));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});

    std::vector<float> output_buffer(64, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 64, 0.0};
    const std::uint64_t oversize_before = SushiDSP::RigGraph::counters().oversize_block.value();
    const std::uint64_t missing_before =
        SushiDSP::RigGraph::counters().missing_node_change.value();
    graph.process(context);

    EXPECT_EQ(SushiDSP::RigGraph::counters().oversize_block.value(), oversize_before);
    EXPECT_EQ(SushiDSP::RigGraph::counters().missing_node_change.value(), missing_before);
}
