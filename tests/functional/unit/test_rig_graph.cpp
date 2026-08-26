/**************************************************************************/
/* test_rig_graph.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <memory>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/DenormalGuard.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"

namespace
{
    class GainNode : public SushiDSP::IAudioNode
    {
    public:
        explicit GainNode(float gain) : gain_(gain) {}

        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = context.inputs[0][frame] * gain_;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 1, 1 }; }

        void reset() override {}

        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}

    private:
        float gain_;
    };

    class ConstantSourceNode : public SushiDSP::IAudioNode
    {
    public:
        explicit ConstantSourceNode(float value) : value_(value) {}

        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = value_;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 0, 1 }; }

        void reset() override {}

        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}

    private:
        float value_;
    };
}

TEST(Unit_Dsp, PortLayoutIsPlainOldData)
{
    SushiDSP::PortLayout layout{ 1, 2 };
    EXPECT_EQ(layout.input_channels, 1u);
    EXPECT_EQ(layout.output_channels, 2u);
}

TEST(Unit_Dsp, RigGraphProcessesTwoNodeChainInOrder)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(std::make_unique<ConstantSourceNode>(2.0f));
    auto gain_handle    = graph.add_node(std::make_unique<GainNode>(3.0f));
    graph.connect(source_handle, gain_handle);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };

    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 6.0f);
    }
}

TEST(Unit_Dsp, RigGraphProcessZeroesOutputWhenGraphIsEmpty)
{
    SushiDSP::RigGraph graph(256, 4);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    // Pre-fill with a sentinel to prove process() actively writes zero rather than merely
    // leaving whatever the caller's buffer already held (e.g. SDL's raw stream buffer replaying
    // stale audio from a previous block).
    std::vector<float> output_buffer(256, 12345.0f);
    float* output_channels[1] = { output_buffer.data() };

    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(Unit_Dsp, RigGraphRecompileAfterAddingNodeProducesUpdatedOutput)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto gain_handle_a  = graph.add_node(std::make_unique<GainNode>(2.0f));
    graph.connect(source_handle, gain_handle_a);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    auto gain_handle_b = graph.add_node(std::make_unique<GainNode>(5.0f));
    graph.connect(gain_handle_a, gain_handle_b);
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };
    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 10.0f);
    }
}

TEST(Unit_Dsp, RigGraphFeedsItsOwnInputToARootNodeWithNoUpstreamEdge)
{
    SushiDSP::RigGraph graph(256, 4);
    auto gain_handle = graph.add_node(std::make_unique<GainNode>(4.0f));
    // No connect() call: gain_handle has no upstream edge, so it is a root node.

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> input_buffer(256, 2.0f);
    std::vector<float> output_buffer(256, 0.0f);
    const float* input_channels[1] = { input_buffer.data() };
    float* output_channels[1] = { output_buffer.data() };

    SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 8.0f);
    }
}

TEST(Unit_Dsp, RigGraphRootNodeStillReceivesNullptrWhenGraphHasNoInput)
{
    // Unchanged behavior: a root node must not crash or misbehave when the graph itself has
    // no live input (e.g. the existing SDL2 output-only path) — GainNode would dereference a
    // null context.inputs, so this uses ConstantSourceNode (0 input ports) to prove the
    // *graph* passes nullptr through, without the test node needing to read it.
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(std::make_unique<ConstantSourceNode>(5.0f));

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };

    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 5.0f);
    }
}

TEST(Unit_Dsp, RigGraphNodesReturnsAllLiveHandles)
{
    SushiDSP::RigGraph graph(256, 4);
    auto a = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto b = graph.add_node(std::make_unique<GainNode>(2.0f));
    graph.connect(a, b);

    const std::vector<SushiDSP::NodeHandle> handles = graph.nodes();
    ASSERT_EQ(handles.size(), 2u);
    EXPECT_EQ(handles[0], a);
    EXPECT_EQ(handles[1], b);
    EXPECT_EQ(graph.node(a)->ports().output_channels, 1u);
}

TEST(Unit_Dsp, RigGraphRemoveNodeDropsItFromNodesAndProcess)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(5.0f));
    auto gain   = graph.add_node(std::make_unique<GainNode>(2.0f));
    graph.connect(source, gain);

    graph.remove_node(gain);

    const std::vector<SushiDSP::NodeHandle> handles = graph.nodes();
    ASSERT_EQ(handles.size(), 1u);
    EXPECT_EQ(handles[0], source);
    EXPECT_EQ(graph.node(gain), nullptr);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output(256, 0.0f);
    float* output_ptr[1] = { output.data() };
    SushiDSP::ProcessContext context{ output_ptr, nullptr, 256, 0.0 };
    graph.process(context);

    EXPECT_FLOAT_EQ(output[0], 5.0f);
}

TEST(Unit_Dsp, RigGraphDisconnectRemovesOnlyThatEdge)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(4.0f));
    auto gain_a = graph.add_node(std::make_unique<GainNode>(2.0f));
    auto gain_b = graph.add_node(std::make_unique<GainNode>(3.0f));
    graph.connect(source, gain_a);
    graph.connect(source, gain_b);
    graph.disconnect(source, gain_b);

    // gain_b now has no upstream edge; RigGraph feeds root nodes context.inputs
    // directly, so it must NOT receive source's output. Use a real live-input
    // buffer here (rather than nullptr) since GainNode unconditionally reads
    // context.inputs[0] and a null root input would crash the fixture, not RigGraph.
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output(256, 0.0f);
    float* output_ptr[1] = { output.data() };
    std::vector<float> device_input(256, 1.0f);
    const float* device_input_ptr[1] = { device_input.data() };
    SushiDSP::ProcessContext context{ output_ptr, device_input_ptr, 256, 0.0 };
    graph.process(context);

    // gain_b lost its edge from source, so it becomes a root fed by device_input
    // (1.0) directly; with the source->gain_a edge intact, gain_a still becomes
    // last in topological order (unblocked only once source is processed) and is
    // what gets copied to the graph's output. Its value, 8.0 (source's 4.0 *
    // gain_a's gain of 2.0), only comes out right if BOTH held: disconnect()
    // actually removed source->gain_b (else gain_a would still be sandwiched
    // between source and gain_b in a different topological order, and the last
    // node copied to output would be gain_b at 4.0*3.0=12.0), and the untouched
    // source->gain_a edge survived (else gain_a would be a root fed by
    // device_input directly, giving 1.0*2.0=2.0 instead of 8.0).
    EXPECT_FLOAT_EQ(output[0], 8.0f);
}

namespace
{
    class LatencyNode : public SushiDSP::IAudioNode
    {
    public:
        explicit LatencyNode(std::uint32_t latency) : latency_(latency) {}
        void prepare(const SushiDSP::PrepareInfo&) override {}
        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t i = 0; i < context.frames; ++i) { context.outputs[0][i] = 0.0f; }
        }
        SushiDSP::PortLayout ports() const override { return { 1, 1 }; }
        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}
        void reset() override {}
        std::uint32_t latency_samples() const override { return latency_; }
    private:
        std::uint32_t latency_;
    };

    class SumTwoInputsNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                const float a = context.inputs[0] != nullptr ? context.inputs[0][frame] : 0.0f;
                const float b = context.inputs[1] != nullptr ? context.inputs[1][frame] : 0.0f;
                context.outputs[0][frame] = a + b;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 2, 1 }; }
        void reset() override {}
        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}
    };
}

TEST(Unit_Dsp, RigGraphLatencyIsSumOfChainedNodeLatencies)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto a = graph.add_node(std::make_unique<LatencyNode>(7));
    const auto b = graph.add_node(std::make_unique<LatencyNode>(128));
    graph.connect(a, b);
    graph.compile(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    EXPECT_EQ(graph.latency_samples(), 135u);
}

TEST(Unit_Dsp, RigGraphLatencyIsMaxOverParallelBranchesNotSum)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto short_branch = graph.add_node(std::make_unique<LatencyNode>(10));
    auto long_branch = graph.add_node(std::make_unique<LatencyNode>(200));
    auto sum = graph.add_node(std::make_unique<SumTwoInputsNode>());
    graph.connect(source, short_branch);
    graph.connect(source, long_branch);
    graph.connect(short_branch, sum);
    graph.connect(long_branch, sum);

    graph.compile(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

    // sum itself has 0 latency; the graph's total is max(10, 200) + 0 = 200, not 10 + 200 = 210.
    EXPECT_EQ(graph.latency_samples(), 200u);
}

namespace
{
    class RecordingNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}
        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t i = 0; i < context.frames; ++i) { context.outputs[0][i] = last_value_; }
        }
        SushiDSP::PortLayout ports() const override { return { 0, 1 }; }
        float get_parameter(const char*) const override { return last_value_; }
        void set_parameter(const char*, float) override {}
        void set_parameter(std::uint16_t index, float value) override { last_index_ = index; last_value_ = value; }
        void reset() override {}
        std::uint16_t last_index_ = 0xFFFF;
        float last_value_ = 0.0f;
    };
}

TEST(Unit_Dsp, RigGraphAppliesQueuedParameterChangesBeforeProcessing)
{
    SushiDSP::RigGraph graph(64, 4);
    auto node = std::make_unique<RecordingNode>();
    RecordingNode* raw = node.get();
    const auto handle = graph.add_node(std::move(node));
    graph.compile(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    EXPECT_TRUE(graph.push_parameter_change(SushiDSP::ParameterChange{ handle, 3, 0.25f }));
    std::vector<float> out(64);
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, nullptr, 64, 0.0 };
    graph.process(context);
    EXPECT_EQ(raw->last_index_, 3);
    EXPECT_FLOAT_EQ(out[0], 0.25f);
}

TEST(Unit_Dsp, RigGraphIgnoresParameterChangeForRemovedNode)
{
    SushiDSP::RigGraph graph(64, 4);
    const auto a = graph.add_node(std::make_unique<RecordingNode>());
    graph.remove_node(a);
    graph.compile(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    graph.push_parameter_change(SushiDSP::ParameterChange{ a, 0, 1.0f });
    std::vector<float> out(64);
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, nullptr, 64, 0.0 };
    graph.process(context);   // must not crash
    EXPECT_FLOAT_EQ(out[0], 0.0f);
}

namespace
{
    class DenormalNode : public SushiDSP::IAudioNode
    {
    public:
        void prepare(const SushiDSP::PrepareInfo&) override {}
        void process(SushiDSP::ProcessContext& context) override
        {
            volatile float state = 1e-40f;                 // a denormal; volatile defeats constant folding
            for (std::uint32_t i = 0; i < context.frames; ++i)
            {
                state = state * 0.5f;
                context.outputs[0][i] = state;
            }
        }
        SushiDSP::PortLayout ports() const override { return { 0, 1 }; }
        float get_parameter(const char*) const override { return 0.0f; }
        void set_parameter(const char*, float) override {}
        void reset() override {}
    };
}

TEST(Unit_Dsp, RigGraphProcessFlushesDenormalsForItsNodes)
{
    if (!SushiDSP::DenormalGuard::is_supported()) { GTEST_SKIP(); }
    SushiDSP::RigGraph graph(64, 4);
    graph.add_node(std::make_unique<DenormalNode>());
    graph.compile(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    std::vector<float> out(64, 1.0f);
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, nullptr, 64, 0.0 };
    graph.process(context);
    EXPECT_EQ(out[0], 0.0f);          // 1e-40 * 0.5 is a non-zero denormal without FTZ/DAZ
}

TEST(Unit_Dsp, RigGraphCompileThrowsOnCycle)
{
    SushiDSP::RigGraph graph(256, 4);
    auto a = graph.add_node(std::make_unique<GainNode>(1.0f));
    auto b = graph.add_node(std::make_unique<GainNode>(1.0f));
    graph.connect(a, b);
    graph.connect(b, a);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    EXPECT_THROW(graph.compile(info), std::runtime_error);
}

TEST(Unit_Dsp, RigGraphCompileThrowsOnMultipleSinkNodes)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto sink_a = graph.add_node(std::make_unique<GainNode>(2.0f));
    auto sink_b = graph.add_node(std::make_unique<GainNode>(3.0f));
    graph.connect(source, sink_a);
    graph.connect(source, sink_b);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    EXPECT_THROW(graph.compile(info), std::runtime_error);
}

TEST(Unit_Dsp, RigGraphCompileThrowsWhenNoSinkNodeExists)
{
    // Two live nodes, no edges between them at all: neither node has an outgoing edge, but
    // neither has an incoming edge either (and live_node_count != 1), so neither qualifies
    // as a sink candidate. This is a genuine zero-sink, non-empty, acyclic graph shape, and
    // compile() must reject it rather than leaving sink_handle_ pointing at a stale/default
    // handle for process() to misuse.
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    graph.add_node(std::make_unique<ConstantSourceNode>(2.0f));

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    EXPECT_THROW(graph.compile(info), std::runtime_error);
}

TEST(Unit_Dsp, RigGraphCompileAcceptsSingleSinkGraph)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto sink = graph.add_node(std::make_unique<GainNode>(2.0f));
    graph.connect(source, sink);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    EXPECT_NO_THROW(graph.compile(info));
}

TEST(Unit_Dsp, RigGraphConnectThrowsWhenFanOutExceedsLimit)
{
    SushiDSP::RigGraph graph(256, SushiDSP::RigGraph::kMaxFanInOut + 2);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    std::vector<SushiDSP::NodeHandle> sinks;
    for (std::size_t i = 0; i < SushiDSP::RigGraph::kMaxFanInOut; ++i)
    {
        sinks.push_back(graph.add_node(std::make_unique<GainNode>(1.0f)));
        graph.connect(source, sinks.back());
    }

    auto one_too_many = graph.add_node(std::make_unique<GainNode>(1.0f));
    EXPECT_THROW(graph.connect(source, one_too_many), std::runtime_error);
}

TEST(Unit_Dsp, RigGraphConnectThrowsWhenFanInExceedsLimit)
{
    SushiDSP::RigGraph graph(256, SushiDSP::RigGraph::kMaxFanInOut + 2);
    auto sink = graph.add_node(std::make_unique<GainNode>(1.0f));
    std::vector<SushiDSP::NodeHandle> sources;
    for (std::size_t i = 0; i < SushiDSP::RigGraph::kMaxFanInOut; ++i)
    {
        sources.push_back(graph.add_node(std::make_unique<ConstantSourceNode>(1.0f)));
        graph.connect(sources.back(), sink);
    }

    auto one_too_many = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    EXPECT_THROW(graph.connect(one_too_many, sink), std::runtime_error);
}

TEST(Unit_Dsp, RigGraphProcessGathersAllUpstreamInputsInConnectOrder)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_a = graph.add_node(std::make_unique<ConstantSourceNode>(3.0f));
    auto source_b = graph.add_node(std::make_unique<ConstantSourceNode>(4.0f));
    auto sum = graph.add_node(std::make_unique<SumTwoInputsNode>());
    graph.connect(source_a, sum);
    graph.connect(source_b, sum);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };
    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 7.0f);
    }
}

TEST(Unit_Dsp, RigGraphProcessFansOutOneNodeToTwoDownstreamNodes)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(2.0f));
    auto branch_a = graph.add_node(std::make_unique<GainNode>(3.0f));
    auto branch_b = graph.add_node(std::make_unique<GainNode>(5.0f));
    auto sum = graph.add_node(std::make_unique<SumTwoInputsNode>());
    graph.connect(source, branch_a);
    graph.connect(source, branch_b);
    graph.connect(branch_a, sum);
    graph.connect(branch_b, sum);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };
    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    // source(2.0) -> branch_a(*3=6.0), branch_b(*5=10.0) -> sum(6.0+10.0=16.0)
    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 16.0f);
    }
}

TEST(Unit_Dsp, RigGraphSplitterAndMixerRoundTripThroughTwoEffectBranches)
{
    SushiDSP::RigGraph graph(256, 6);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto splitter = SushiDSP::make_node("sushidsp.splitter", SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    auto splitter_handle = graph.add_node(std::move(splitter));
    auto branch_a = graph.add_node(std::make_unique<GainNode>(2.0f));
    auto branch_b = graph.add_node(std::make_unique<GainNode>(3.0f));
    auto mixer = SushiDSP::make_node("sushidsp.mixer", SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    auto mixer_handle = graph.add_node(std::move(mixer));

    graph.connect(source, splitter_handle);
    graph.connect(splitter_handle, branch_a);
    graph.connect(splitter_handle, branch_b);
    graph.connect(branch_a, mixer_handle);
    graph.connect(branch_b, mixer_handle);

    graph.node(mixer_handle)->set_parameter("gain_0", 0.5f);
    graph.node(mixer_handle)->set_parameter("gain_1", 1.0f);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };
    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    // source(1.0) -split-> branch_a(*2=2.0), branch_b(*3=3.0) -> mixer(2.0*0.5 + 3.0*1.0 = 4.0)
    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 4.0f);
    }
}

TEST(Unit_Dsp, GainNodeTypeIdDefaultsToEmptyString)
{
    GainNode node(1.0f);
    EXPECT_STREQ(node.type_id(), "");
}
