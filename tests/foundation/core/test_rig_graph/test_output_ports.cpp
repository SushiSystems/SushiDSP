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
/** @file test_output_ports.cpp
 *  @brief Tests the named output ports RigGraph routes from a node.
 *  @author Mustafa Garip
 */

#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/in_memory_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>

#include "support/in_memory_nodes.hpp"

namespace
{
    /// @brief Returns a source of two output ports: port 0 mono at @p mono, port 1 stereo at @p left and @p right.
    std::unique_ptr<SushiDSP::InMemoryNode> two_port_source_node(float mono, float left, float right)
    {
        SushiDSP::InMemoryNodeSignal signal;
        signal.levels[0][0] = mono;
        signal.levels[1] = {left, right};
        return TestSupport::multi_port_source_node(2, {1, 2}, signal);
    }

    /// @brief Returns a source of as many mono output ports as RigGraph allows; port p carries the level p + 1.
    std::unique_ptr<SushiDSP::InMemoryNode> every_port_source_node()
    {
        SushiDSP::InMemoryNodeSignal signal;
        std::array<std::uint32_t, SushiDSP::K_IN_MEMORY_NODE_MAX_PORTS> widths{};
        for (std::uint32_t port = 0; port < SushiDSP::RigGraph::K_MAX_OUTPUT_PORTS; ++port)
        {
            widths[port] = 1;
            signal.levels[port][0] = static_cast<float>(port + 1);
        }
        return TestSupport::multi_port_source_node(SushiDSP::RigGraph::K_MAX_OUTPUT_PORTS, widths, signal);
    }
} // namespace


TEST(Small_RigGraph, AudioNodeDefaultsToOneOutputPortOfItsMainWidth)
{
    const auto node = TestSupport::affine_node({1, 2}, 1.0f, 0.0f);
    EXPECT_EQ(node->output_port_count(), 1u);
    EXPECT_EQ(node->output_port_channels(0), 2u);
    EXPECT_EQ(node->output_port_channels(1), 0u);
}

TEST(Small_RigGraph, RigGraphRoutesAnEdgeFromItsMainOutputPort)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto source = graph.add_node(two_port_source_node(0.25f, 0.75f, 0.5f));
    SushiDSP::InMemoryNodeRecord sink_record;
    const auto sink_handle = graph.add_node(TestSupport::recording_sink_node(sink_record));
    graph.connect(source, sink_handle, 0);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> out(256, 0.0f);
    float* outputs[1] = {out.data()};
    SushiDSP::ProcessContext context{outputs, nullptr, 256, 0.0};
    graph.process(context);

    EXPECT_FLOAT_EQ(sink_record.first_input_samples[0], 0.25f);
}

TEST(Small_RigGraph, RigGraphRoutesAnEdgeFromItsOwnPortAndAdaptsItsWidth)
{
    // Port 1 is stereo (0.75, 0.5); a mono sink sums it to half, (0.75 + 0.5) / 2 = 0.625.
    SushiDSP::RigGraph graph(256, 4);
    const auto source = graph.add_node(two_port_source_node(0.25f, 0.75f, 0.5f));
    SushiDSP::InMemoryNodeRecord sink_record;
    const auto sink_handle = graph.add_node(TestSupport::recording_sink_node(sink_record));
    graph.connect(source, sink_handle, 1);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> out(256, 0.0f);
    float* outputs[1] = {out.data()};
    SushiDSP::ProcessContext context{outputs, nullptr, 256, 0.0};
    graph.process(context);

    EXPECT_FLOAT_EQ(sink_record.first_input_samples[0], 0.625f);
}

TEST(Small_RigGraph, RigGraphSetsPortZeroOfTheTableToTheOutputsPointer)
{
    SushiDSP::RigGraph graph(256, 4);
    SushiDSP::InMemoryNodeRecord probe_record;
    graph.add_node(TestSupport::recording_source_node(0.0f, probe_record));
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> out(256, 0.0f);
    float* outputs[1] = {out.data()};
    SushiDSP::ProcessContext context{outputs, nullptr, 256, 0.0};
    graph.process(context);

    EXPECT_TRUE(probe_record.port_table_present);
    EXPECT_TRUE(probe_record.port_zero_is_outputs);
}

TEST(Small_RigGraph, RigGraphCompileRefusesAnEdgeFromAnUndeclaredPort)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto source = graph.add_node(two_port_source_node(0.0f, 0.0f, 0.0f));
    const auto sink = graph.add_node(TestSupport::gain_node(0.0f));
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
    graph.add_node(TestSupport::undeclared_width_source_node(SushiDSP::RigGraph::K_MAX_OUTPUT_PORTS + 1));

    EXPECT_THROW(graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1}), std::runtime_error);
}

TEST(Small_RigGraph, RigGraphCompileRefusesAPortWithNoChannelWidth)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(TestSupport::undeclared_width_source_node(2));

    EXPECT_THROW(graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1}), std::runtime_error);
}

TEST(Small_RigGraph, RigGraphDisconnectRemovesEveryPortOfThePair)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto source = graph.add_node(two_port_source_node(0.0f, 0.0f, 0.0f));
    const auto sink = graph.add_node(TestSupport::gain_node(0.0f));
    graph.connect(source, sink, 0);
    graph.connect(source, sink, 1);
    ASSERT_EQ(graph.upstream_of(sink).size(), 2u);

    graph.disconnect(source, sink);

    EXPECT_TRUE(graph.upstream_of(sink).empty());
}

TEST(Small_RigGraph, RigGraphRootReadsTheHostChannelItIsBoundTo)
{
    // The root copies its live input to its output; the sink then records what the root passed on.
    SushiDSP::RigGraph graph(256, 4);
    const auto root = graph.add_node(TestSupport::gain_node(1.0f));
    SushiDSP::InMemoryNodeRecord sink_record;
    const auto sink_handle = graph.add_node(TestSupport::recording_sink_node(sink_record));
    graph.connect(root, sink_handle);
    graph.bind_input(root, 1);
    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> left(256, 0.1f), right(256, 0.9f);
    const float* host_inputs[2] = {left.data(), right.data()};
    std::vector<float> out(256, 0.0f);
    float* outputs[1] = {out.data()};
    SushiDSP::ProcessContext context{outputs, host_inputs, 256, 0.0};
    graph.process(context);

    EXPECT_FLOAT_EQ(sink_record.first_input_samples[0], 0.9f);
}

TEST(Small_RigGraph, RigGraphCompileRefusesABindingBeyondTheHostChannels)
{
    SushiDSP::RigGraph graph(256, 4);
    const auto root = graph.add_node(TestSupport::gain_node(0.0f));
    graph.bind_input(root, SushiDSP::RigGraph::K_MAX_CHANNELS);

    EXPECT_THROW(graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1}), std::runtime_error);
}

TEST(Small_RigGraph, RigGraphRoutesEveryPortOfANodeThatDeclaresTheLimit)
{
    // The recorders join in two steps, because one node takes at most K_MAX_FAN_IN_OUT inputs.
    const std::uint32_t ports = SushiDSP::RigGraph::K_MAX_OUTPUT_PORTS;
    SushiDSP::RigGraph graph(256, 64);
    const auto source = graph.add_node(every_port_source_node());
    const auto first_join = graph.add_node(TestSupport::gain_node(0.0f));
    const auto last_join = graph.add_node(TestSupport::gain_node(0.0f));

    std::vector<SushiDSP::InMemoryNodeRecord> recorders(ports);
    for (std::uint32_t port = 0; port < ports; ++port)
    {
        const auto handle = graph.add_node(TestSupport::recording_sink_node(recorders[port]));
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
        EXPECT_FLOAT_EQ(recorders[port].first_input_samples[0], static_cast<float>(port + 1)) << "port " << port;
    }
}

TEST(Small_RigGraph, RigGraphCountsFanOutPerOutputPort)
{
    SushiDSP::RigGraph graph(256, 64);
    const auto source = graph.add_node(two_port_source_node(0.0f, 0.0f, 0.0f));
    for (std::size_t edge = 0; edge < SushiDSP::RigGraph::K_MAX_FAN_IN_OUT; ++edge)
    {
        graph.connect(source, graph.add_node(TestSupport::gain_node(0.0f)), 0);
    }

    const auto one_more = graph.add_node(TestSupport::gain_node(0.0f));
    EXPECT_THROW(graph.connect(source, one_more, 0), std::runtime_error);
    EXPECT_NO_THROW(graph.connect(source, one_more, 1));
}
