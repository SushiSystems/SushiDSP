/**************************************************************************/
/* test_rig_preset.cpp                                                    */
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
 * @file test_rig_preset.cpp
 * @brief Tests the JSON serialisation of a rig in RigPreset.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/catalog/node_registry/node_registry.hpp>
#include <SushiDSP/session/rig_preset/node_catalogue.hpp>
#include <SushiDSP/session/rig_preset/rig_preset.hpp>

namespace
{
    const SushiDSP::PrepareInfo kInfo{48000.0, 256, 1};

    /// Frames per block and the node capacity of the graphs the tests build.
    constexpr std::size_t K_BLOCK = 256;
    constexpr std::size_t K_CAPACITY = 8;

    /// A preset whose topology branches at the splitter and joins at the mixer.
    constexpr const char* K_BRANCHING_PRESET = R"({
        "nodes": [
            { "id": 0, "type": "sushidsp.tubescreamer", "params": { "drive": 0.75 }, "inputs": [] },
            { "id": 1, "type": "sushidsp.splitter", "params": {}, "inputs": [0] },
            { "id": 2, "type": "sushidsp.jcm800", "params": {}, "inputs": [1] },
            { "id": 3, "type": "sushidsp.tubescreamer", "params": { "drive": 0.1 }, "inputs": [1] },
            { "id": 4, "type": "sushidsp.mixer", "params": { "gain_0": 0.5, "gain_1": 0.5 }, "inputs": [2, 3] }
        ]
    })";

    /// Adds a node of a registered type to a graph.
    SushiDSP::NodeHandle add(SushiDSP::RigGraph& graph, const char* type)
    {
        return graph.add_node(SushiDSP::make_node(type, kInfo));
    }

    /// A tubescreamer into a jcm800.
    void build_chain(SushiDSP::RigGraph& graph)
    {
        const auto source = add(graph, "sushidsp.tubescreamer");
        const auto amp = add(graph, "sushidsp.jcm800");
        graph.connect(source, amp);
        graph.compile(kInfo);
    }

    /// A tubescreamer, a splitter, an amp and a dry tubescreamer, joined at a mixer with set gains.
    void build_branching_graph(SushiDSP::RigGraph& graph)
    {
        const auto source = add(graph, "sushidsp.tubescreamer");
        graph.node(source)->set_parameter_for_control_thread("drive", 0.9f);
        const auto splitter = add(graph, "sushidsp.splitter");
        const auto amp = add(graph, "sushidsp.jcm800");
        const auto dry = add(graph, "sushidsp.tubescreamer");
        const auto mixer = add(graph, "sushidsp.mixer");
        graph.connect(source, splitter);
        graph.connect(splitter, amp);
        graph.connect(splitter, dry);
        graph.connect(amp, mixer);
        graph.connect(dry, mixer);
        graph.node(mixer)->set_parameter_for_control_thread("gain_0", 0.3f);
        graph.node(mixer)->set_parameter_for_control_thread("gain_1", 0.7f);
        graph.compile(kInfo);
    }

    /// A single tubescreamer.
    void build_single_node(SushiDSP::RigGraph& graph)
    {
        add(graph, "sushidsp.tubescreamer");
        graph.compile(kInfo);
    }

    /// Every node's upstream handles, in handle order.
    std::vector<std::vector<SushiDSP::NodeHandle>> upstream_table(const SushiDSP::RigGraph& graph)
    {
        std::vector<std::vector<SushiDSP::NodeHandle>> table;
        for (const SushiDSP::NodeHandle handle : graph.nodes())
        {
            table.push_back(graph.upstream_of(handle));
        }
        return table;
    }

    /// The count of inputs of each node entry of the given type.
    std::vector<std::size_t> input_counts_of_type(const nlohmann::json& root, const std::string& type)
    {
        std::vector<std::size_t> counts;
        for (const auto& node_json : root.at("nodes"))
        {
            if (node_json.at("type").get<std::string>() == type)
            {
                counts.push_back(node_json.at("inputs").size());
            }
        }
        return counts;
    }

    /// The value of one field of each node entry, as text.
    std::vector<std::string> field_of_each(const nlohmann::json& root, const char* field)
    {
        std::vector<std::string> values;
        for (const auto& node_json : root.at("nodes"))
        {
            values.push_back(node_json.value(field, std::string("<absent>")));
        }
        return values;
    }

    /// One graph builder, for the rows of the round trip.
    struct GraphRow
    {
            const char* name;
            void (*build)(SushiDSP::RigGraph&);
    };

    const GraphRow kGraphRows[] = {
        {"BranchingGraph", build_branching_graph},
        {"SingleNode", build_single_node},
    };

    std::string graph_name(const ::testing::TestParamInfo<GraphRow>& info)
    {
        return info.param.name;
    }

    class Small_RigPresetRoundTrip : public ::testing::TestWithParam<GraphRow>
    {
    };

    /// One preset that must be refused.
    struct MalformedRow
    {
            const char* name;
            const char* json_text;
    };

    // The first row is the shape of a host_gui-format preset, the likeliest wrong file.
    const MalformedRow kMalformedRows[] = {
        {"MissingId", R"({"nodes":[{"type":"sushidsp.tubescreamer","params":{},"inputs":[]}]})"},
        {"MissingParams", R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","inputs":[]}]})"},
        {"MissingInputs", R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{}}]})"},
        {"InputsNotAnArray",
         R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":3}]})"},
        {"DuplicateId", R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
                    {"id":0,"type":"sushidsp.jcm800","params":{},"inputs":[]}]})"},
        // Id 7 is declared by no entry. See tests/session/rig_preset/README.md.
        {"DanglingInput", R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
                    {"id":1,"type":"sushidsp.jcm800","params":{},"inputs":[7]}]})"},
        {"ValidEntryThenABrokenOne", R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
                    {"type":"sushidsp.jcm800","params":{},"inputs":[]}]})"},
        {"ValidEntryThenAnUnknownType", R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
                    {"id":1,"type":"sushidsp.nonexistent","params":{},"inputs":[]}]})"},
    };

    std::string malformed_name(const ::testing::TestParamInfo<MalformedRow>& info)
    {
        return info.param.name;
    }

    class Small_RigPresetMalformed : public ::testing::TestWithParam<MalformedRow>
    {
    };

    /// One chain of registered types, for the rows of the annotate test.
    struct ChainRow
    {
            const char* name;
            std::vector<const char*> types;
    };

    const ChainRow kChainRows[] = {
        {"OneNode", {"sushidsp.tubescreamer"}},
        {"TwoNodes", {"sushidsp.tubescreamer", "sushidsp.jcm800"}},
    };

    std::string chain_name(const ::testing::TestParamInfo<ChainRow>& info)
    {
        return info.param.name;
    }

    class Small_RigPresetAnnotate : public ::testing::TestWithParam<ChainRow>
    {
    };
} // namespace

TEST(Small_RigPreset, SerializeRigToJsonProducesOneEntryPerNodeWithInputs)
{
    SushiDSP::RigGraph graph(K_BLOCK, 4);
    build_chain(graph);

    const nlohmann::json root = nlohmann::json::parse(SushiDSP::serialize_rig_to_json(graph));

    ASSERT_EQ(root.at("nodes").size(), 2u);
    EXPECT_EQ(input_counts_of_type(root, "sushidsp.jcm800"), std::vector<std::size_t>{1u});
}

TEST(Small_RigPreset, DeserializeRigFromJsonRebuildsTheParameters)
{
    SushiDSP::RigGraph graph(K_BLOCK, K_CAPACITY);
    SushiDSP::deserialize_rig_from_json(graph, K_BRANCHING_PRESET, kInfo);
    graph.compile(kInfo);

    EXPECT_EQ(graph.node(0)->get_parameter("drive"), 0.75f);
    EXPECT_EQ(graph.node(3)->get_parameter("drive"), 0.1f);
}

// The mixer's inputs are handles 2 and 3, in that order. See tests/session/rig_preset/README.md.
TEST(Small_RigPreset, DeserializeRigFromJsonRebuildsBranchingTopology)
{
    SushiDSP::RigGraph graph(K_BLOCK, K_CAPACITY);
    SushiDSP::deserialize_rig_from_json(graph, K_BRANCHING_PRESET, kInfo);
    graph.compile(kInfo);

    ASSERT_EQ(graph.nodes().size(), 5u);
    EXPECT_EQ(graph.upstream_of(4), (std::vector<SushiDSP::NodeHandle>{2u, 3u}));
}

// Rebuilt handles line up 1:1 with the original's. See tests/session/rig_preset/README.md.
TEST_P(Small_RigPresetRoundTrip, SerializeThenDeserializeRoundTripsTheTopology)
{
    SushiDSP::RigGraph original(K_BLOCK, K_CAPACITY);
    GetParam().build(original);
    const std::string json_text = SushiDSP::serialize_rig_to_json(original);

    SushiDSP::RigGraph rebuilt(K_BLOCK, K_CAPACITY);
    SushiDSP::deserialize_rig_from_json(rebuilt, json_text, kInfo);
    rebuilt.compile(kInfo);

    EXPECT_EQ(upstream_table(rebuilt), upstream_table(original));
}

TEST_P(Small_RigPresetRoundTrip, SerializeThenDeserializeRoundTripsEveryParameter)
{
    SushiDSP::RigGraph original(K_BLOCK, K_CAPACITY);
    GetParam().build(original);
    const std::string json_text = SushiDSP::serialize_rig_to_json(original);

    SushiDSP::RigGraph rebuilt(K_BLOCK, K_CAPACITY);
    SushiDSP::deserialize_rig_from_json(rebuilt, json_text, kInfo);
    rebuilt.compile(kInfo);

    EXPECT_EQ(SushiDSP::serialize_rig_to_json(rebuilt), json_text);
}

INSTANTIATE_TEST_SUITE_P(Graphs, Small_RigPresetRoundTrip, ::testing::ValuesIn(kGraphRows), graph_name);

TEST(Small_RigPreset, DeserializeRigFromJsonThrowsOnUnknownNodeType)
{
    const std::string json_text = R"({
        "nodes": [
            { "id": 0, "type": "sushidsp.nonexistent", "params": {}, "inputs": [] }
        ]
    })";

    SushiDSP::RigGraph graph(K_BLOCK, 4);
    EXPECT_THROW(SushiDSP::deserialize_rig_from_json(graph, json_text, kInfo), std::runtime_error);
}

// A rejected preset leaves the graph unmodified. See tests/session/rig_preset/README.md.
TEST_P(Small_RigPresetMalformed, DeserializeRigFromJsonRejectsMalformedEntriesWithoutTouchingTheGraph)
{
    SushiDSP::RigGraph graph(K_BLOCK, 4);

    EXPECT_THROW(SushiDSP::deserialize_rig_from_json(graph, GetParam().json_text, kInfo), std::runtime_error);
    EXPECT_TRUE(graph.nodes().empty());
}

INSTANTIATE_TEST_SUITE_P(Presets, Small_RigPresetMalformed, ::testing::ValuesIn(kMalformedRows), malformed_name);

TEST(Small_RigPreset, SerializeRigToJsonThrowsWhenNodeTypeIsNotInTheCatalogue)
{
    SushiDSP::RigGraph graph(K_BLOCK, 4);
    build_single_node(graph);
    const SushiDSP::InMemoryNodeCatalogue empty_catalogue;

    EXPECT_THROW(SushiDSP::serialize_rig_to_json(graph, nullptr, empty_catalogue), std::runtime_error);
}

TEST_P(Small_RigPresetAnnotate, SerializeRigToJsonCallsAnnotateNodeOncePerNode)
{
    SushiDSP::RigGraph graph(K_BLOCK, 4);
    SushiDSP::NodeHandle previous = 0;
    for (std::size_t index = 0; index < GetParam().types.size(); ++index)
    {
        const SushiDSP::NodeHandle handle = add(graph, GetParam().types[index]);
        if (index > 0)
        {
            graph.connect(previous, handle);
        }
        previous = handle;
    }
    graph.compile(kInfo);

    const std::string json_text = SushiDSP::serialize_rig_to_json(
        graph,
        [](SushiDSP::NodeHandle, nlohmann::json& node_json)
        {
            node_json["marker"] = node_json.contains("marker") ? "annotated twice" : "annotated";
        });

    EXPECT_EQ(field_of_each(nlohmann::json::parse(json_text), "marker"),
              std::vector<std::string>(GetParam().types.size(), "annotated"));
}

INSTANTIATE_TEST_SUITE_P(Chains, Small_RigPresetAnnotate, ::testing::ValuesIn(kChainRows), chain_name);

TEST(Small_RigPreset, DeserializeRigFromJsonCallsOnNodeLoadedWithFullEntry)
{
    const std::string json_text = R"({
        "nodes": [
            { "id": 0, "type": "sushidsp.tubescreamer", "params": {}, "inputs": [], "drive_override": 0.25 }
        ]
    })";
    SushiDSP::RigGraph graph(K_BLOCK, 4);

    SushiDSP::deserialize_rig_from_json(
        graph, json_text, kInfo,
        [&graph](SushiDSP::NodeHandle handle, const nlohmann::json& node_json)
        {
            graph.node(handle)->set_parameter_for_control_thread("drive", node_json.at("drive_override").get<float>());
        });
    graph.compile(kInfo);

    EXPECT_EQ(graph.node(0)->get_parameter("drive"), 0.25f);
}
