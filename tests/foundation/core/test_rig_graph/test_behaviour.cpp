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
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/catalog/node_registry/node_registry.hpp>

namespace
{
    class GainNode : public SushiDSP::AudioNode
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

        private:
            float gain_;
    };

    class ConstantSourceNode : public SushiDSP::AudioNode
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

        private:
            float value_;
    };
} // namespace

TEST(RigGraph, PortLayoutIsPlainOldData)
{
    SushiDSP::PortLayout layout{1, 2};
    EXPECT_EQ(layout.input_channels, 1u);
    EXPECT_EQ(layout.output_channels, 2u);
}

TEST(RigGraph, RigGraphProcessesTwoNodeChainInOrder)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(std::make_unique<ConstantSourceNode>(2.0f));
    auto gain_handle = graph.add_node(std::make_unique<GainNode>(3.0f));
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

TEST(RigGraph, RigGraphProcessZeroesOutputWhenGraphIsEmpty)
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

TEST(RigGraph, RigGraphRecompileAfterAddingNodeProducesUpdatedOutput)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto gain_handle_a = graph.add_node(std::make_unique<GainNode>(2.0f));
    graph.connect(source_handle, gain_handle_a);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    auto gain_handle_b = graph.add_node(std::make_unique<GainNode>(5.0f));
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

TEST(RigGraph, RigGraphFeedsItsOwnInputToARootNodeWithNoUpstreamEdge)
{
    SushiDSP::RigGraph graph(256, 4);
    auto gain_handle = graph.add_node(std::make_unique<GainNode>(4.0f));
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

TEST(RigGraph, RigGraphRootNodeStillReceivesNullptrWhenGraphHasNoInput)
{
    // The graph passes nullptr through to a root node. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(std::make_unique<ConstantSourceNode>(5.0f));

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

TEST(RigGraph, RigGraphNodesReturnsAllLiveHandles)
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

TEST(RigGraph, RigGraphRemoveNodeDropsItFromNodesAndProcess)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(5.0f));
    auto gain = graph.add_node(std::make_unique<GainNode>(2.0f));
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

TEST(RigGraph, RigGraphDisconnectRemovesOnlyThatEdge)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(4.0f));
    auto gain_a = graph.add_node(std::make_unique<GainNode>(2.0f));
    auto gain_b = graph.add_node(std::make_unique<GainNode>(3.0f));
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

namespace
{
    class LatencyNode : public SushiDSP::AudioNode
    {
        public:
            explicit LatencyNode(std::uint32_t latency) : latency_(latency) {}
            void prepare(const SushiDSP::PrepareInfo&) override {}
            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t i = 0; i < context.frames; ++i)
                {
                    context.outputs[0][i] = 0.0f;
                }
            }
            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }
            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
            void reset() override {}
            std::uint32_t latency_samples() const override
            {
                return latency_;
            }

        private:
            std::uint32_t latency_;
    };

    class SumTwoInputsNode : public SushiDSP::AudioNode
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

            SushiDSP::PortLayout ports() const override
            {
                return {2, 1};
            }
            void reset() override {}
            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
    };
} // namespace

TEST(RigGraph, RigGraphLatencyIsSumOfChainedNodeLatencies)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto a = graph.add_node(std::make_unique<LatencyNode>(7));
    const auto b = graph.add_node(std::make_unique<LatencyNode>(128));
    graph.connect(a, b);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});
    EXPECT_EQ(graph.latency_samples(), 135u);
}

TEST(RigGraph, RigGraphLatencyIsMaxOverParallelBranchesNotSum)
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

    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    // sum itself has 0 latency; the graph's total is max(10, 200) + 0 = 200, not 10 + 200 = 210.
    EXPECT_EQ(graph.latency_samples(), 200u);
}

namespace
{
    class RecordingNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}
            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t i = 0; i < context.frames; ++i)
                {
                    context.outputs[0][i] = last_value_;
                }
            }
            SushiDSP::PortLayout ports() const override
            {
                return {0, 1};
            }
            float get_parameter(const char*) const override
            {
                return last_value_;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
            void set_parameter_for_audio_thread(std::uint16_t index, float value) override
            {
                last_index_ = index;
                last_value_ = value;
            }
            void reset() override {}
            std::uint16_t last_index_ = 0xFFFF;
            float last_value_ = 0.0f;
    };
} // namespace

TEST(RigGraph, RigGraphAppliesQueuedParameterChangesBeforeProcessing)
{
    SushiDSP::RigGraph graph(64, 4);
    auto node = std::make_unique<RecordingNode>();
    RecordingNode* raw = node.get();
    const auto handle = graph.add_node(std::move(node));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});
    EXPECT_TRUE(graph.push_parameter_change(SushiDSP::ParameterChange{handle, 3, 0.25f}));
    std::vector<float> out(64);
    float* outs[1] = {out.data()};
    SushiDSP::ProcessContext context{outs, nullptr, 64, 0.0};
    graph.process(context);
    EXPECT_EQ(raw->last_index_, 3);
    EXPECT_FLOAT_EQ(out[0], 0.25f);
}

TEST(RigGraph, RigGraphIgnoresParameterChangeForRemovedNode)
{
    SushiDSP::RigGraph graph(64, 4);
    const auto a = graph.add_node(std::make_unique<RecordingNode>());
    graph.remove_node(a);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});
    graph.push_parameter_change(SushiDSP::ParameterChange{a, 0, 1.0f});
    std::vector<float> out(64);
    float* outs[1] = {out.data()};
    SushiDSP::ProcessContext context{outs, nullptr, 64, 0.0};
    graph.process(context); // must not crash
    EXPECT_FLOAT_EQ(out[0], 0.0f);
}

namespace
{
    class DenormalNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}
            void process(SushiDSP::ProcessContext& context) override
            {
                volatile float state = 1e-40f; // a denormal; volatile defeats constant folding
                for (std::uint32_t i = 0; i < context.frames; ++i)
                {
                    state = state * 0.5f;
                    context.outputs[0][i] = state;
                }
            }
            SushiDSP::PortLayout ports() const override
            {
                return {0, 1};
            }
            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
            void reset() override {}
    };
} // namespace

TEST(RigGraph, RigGraphProcessFlushesDenormalsForItsNodes)
{
    if (!SushiDSP::DenormalGuard::is_supported())
    {
        GTEST_SKIP();
    }
    SushiDSP::RigGraph graph(64, 4);
    graph.add_node(std::make_unique<DenormalNode>());
    graph.compile(SushiDSP::PrepareInfo{48000.0, 64, 1});
    std::vector<float> out(64, 1.0f);
    float* outs[1] = {out.data()};
    SushiDSP::ProcessContext context{outs, nullptr, 64, 0.0};
    graph.process(context);
    EXPECT_EQ(out[0], 0.0f); // 1e-40 * 0.5 is a non-zero denormal without FTZ/DAZ
}

TEST(RigGraph, RigGraphCompileThrowsOnCycle)
{
    SushiDSP::RigGraph graph(256, 4);
    auto a = graph.add_node(std::make_unique<GainNode>(1.0f));
    auto b = graph.add_node(std::make_unique<GainNode>(1.0f));
    graph.connect(a, b);
    graph.connect(b, a);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    EXPECT_THROW(graph.compile(info), std::runtime_error);
}

TEST(RigGraph, RigGraphCompileThrowsOnMultipleSinkNodes)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto sink_a = graph.add_node(std::make_unique<GainNode>(2.0f));
    auto sink_b = graph.add_node(std::make_unique<GainNode>(3.0f));
    graph.connect(source, sink_a);
    graph.connect(source, sink_b);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    EXPECT_THROW(graph.compile(info), std::runtime_error);
}

TEST(RigGraph, RigGraphCompileThrowsWhenNoSinkNodeExists)
{
    // Two unconnected nodes form a graph with no sink. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    graph.add_node(std::make_unique<ConstantSourceNode>(2.0f));

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    EXPECT_THROW(graph.compile(info), std::runtime_error);
}

TEST(RigGraph, RigGraphCompileAcceptsSingleSinkGraph)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto sink = graph.add_node(std::make_unique<GainNode>(2.0f));
    graph.connect(source, sink);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    EXPECT_NO_THROW(graph.compile(info));
}

TEST(RigGraph, RigGraphConnectThrowsWhenFanOutExceedsLimit)
{
    SushiDSP::RigGraph graph(256, SushiDSP::RigGraph::K_MAX_FAN_IN_OUT + 2);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    std::vector<SushiDSP::NodeHandle> sinks;
    for (std::size_t i = 0; i < SushiDSP::RigGraph::K_MAX_FAN_IN_OUT; ++i)
    {
        sinks.push_back(graph.add_node(std::make_unique<GainNode>(1.0f)));
        graph.connect(source, sinks.back());
    }

    auto one_too_many = graph.add_node(std::make_unique<GainNode>(1.0f));
    EXPECT_THROW(graph.connect(source, one_too_many), std::runtime_error);
}

TEST(RigGraph, RigGraphConnectThrowsWhenFanInExceedsLimit)
{
    SushiDSP::RigGraph graph(256, SushiDSP::RigGraph::K_MAX_FAN_IN_OUT + 2);
    auto sink = graph.add_node(std::make_unique<GainNode>(1.0f));
    std::vector<SushiDSP::NodeHandle> sources;
    for (std::size_t i = 0; i < SushiDSP::RigGraph::K_MAX_FAN_IN_OUT; ++i)
    {
        sources.push_back(graph.add_node(std::make_unique<ConstantSourceNode>(1.0f)));
        graph.connect(sources.back(), sink);
    }

    auto one_too_many = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    EXPECT_THROW(graph.connect(one_too_many, sink), std::runtime_error);
}

TEST(RigGraph, RigGraphProcessGathersAllUpstreamInputsInConnectOrder)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_a = graph.add_node(std::make_unique<ConstantSourceNode>(3.0f));
    auto source_b = graph.add_node(std::make_unique<ConstantSourceNode>(4.0f));
    auto sum = graph.add_node(std::make_unique<SumTwoInputsNode>());
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

TEST(RigGraph, RigGraphProcessFansOutOneNodeToTwoDownstreamNodes)
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

TEST(RigGraph, RigGraphSplitterAndMixerRoundTripThroughTwoEffectBranches)
{
    SushiDSP::RigGraph graph(256, 6);
    auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto splitter =
        SushiDSP::make_node("sushidsp.splitter", SushiDSP::PrepareInfo{48000.0, 256, 1});
    auto splitter_handle = graph.add_node(std::move(splitter));
    auto branch_a = graph.add_node(std::make_unique<GainNode>(2.0f));
    auto branch_b = graph.add_node(std::make_unique<GainNode>(3.0f));
    auto mixer = SushiDSP::make_node("sushidsp.mixer", SushiDSP::PrepareInfo{48000.0, 256, 1});
    auto mixer_handle = graph.add_node(std::move(mixer));

    graph.connect(source, splitter_handle);
    graph.connect(splitter_handle, branch_a);
    graph.connect(splitter_handle, branch_b);
    graph.connect(branch_a, mixer_handle);
    graph.connect(branch_b, mixer_handle);

    graph.node(mixer_handle)->set_parameter_for_control_thread("gain_0", 0.5f);
    graph.node(mixer_handle)->set_parameter_for_control_thread("gain_1", 1.0f);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    // source(1.0) -split-> branch_a(*2=2.0), branch_b(*3=3.0) -> mixer(2.0*0.5 + 3.0*1.0 = 4.0)
    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 4.0f);
    }
}

TEST(RigGraph, GainNodeTypeIdDefaultsToEmptyString)
{
    GainNode node(1.0f);
    EXPECT_STREQ(node.type_id(), "");
}

TEST(RigGraph, RigGraphUpstreamOfListsEdgesInConnectOrder)
{
    SushiDSP::RigGraph graph(256, 4);
    auto a = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto b = graph.add_node(std::make_unique<ConstantSourceNode>(2.0f));
    auto sink = graph.add_node(std::make_unique<SumTwoInputsNode>());
    graph.connect(a, sink);
    graph.connect(b, sink);

    const std::vector<SushiDSP::NodeHandle> upstream = graph.upstream_of(sink);
    ASSERT_EQ(upstream.size(), 2u);
    EXPECT_EQ(upstream[0], a);
    EXPECT_EQ(upstream[1], b);
    EXPECT_TRUE(graph.upstream_of(a).empty());
}

TEST(RigGraph, RigGraphFeedsRootNodeExternalInputThroughAFullWidthGatherArray)
{
    // Every node receives a K_MAX_FAN_IN_OUT-sized array. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(SushiDSP::make_node("sushidsp.mixer", info));
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

TEST(RigGraph, RigGraphIsEmptyBeforeAnyNodeIsAdded)
{
    SushiDSP::RigGraph graph(256, 4);
    EXPECT_TRUE(graph.is_empty());
}

TEST(RigGraph, RigGraphStaysEmptyAfterCompilingAGraphWithNoNode)
{
    SushiDSP::RigGraph graph(256, 4);
    const SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    EXPECT_TRUE(graph.is_empty());
}

TEST(RigGraph, RigGraphIsNotEmptyOnceItHoldsACompiledNode)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(
        SushiDSP::make_node("sushidsp.tubescreamer", SushiDSP::PrepareInfo{48000.0, 256, 1}));
    const SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    EXPECT_FALSE(graph.is_empty());
}

namespace
{
    /// @brief Emits a fixed value on each of its two output channels.
    class StereoConstantSourceNode : public SushiDSP::AudioNode
    {
        public:
            StereoConstantSourceNode(float left, float right) : left_(left), right_(right) {}

            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] = left_;
                    context.outputs[1][frame] = right_;
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

        private:
            float left_;
            float right_;
    };

    /// @brief Scales both of its input channels and records the first frame of each.
    class StereoGainNode : public SushiDSP::AudioNode
    {
        public:
            explicit StereoGainNode(float gain) : gain_(gain) {}

            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t channel = 0; channel < 2; ++channel)
                {
                    const float* source =
                        context.inputs != nullptr ? context.inputs[channel] : nullptr;
                    seen_[channel] = source != nullptr ? source[0] : 0.0f;
                    for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                    {
                        context.outputs[channel][frame] =
                            (source != nullptr ? source[frame] : 0.0f) * gain_;
                    }
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {2, 2};
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}

            float seen_[2] = {0.0f, 0.0f};

        private:
            float gain_;
    };

    /// @brief Declares three output channels, wider than the two the graph routes per port.
    class ThreeChannelSourceNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t channel = 0; channel < 3; ++channel)
                {
                    for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                    {
                        context.outputs[channel][frame] = 0.0f;
                    }
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {0, 3};
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
    };
} // namespace

TEST(RigGraph, RigGraphCopiesAMonoProducerIntoAStereoConsumer)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto source = graph.add_node(std::make_unique<ConstantSourceNode>(0.5f));
    auto stereo = std::make_unique<StereoGainNode>(1.0f);
    StereoGainNode* stereo_raw = stereo.get();
    const auto stereo_handle = graph.add_node(std::move(stereo));
    graph.connect(source, stereo_handle);

    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 2});

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    EXPECT_FLOAT_EQ(stereo_raw->seen_[0], 0.5f);
    EXPECT_FLOAT_EQ(stereo_raw->seen_[1], 0.5f);
    EXPECT_FLOAT_EQ(left[0], 0.5f);
    EXPECT_FLOAT_EQ(right[0], 0.5f);
}

TEST(RigGraph, RigGraphSumsAStereoProducerIntoAMonoConsumerAtHalfLevel)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto source = graph.add_node(std::make_unique<StereoConstantSourceNode>(1.0f, 0.0f));
    const auto mono = graph.add_node(std::make_unique<GainNode>(1.0f));
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

TEST(RigGraph, RigGraphCarriesTwoChannelsThroughAStereoChain)
{
    SushiDSP::RigGraph graph(256, 8);
    const auto source = graph.add_node(std::make_unique<StereoConstantSourceNode>(1.0f, -1.0f));
    const auto gain = graph.add_node(std::make_unique<StereoGainNode>(2.0f));
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

TEST(RigGraph, RigGraphWritesEveryOutputChannelTheHostAsksFor)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(std::make_unique<ConstantSourceNode>(0.25f));

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

TEST(RigGraph, RigGraphCompileRefusesAPortWiderThanTheRoute)
{
    // A port's buffers hold K_MAX_CHANNELS channels, so a wider port would write past them.
    SushiDSP::RigGraph graph(256, 8);
    const auto source = graph.add_node(std::make_unique<ThreeChannelSourceNode>());
    const auto stereo = graph.add_node(std::make_unique<StereoGainNode>(1.0f));
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

TEST(RigGraph, RigGraphSumsFanInPerChannel)
{
    // A surplus edge is summed into the slot per channel. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::RigGraph graph(256, 8);
    const auto first_source =
        graph.add_node(std::make_unique<StereoConstantSourceNode>(1.0f, 2.0f));
    const auto second_source =
        graph.add_node(std::make_unique<StereoConstantSourceNode>(0.25f, 0.5f));
    const auto sink = graph.add_node(std::make_unique<StereoGainNode>(1.0f));
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

TEST(RigGraph, RigGraphEmptyGraphZeroesEveryOutputChannel)
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
