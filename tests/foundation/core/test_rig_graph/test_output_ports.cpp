/**************************************************************************/
/* test_output_ports.cpp                                                  */
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
/** @file test_output_ports.cpp
 *  @brief Tests the named output ports RigGraph routes from a node.
 *  @author Mustafa Garip
 */

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>

namespace
{
    /// @brief Declares two output ports: port 0 mono, port 1 stereo. Each carries its own level.
    class TwoPortSourceNode : public SushiDSP::AudioNode
    {
        public:
            TwoPortSourceNode(float mono, float left, float right)
                : mono_(mono), left_(left), right_(right)
            {
            }

            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.port_outputs[0][0][frame] = mono_;
                    context.port_outputs[1][0][frame] = left_;
                    context.port_outputs[1][1][frame] = right_;
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {0, 1};
            }

            std::uint32_t output_port_count() const override
            {
                return 2;
            }

            std::uint32_t output_port_channels(std::uint32_t port) const override
            {
                return port == 0 ? 1u : (port == 1 ? 2u : 0u);
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            void set_parameter_for_control_thread(const char*, float) override {}

        private:
            float mono_;
            float left_;
            float right_;
    };

    /// @brief A mono sink that records the first sample of its input and writes silence.
    class RecordingSinkNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                seen_ = context.inputs != nullptr ? context.inputs[0][0] : -1.0f;
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] = 0.0f;
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

            float seen_ = 0.0f;
    };

    /// @brief A single-node graph that records whether RigGraph set port_outputs[0] to outputs.
    class PortTableProbeNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                table_present_ = context.port_outputs != nullptr;
                port_zero_is_outputs_ =
                    context.port_outputs != nullptr && context.port_outputs[0] == context.outputs;
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] = 0.0f;
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

            bool table_present_ = false;
            bool port_zero_is_outputs_ = false;
    };

    /// @brief Declares more output ports than RigGraph allows.
    class TooManyPortsNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] = 0.0f;
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {0, 1};
            }

            std::uint32_t output_port_count() const override
            {
                return SushiDSP::RigGraph::K_MAX_OUTPUT_PORTS + 1;
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            void set_parameter_for_control_thread(const char*, float) override {}
    };

    /// @brief Declares as many mono output ports as RigGraph allows; port p carries the level p + 1.
    class EveryPortSourceNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t port = 0; port < SushiDSP::RigGraph::K_MAX_OUTPUT_PORTS; ++port)
                {
                    for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                    {
                        context.port_outputs[port][0][frame] = static_cast<float>(port + 1);
                    }
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {0, 1};
            }

            std::uint32_t output_port_count() const override
            {
                return SushiDSP::RigGraph::K_MAX_OUTPUT_PORTS;
            }

            std::uint32_t output_port_channels(std::uint32_t port) const override
            {
                return port < SushiDSP::RigGraph::K_MAX_OUTPUT_PORTS ? 1u : 0u;
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            void set_parameter_for_control_thread(const char*, float) override {}
    };

    /// @brief Declares a second port but no width for it, which RigGraph refuses.
    class WidthlessPortNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] = 0.0f;
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {0, 1};
            }

            std::uint32_t output_port_count() const override
            {
                return 2;
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            void set_parameter_for_control_thread(const char*, float) override {}
    };
} // namespace

TEST(Small_RigGraph, AudioNodeDefaultsToOneOutputPortOfItsMainWidth)
{
    class MonoNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}
            void process(SushiDSP::ProcessContext&) override {}
            SushiDSP::PortLayout ports() const override { return {1, 2}; }
            void reset() override {}
            float get_parameter(const char*) const override { return 0.0f; }
            void set_parameter_for_control_thread(const char*, float) override {}
    };

    MonoNode node;
    EXPECT_EQ(node.output_port_count(), 1u);
    EXPECT_EQ(node.output_port_channels(0), 2u);
    EXPECT_EQ(node.output_port_channels(1), 0u);
}

TEST(Small_RigGraph, RigGraphRoutesAnEdgeFromItsMainOutputPort)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto source = graph.add_node(std::make_unique<TwoPortSourceNode>(0.25f, 0.75f, 0.5f));
    auto sink = std::make_unique<RecordingSinkNode>();
    RecordingSinkNode* sink_raw = sink.get();
    const auto sink_handle = graph.add_node(std::move(sink));
    graph.connect(source, sink_handle, 0);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> out(256, 0.0f);
    float* outputs[1] = {out.data()};
    SushiDSP::ProcessContext context{outputs, nullptr, 256, 0.0};
    graph.process(context);

    EXPECT_FLOAT_EQ(sink_raw->seen_, 0.25f);
}

TEST(Small_RigGraph, RigGraphRoutesAnEdgeFromItsOwnPortAndAdaptsItsWidth)
{
    // Port 1 is stereo (0.75, 0.5); a mono sink sums it to half, (0.75 + 0.5) / 2 = 0.625.
    SushiDSP::RigGraph graph(256, 4);
    const auto source = graph.add_node(std::make_unique<TwoPortSourceNode>(0.25f, 0.75f, 0.5f));
    auto sink = std::make_unique<RecordingSinkNode>();
    RecordingSinkNode* sink_raw = sink.get();
    const auto sink_handle = graph.add_node(std::move(sink));
    graph.connect(source, sink_handle, 1);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> out(256, 0.0f);
    float* outputs[1] = {out.data()};
    SushiDSP::ProcessContext context{outputs, nullptr, 256, 0.0};
    graph.process(context);

    EXPECT_FLOAT_EQ(sink_raw->seen_, 0.625f);
}

TEST(Small_RigGraph, RigGraphSetsPortZeroOfTheTableToTheOutputsPointer)
{
    SushiDSP::RigGraph graph(256, 4);
    auto probe = std::make_unique<PortTableProbeNode>();
    PortTableProbeNode* probe_raw = probe.get();
    graph.add_node(std::move(probe));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> out(256, 0.0f);
    float* outputs[1] = {out.data()};
    SushiDSP::ProcessContext context{outputs, nullptr, 256, 0.0};
    graph.process(context);

    EXPECT_TRUE(probe_raw->table_present_);
    EXPECT_TRUE(probe_raw->port_zero_is_outputs_);
}

TEST(Small_RigGraph, RigGraphCompileRefusesAnEdgeFromAnUndeclaredPort)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto source = graph.add_node(std::make_unique<TwoPortSourceNode>(0.0f, 0.0f, 0.0f));
    const auto sink = graph.add_node(std::make_unique<RecordingSinkNode>());
    graph.connect(source, sink, 2);

    try
    {
        graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});
        FAIL() << "compile() accepted an edge from a port the node does not declare";
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        EXPECT_NE(message.find("output port 2"), std::string::npos) << message;
        EXPECT_NE(message.find("node " + std::to_string(source)), std::string::npos) << message;
    }
}

TEST(Small_RigGraph, RigGraphCompileRefusesMoreOutputPortsThanTheLimit)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(std::make_unique<TooManyPortsNode>());

    EXPECT_THROW(graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1}), std::runtime_error);
}

TEST(Small_RigGraph, RigGraphCompileRefusesAPortWithNoChannelWidth)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(std::make_unique<WidthlessPortNode>());

    EXPECT_THROW(graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1}), std::runtime_error);
}

TEST(Small_RigGraph, RigGraphDisconnectRemovesEveryPortOfThePair)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto source = graph.add_node(std::make_unique<TwoPortSourceNode>(0.0f, 0.0f, 0.0f));
    const auto sink = graph.add_node(std::make_unique<RecordingSinkNode>());
    graph.connect(source, sink, 0);
    graph.connect(source, sink, 1);
    ASSERT_EQ(graph.upstream_of(sink).size(), 2u);

    graph.disconnect(source, sink);

    EXPECT_TRUE(graph.upstream_of(sink).empty());
}

TEST(Small_RigGraph, RigGraphRootReadsTheHostChannelItIsBoundTo)
{
    // The root copies its live input to its output; the sink then records what the root passed on.
    class PassThroughNode : public SushiDSP::AudioNode
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
            SushiDSP::PortLayout ports() const override { return {1, 1}; }
            void reset() override {}
            float get_parameter(const char*) const override { return 0.0f; }
            void set_parameter_for_control_thread(const char*, float) override {}
    };

    SushiDSP::RigGraph graph(256, 4);
    const auto root = graph.add_node(std::make_unique<PassThroughNode>());
    auto sink = std::make_unique<RecordingSinkNode>();
    RecordingSinkNode* sink_raw = sink.get();
    const auto sink_handle = graph.add_node(std::move(sink));
    graph.connect(root, sink_handle);
    graph.bind_input(root, 1);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> left(256, 0.1f), right(256, 0.9f);
    const float* host_inputs[2] = {left.data(), right.data()};
    std::vector<float> out(256, 0.0f);
    float* outputs[1] = {out.data()};
    SushiDSP::ProcessContext context{outputs, host_inputs, 256, 0.0};
    graph.process(context);

    EXPECT_FLOAT_EQ(sink_raw->seen_, 0.9f);
}

TEST(Small_RigGraph, RigGraphCompileRefusesABindingBeyondTheHostChannels)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto root = graph.add_node(std::make_unique<RecordingSinkNode>());
    graph.bind_input(root, SushiDSP::RigGraph::K_MAX_CHANNELS);

    EXPECT_THROW(graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1}), std::runtime_error);
}

TEST(Small_RigGraph, RigGraphRoutesEveryPortOfANodeThatDeclaresTheLimit)
{
    // The recorders join in two steps, because one node takes at most K_MAX_FAN_IN_OUT inputs.
    const std::uint32_t ports = SushiDSP::RigGraph::K_MAX_OUTPUT_PORTS;
    SushiDSP::RigGraph graph(256, 64);
    const auto source = graph.add_node(std::make_unique<EveryPortSourceNode>());
    const auto first_join = graph.add_node(std::make_unique<RecordingSinkNode>());
    const auto last_join = graph.add_node(std::make_unique<RecordingSinkNode>());

    std::vector<RecordingSinkNode*> recorders;
    for (std::uint32_t port = 0; port < ports; ++port)
    {
        auto recorder = std::make_unique<RecordingSinkNode>();
        recorders.push_back(recorder.get());
        const auto handle = graph.add_node(std::move(recorder));
        graph.connect(source, handle, port);
        graph.connect(handle, port + 1 < ports ? first_join : last_join);
    }
    graph.connect(first_join, last_join);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> out(256, 0.0f);
    float* outputs[1] = {out.data()};
    SushiDSP::ProcessContext context{outputs, nullptr, 256, 0.0};
    graph.process(context);

    for (std::uint32_t port = 0; port < ports; ++port)
    {
        EXPECT_FLOAT_EQ(recorders[port]->seen_, static_cast<float>(port + 1)) << "port " << port;
    }
}

TEST(Small_RigGraph, RigGraphCountsFanOutPerOutputPort)
{
    SushiDSP::RigGraph graph(256, 64);
    const auto source = graph.add_node(std::make_unique<TwoPortSourceNode>(0.0f, 0.0f, 0.0f));
    for (std::size_t edge = 0; edge < SushiDSP::RigGraph::K_MAX_FAN_IN_OUT; ++edge)
    {
        graph.connect(source, graph.add_node(std::make_unique<RecordingSinkNode>()), 0);
    }

    const auto one_more = graph.add_node(std::make_unique<RecordingSinkNode>());
    EXPECT_THROW(graph.connect(source, one_more, 0), std::runtime_error);
    EXPECT_NO_THROW(graph.connect(source, one_more, 1));
}
