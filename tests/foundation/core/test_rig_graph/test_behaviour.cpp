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

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <numeric>
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

    /// @brief Sentinel the empty-graph test writes into the output, which only process() can clear.
    constexpr float K_SENTINEL = 12345.0f;

    /// @brief The index an InMemoryNodeRecord holds before any audio-thread parameter write.
    constexpr std::uint16_t K_NO_PARAMETER_WRITE = 0xFFFF;

    /// @brief Block size and rate the compiled graphs of these tests use.
    constexpr std::uint32_t K_FRAMES = 256;

    /// @brief Returns a prepare request for @p channels host channels at the block size of these tests.
    SushiDSP::PrepareInfo prepare_info(std::uint32_t channels)
    {
        return SushiDSP::PrepareInfo{48000.0, K_FRAMES, channels};
    }

    /// @brief Processes one block of a compiled graph with no host input and returns channel 0.
    /// @param graph The compiled graph.
    /// @param initial The value the output buffer holds before process() runs.
    std::vector<float> render_mono(SushiDSP::RigGraph& graph, float initial = 0.0f)
    {
        std::vector<float> output_buffer(K_FRAMES, initial);
        float* output_channels[1] = {output_buffer.data()};
        SushiDSP::ProcessContext context{output_channels, nullptr, K_FRAMES, 0.0};
        graph.process(context);
        return output_buffer;
    }

    /// @brief Processes one block of a compiled graph with no host input and returns both channels.
    /// @param graph The graph compiled for two host channels.
    /// @param initial The value both output buffers hold before process() runs.
    std::array<std::vector<float>, 2> render_stereo(SushiDSP::RigGraph& graph, float initial = 0.0f)
    {
        std::array<std::vector<float>, 2> channels{std::vector<float>(K_FRAMES, initial),
                                                   std::vector<float>(K_FRAMES, initial)};
        float* output_channels[2] = {channels[0].data(), channels[1].data()};
        SushiDSP::ProcessContext context{output_channels, nullptr, K_FRAMES, 0.0};
        graph.process(context);
        return channels;
    }

    /// @brief Processes one block of a compiled graph for one or two host channels and returns the channels.
    /// @param graph The compiled graph.
    /// @param channels One or two.
    /// @param initial The value the output buffers hold before process() runs.
    std::array<std::vector<float>, 2> render_stereo_or_mono(SushiDSP::RigGraph& graph, std::uint32_t channels,
                                                            float initial)
    {
        if (channels == 2u)
        {
            return render_stereo(graph, initial);
        }
        return {render_mono(graph, initial), std::vector<float>()};
    }

    /// @brief Returns whether every sample of @p buffer equals @p expected.
    ::testing::AssertionResult every_sample_is(const std::vector<float>& buffer, float expected)
    {
        for (std::size_t index = 0; index < buffer.size(); ++index)
        {
            if (buffer[index] != expected)
            {
                return ::testing::AssertionFailure()
                       << "sample " << index << " is " << buffer[index] << ", expected " << expected;
            }
        }
        return ::testing::AssertionSuccess();
    }

    /// @brief Adds a sink and connects @p limit sources to it, one source node per edge.
    /// @return The handle of the sink.
    SushiDSP::NodeHandle connect_sources_up_to(SushiDSP::RigGraph& graph, std::size_t limit)
    {
        const auto sink = graph.add_node(TestSupport::gain_node(1.0f));
        for (std::size_t index = 0; index < limit; ++index)
        {
            graph.connect(graph.add_node(TestSupport::source_node(1.0f)), sink);
        }
        return sink;
    }

    /// @brief Compiles @p graph and returns the message of the refusal, or an empty string if it compiled.
    std::string compile_refusal_message(SushiDSP::RigGraph& graph, const SushiDSP::PrepareInfo& info)
    {
        try
        {
            graph.compile(info);
        }
        catch (const std::runtime_error& refusal)
        {
            return refusal.what();
        }
        return std::string();
    }

    /// @brief Returns the host input ramp 0, 1, 2, ... of @p length samples.
    std::vector<float> ramp(std::size_t length)
    {
        std::vector<float> samples(length);
        std::iota(samples.begin(), samples.end(), 0.0f);
        return samples;
    }

    /// @brief Returns @p ramp scaled by @p gain.
    std::vector<float> scaled(const std::vector<float>& samples, float gain)
    {
        std::vector<float> result(samples);
        std::transform(result.begin(), result.end(), result.begin(), [gain](float sample) { return sample * gain; });
        return result;
    }

    /// @brief One state of the graph and whether is_empty() must say empty there.
    struct EmptyStateRow
    {
            const char* name;
            bool add_node;
            bool compile;
            bool expected_empty;
    };

    constexpr EmptyStateRow K_EMPTY_STATES[] = {{"BeforeAnyNodeIsAdded", false, false, true},
                                                {"AfterCompilingAGraphWithNoNode", false, true, true},
                                                {"OnceItHoldsACompiledNode", true, true, false}};
} // namespace

/// @brief Skips the denormal test on a target with no flush-to-zero control.
class Small_RigGraphDenormal : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            if (!SushiDSP::DenormalGuard::is_supported())
            {
                GTEST_SKIP() << "no FTZ/DAZ control on this target";
            }
        }
};


TEST(Small_RigGraph, RigGraphProcessesTwoNodeChainInOrder)
{
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    auto source_handle = graph.add_node(TestSupport::source_node(2.0f));
    auto gain_handle = graph.add_node(TestSupport::gain_node(3.0f));
    graph.connect(source_handle, gain_handle);
    graph.compile(prepare_info(1));

    EXPECT_TRUE(every_sample_is(render_mono(graph), 6.0f));
}

class Small_RigGraphEmptyOutput : public ::testing::TestWithParam<std::uint32_t>
{
};

TEST_P(Small_RigGraphEmptyOutput, RigGraphEmptyGraphZeroesEveryOutputChannel)
{
    const std::uint32_t channels = GetParam();
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    graph.compile(prepare_info(channels));

    // The sentinel proves process() writes zero itself. See tests/foundation/core/test_rig_graph/README.md.
    const auto rendered = render_stereo_or_mono(graph, channels, K_SENTINEL);

    EXPECT_TRUE(every_sample_is(rendered[0], 0.0f));
    EXPECT_TRUE(channels < 2u || every_sample_is(rendered[1], 0.0f));
}

INSTANTIATE_TEST_SUITE_P(Widths, Small_RigGraphEmptyOutput, ::testing::Values<std::uint32_t>(1u, 2u),
                         [](const ::testing::TestParamInfo<std::uint32_t>& info)
                         { return std::string(info.param == 1u ? "Mono" : "Stereo"); });

TEST(Small_RigGraph, RigGraphRecompileAfterAddingNodeProducesUpdatedOutput)
{
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    auto source_handle = graph.add_node(TestSupport::source_node(1.0f));
    auto gain_handle_a = graph.add_node(TestSupport::gain_node(2.0f));
    graph.connect(source_handle, gain_handle_a);
    graph.compile(prepare_info(1));

    auto gain_handle_b = graph.add_node(TestSupport::gain_node(5.0f));
    graph.connect(gain_handle_a, gain_handle_b);
    graph.compile(prepare_info(1));

    EXPECT_TRUE(every_sample_is(render_mono(graph), 10.0f));
}

TEST(Small_RigGraph, RigGraphFeedsItsOwnInputToARootNodeWithNoUpstreamEdge)
{
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    // No connect() call: the gain node has no upstream edge, so it is a root node.
    graph.add_node(TestSupport::gain_node(4.0f));
    graph.compile(prepare_info(1));

    std::vector<float> input_buffer(K_FRAMES, 2.0f);
    std::vector<float> output_buffer(K_FRAMES, 0.0f);
    const float* input_channels[1] = {input_buffer.data()};
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, K_FRAMES, 0.0};
    graph.process(context);

    EXPECT_TRUE(every_sample_is(output_buffer, 8.0f));
}

TEST(Small_RigGraph, RigGraphRootNodeStillReceivesNullptrWhenGraphHasNoInput)
{
    // The graph passes nullptr through to a root node. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::InMemoryNodeRecord record;
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    graph.add_node(TestSupport::recording_source_node(5.0f, record));
    graph.compile(prepare_info(1));

    static_cast<void>(render_mono(graph));

    EXPECT_FALSE(record.inputs_present);
}

TEST(Small_RigGraph, RigGraphNodesReturnsAllLiveHandles)
{
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    auto a = graph.add_node(TestSupport::source_node(1.0f));
    auto b = graph.add_node(TestSupport::gain_node(2.0f));
    graph.connect(a, b);

    const std::vector<SushiDSP::NodeHandle> handles = graph.nodes();
    ASSERT_EQ(handles.size(), 2u);
    EXPECT_EQ(handles[0], a);
    EXPECT_EQ(handles[1], b);
}

TEST(Small_RigGraph, RigGraphNodeLooksUpTheNodeOfAHandle)
{
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    auto source = graph.add_node(TestSupport::source_node(1.0f));

    EXPECT_EQ(graph.node(source)->ports().output_channels, 1u);
}

TEST(Small_RigGraph, RigGraphRemoveNodeDropsItFromNodes)
{
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    auto source = graph.add_node(TestSupport::source_node(5.0f));
    auto gain = graph.add_node(TestSupport::gain_node(2.0f));
    graph.connect(source, gain);

    graph.remove_node(gain);

    const std::vector<SushiDSP::NodeHandle> handles = graph.nodes();
    ASSERT_EQ(handles.size(), 1u);
    EXPECT_EQ(handles[0], source);
    EXPECT_EQ(graph.node(gain), nullptr);
}

TEST(Small_RigGraph, RigGraphRemoveNodeDropsItFromProcess)
{
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    auto source = graph.add_node(TestSupport::source_node(5.0f));
    auto gain = graph.add_node(TestSupport::gain_node(2.0f));
    graph.connect(source, gain);
    graph.remove_node(gain);
    graph.compile(prepare_info(1));

    EXPECT_TRUE(every_sample_is(render_mono(graph), 5.0f));
}

TEST(Small_RigGraph, RigGraphDisconnectRemovesOnlyThatEdge)
{
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    auto source = graph.add_node(TestSupport::source_node(4.0f));
    auto gain_a = graph.add_node(TestSupport::gain_node(2.0f));
    auto gain_b = graph.add_node(TestSupport::gain_node(3.0f));
    graph.connect(source, gain_a);
    graph.connect(source, gain_b);
    graph.disconnect(source, gain_b);

    // A root node is fed context.inputs directly. See tests/foundation/core/test_rig_graph/README.md.
    graph.compile(prepare_info(1));

    std::vector<float> output(K_FRAMES, 0.0f);
    float* output_ptr[1] = {output.data()};
    std::vector<float> device_input(K_FRAMES, 1.0f);
    const float* device_input_ptr[1] = {device_input.data()};
    SushiDSP::ProcessContext context{output_ptr, device_input_ptr, K_FRAMES, 0.0};
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
    SushiDSP::InMemoryNodeRecord record;
    SushiDSP::RigGraph graph(64, 4);
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
    SushiDSP::InMemoryNodeRecord survivor;
    SushiDSP::RigGraph graph(64, 4);
    const auto removed = graph.add_node(TestSupport::source_node(0.0f));
    graph.add_node(TestSupport::recording_source_node(0.0f, survivor));
    graph.remove_node(removed);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});
    static_cast<void>(graph.push_parameter_change(
        SushiDSP::ParameterChange{removed, SushiDSP::K_IN_MEMORY_NODE_LEVEL_INDEX, 1.0f}));
    std::vector<float> out(64);
    float* outs[1] = {out.data()};
    SushiDSP::ProcessContext context{outs, nullptr, 64, 0.0};

    graph.process(context);

    EXPECT_EQ(survivor.last_audio_parameter_index, K_NO_PARAMETER_WRITE);
}

TEST_F(Small_RigGraphDenormal, RigGraphProcessFlushesDenormalsForItsNodes)
{
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

TEST(Small_RigGraph, RigGraphConnectThrowsWhenFanInExceedsLimit)
{
    SushiDSP::RigGraph graph(K_FRAMES, SushiDSP::RigGraph::K_MAX_FAN_IN_OUT + 2);
    const auto sink = connect_sources_up_to(graph, SushiDSP::RigGraph::K_MAX_FAN_IN_OUT);

    auto one_too_many = graph.add_node(TestSupport::source_node(1.0f));
    EXPECT_THROW(graph.connect(one_too_many, sink), std::runtime_error);
}

TEST(Small_RigGraph, RigGraphProcessGathersAllUpstreamInputsInConnectOrder)
{
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    auto source_a = graph.add_node(TestSupport::source_node(3.0f));
    auto source_b = graph.add_node(TestSupport::source_node(4.0f));
    auto sum = graph.add_node(TestSupport::sum_node(2));
    graph.connect(source_a, sum);
    graph.connect(source_b, sum);
    graph.compile(prepare_info(1));

    EXPECT_TRUE(every_sample_is(render_mono(graph), 7.0f));
}

TEST(Small_RigGraph, RigGraphProcessFansOutOneNodeToTwoDownstreamNodes)
{
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    auto source = graph.add_node(TestSupport::source_node(2.0f));
    auto branch_a = graph.add_node(TestSupport::gain_node(3.0f));
    auto branch_b = graph.add_node(TestSupport::gain_node(5.0f));
    auto sum = graph.add_node(TestSupport::sum_node(2));
    graph.connect(source, branch_a);
    graph.connect(source, branch_b);
    graph.connect(branch_a, sum);
    graph.connect(branch_b, sum);
    graph.compile(prepare_info(1));

    // source(2.0) -> branch_a(*3=6.0), branch_b(*5=10.0) -> sum(6.0+10.0=16.0)
    EXPECT_TRUE(every_sample_is(render_mono(graph), 16.0f));
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
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    graph.add_node(TestSupport::sum_node(static_cast<std::uint32_t>(SushiDSP::RigGraph::K_MAX_FAN_IN_OUT)));
    graph.compile(prepare_info(1));

    // A realistic host-shaped call: exactly one input channel, as applications/host* all pass.
    std::vector<float> input_buffer(K_FRAMES, 0.25f);
    const float* input_channels[1] = {input_buffer.data()};
    std::vector<float> output_buffer(K_FRAMES, -1.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, K_FRAMES, 0.0};
    graph.process(context);

    // Unused slots are nullptr, never garbage. See tests/foundation/core/test_rig_graph/README.md.
    EXPECT_TRUE(every_sample_is(output_buffer, 0.25f));
}


class Small_RigGraphEmptiness : public ::testing::TestWithParam<std::size_t>
{
};

TEST_P(Small_RigGraphEmptiness, RigGraphIsEmptyOnlyWhileItHoldsNoNode)
{
    const EmptyStateRow& row = K_EMPTY_STATES[GetParam()];
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    if (row.add_node)
    {
        graph.add_node(TestSupport::gain_node(1.0f));
    }
    if (row.compile)
    {
        graph.compile(prepare_info(1));
    }

    EXPECT_EQ(graph.is_empty(), row.expected_empty);
}

INSTANTIATE_TEST_SUITE_P(States, Small_RigGraphEmptiness, ::testing::Values<std::size_t>(0, 1, 2),
                         [](const ::testing::TestParamInfo<std::size_t>& info)
                         { return std::string(K_EMPTY_STATES[info.param].name); });

TEST(Small_RigGraph, RigGraphCopiesAMonoProducerIntoAStereoConsumer)
{
    SushiDSP::InMemoryNodeRecord stereo_record;
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    const auto source = graph.add_node(TestSupport::source_node(0.5f));
    const auto stereo_handle = graph.add_node(TestSupport::stereo_gain_node(1.0f, stereo_record));
    graph.connect(source, stereo_handle);
    graph.compile(prepare_info(2));

    const auto rendered = render_stereo(graph);

    EXPECT_FLOAT_EQ(stereo_record.first_input_samples[0], 0.5f);
    EXPECT_FLOAT_EQ(stereo_record.first_input_samples[1], 0.5f);
    EXPECT_FLOAT_EQ(rendered[0][0], 0.5f);
    EXPECT_FLOAT_EQ(rendered[1][0], 0.5f);
}

TEST(Small_RigGraph, RigGraphCarriesTwoChannelsThroughAStereoChain)
{
    SushiDSP::RigGraph graph(K_FRAMES, 8);
    const auto source = graph.add_node(TestSupport::stereo_source_node(1.0f, -1.0f));
    const auto gain = graph.add_node(TestSupport::stereo_gain_node(2.0f));
    graph.connect(source, gain);
    graph.compile(prepare_info(2));

    const auto rendered = render_stereo(graph);

    EXPECT_TRUE(every_sample_is(rendered[0], 2.0f));
    EXPECT_TRUE(every_sample_is(rendered[1], -2.0f));
}

TEST(Small_RigGraph, RigGraphWritesEveryOutputChannelTheHostAsksFor)
{
    SushiDSP::RigGraph graph(K_FRAMES, 4);
    graph.add_node(TestSupport::source_node(0.25f));
    graph.compile(prepare_info(2));

    const auto rendered = render_stereo(graph, -1.0f);

    EXPECT_TRUE(every_sample_is(rendered[0], 0.25f));
    EXPECT_TRUE(every_sample_is(rendered[1], 0.25f));
}

TEST(Small_RigGraph, RigGraphCompileRefusesAPortWiderThanTheRoute)
{
    // A port's buffers hold K_MAX_CHANNELS channels, so a wider port would write past them.
    SushiDSP::RigGraph graph(K_FRAMES, 8);
    const auto source = graph.add_node(TestSupport::silent_source_node(3));
    const auto stereo = graph.add_node(TestSupport::stereo_gain_node(1.0f));
    graph.connect(source, stereo);

    const std::string message = compile_refusal_message(graph, prepare_info(2));

    EXPECT_NE(message.find("node " + std::to_string(source)), std::string::npos) << message;
    EXPECT_NE(message.find("3 channels wide"), std::string::npos) << message;
    EXPECT_NE(message.find("the limit is 2"), std::string::npos) << message;
}

TEST(Small_RigGraph, RigGraphSumsFanInPerChannel)
{
    // A surplus edge is summed into the slot per channel. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::RigGraph graph(K_FRAMES, 8);
    const auto first_source = graph.add_node(TestSupport::stereo_source_node(1.0f, 2.0f));
    const auto second_source = graph.add_node(TestSupport::stereo_source_node(0.25f, 0.5f));
    const auto sink = graph.add_node(TestSupport::stereo_gain_node(1.0f));
    graph.connect(first_source, sink);
    graph.connect(second_source, sink);
    graph.compile(prepare_info(2));

    const auto rendered = render_stereo(graph);

    EXPECT_FLOAT_EQ(rendered[0][0], 1.25f);
    EXPECT_FLOAT_EQ(rendered[1][0], 2.5f);
}

TEST(Small_RigGraph, RigGraphCountsAParameterChangeForARemovedNode)
{
    SushiDSP::RigGraph::Counters counts;
    SushiDSP::RigGraph graph(64, 4, counts);
    const auto a = graph.add_node(TestSupport::source_node(0.0f));
    graph.remove_node(a);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});
    EXPECT_TRUE(graph.push_parameter_change(SushiDSP::ParameterChange{a, 0, 1.0f}));

    std::vector<float> out(64);
    float* outs[1] = {out.data()};
    SushiDSP::ProcessContext context{outs, nullptr, 64, 0.0};
    graph.process(context);

    EXPECT_EQ(counts.missing_node_change.value(), 1U);
}

TEST(Small_RigGraph, RigGraphProcessesABlockLongerThanItsBlockSizeInPieces)
{
    constexpr std::uint32_t K_LONG_BLOCK = 200;
    SushiDSP::RigGraph::Counters counts;
    SushiDSP::RigGraph graph(64, 4, counts);
    auto source_handle = graph.add_node(TestSupport::source_node(2.0f));
    auto gain_handle = graph.add_node(TestSupport::gain_node(3.0f));
    graph.connect(source_handle, gain_handle);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});

    std::vector<float> output_buffer(K_LONG_BLOCK, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, K_LONG_BLOCK, 0.0};
    graph.process(context);

    EXPECT_TRUE(every_sample_is(output_buffer, 6.0f));
}

TEST(Small_RigGraph, RigGraphCountsABlockLongerThanItsBlockSizeOnceWithItsLength)
{
    constexpr std::uint32_t K_LONG_BLOCK = 200;
    SushiDSP::RigGraph::Counters counts;
    SushiDSP::RigGraph graph(64, 4, counts);
    graph.add_node(TestSupport::source_node(2.0f));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});

    std::vector<float> output_buffer(K_LONG_BLOCK, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, K_LONG_BLOCK, 0.0};
    graph.process(context);

    EXPECT_EQ(counts.oversize_block.value(), 1U);
    EXPECT_EQ(counts.oversize_block.peak(), K_LONG_BLOCK);
}

TEST(Small_RigGraph, RigGraphOffsetsTheHostInputForEachPieceOfALongBlock)
{
    constexpr std::size_t K_LONG_BLOCK = 150;
    constexpr float K_GAIN = 3.0f;
    SushiDSP::RigGraph graph(64, 4);
    graph.add_node(TestSupport::gain_node(K_GAIN));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});

    const std::vector<float> input_buffer = ramp(K_LONG_BLOCK);
    std::vector<float> output_buffer(K_LONG_BLOCK, 0.0f);
    const float* input_channels[1] = {input_buffer.data()};
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, K_LONG_BLOCK, 0.0};
    graph.process(context);

    EXPECT_EQ(output_buffer, scaled(input_buffer, K_GAIN));
}

TEST(Small_RigGraph, RigGraphCountsNothingForABlockInsideItsBlockSize)
{
    SushiDSP::RigGraph::Counters counts;
    SushiDSP::RigGraph graph(64, 4, counts);
    graph.add_node(TestSupport::source_node(1.0f));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});

    std::vector<float> output_buffer(64, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 64, 0.0};
    graph.process(context);

    EXPECT_EQ(counts.oversize_block.value(), 0U);
    EXPECT_EQ(counts.missing_node_change.value(), 0U);
}
