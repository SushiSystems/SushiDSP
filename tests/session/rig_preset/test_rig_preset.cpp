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
 * @file test_rig_preset.cpp
 * @brief Tests the JSON serialisation of a rig in RigPreset.
 * @author Mustafa Garip
 */

#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/catalog/node_registry/node_registry.hpp>
#include <SushiDSP/session/rig_preset/rig_preset.hpp>

namespace
{
    /// @brief A minimal AudioNode with no node_registry() entry, so its type_id() can never
    /// resolve to a NodeDescriptor — used to exercise serialize_rig_to_json()'s throw path.
    struct UnregisteredNode : SushiDSP::AudioNode
    {
            void prepare(const SushiDSP::PrepareInfo&) override {}
            void process(SushiDSP::ProcessContext&) override {}
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
            const char* type_id() const override
            {
                return "not.registered";
            }
    };
} // namespace

TEST(Unit_Dsp, SerializeRigToJsonProducesOneEntryPerNodeWithInputs)
{
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    SushiDSP::RigGraph graph(256, 4);
    auto ts = graph.add_node(SushiDSP::make_node("sushidsp.tubescreamer", info));
    auto amp = graph.add_node(SushiDSP::make_node("sushidsp.jcm800", info));
    graph.connect(ts, amp);
    graph.compile(info);

    const std::string json_text = SushiDSP::serialize_rig_to_json(graph);
    const nlohmann::json root = nlohmann::json::parse(json_text);

    ASSERT_EQ(root.at("nodes").size(), 2u);
    bool found_amp_with_one_input = false;
    for (const auto& node_json : root.at("nodes"))
    {
        if (node_json.at("type").get<std::string>() == "sushidsp.jcm800")
        {
            ASSERT_EQ(node_json.at("inputs").size(), 1u);
            found_amp_with_one_input = true;
        }
    }
    EXPECT_TRUE(found_amp_with_one_input);
}

TEST(Unit_Dsp, DeserializeRigFromJsonRebuildsBranchingTopologyAndParameters)
{
    SushiDSP::PrepareInfo info{48000.0, 256, 1};

    const std::string json_text = R"({
        "nodes": [
            { "id": 0, "type": "sushidsp.tubescreamer", "params": { "drive": 0.75 }, "inputs": [] },
            { "id": 1, "type": "sushidsp.splitter", "params": {}, "inputs": [0] },
            { "id": 2, "type": "sushidsp.jcm800", "params": {}, "inputs": [1] },
            { "id": 3, "type": "sushidsp.tubescreamer", "params": { "drive": 0.1 }, "inputs": [1] },
            { "id": 4, "type": "sushidsp.mixer", "params": { "gain_0": 0.5, "gain_1": 0.5 }, "inputs": [2, 3] }
        ]
    })";

    SushiDSP::RigGraph graph(256, 8);
    SushiDSP::deserialize_rig_from_json(graph, json_text, info);
    graph.compile(info);

    const auto handles = graph.nodes();
    ASSERT_EQ(handles.size(), 5u);

    bool found_drive_075 = false;
    bool found_drive_010 = false;
    for (SushiDSP::NodeHandle handle : handles)
    {
        SushiDSP::AudioNode* node = graph.node(handle);
        const float drive = node->get_parameter("drive");
        if (std::abs(drive - 0.75f) < 1e-6f)
        {
            found_drive_075 = true;
        }
        if (std::abs(drive - 0.1f) < 1e-6f)
        {
            found_drive_010 = true;
        }
    }
    EXPECT_TRUE(found_drive_075);
    EXPECT_TRUE(found_drive_010);

    // The mixer's inputs are handles 2 and 3, in that order. See tests/session/rig_preset/README.md.
    const std::vector<SushiDSP::NodeHandle> mixer_inputs = graph.upstream_of(4);
    ASSERT_EQ(mixer_inputs.size(), 2u);
    EXPECT_EQ(mixer_inputs[0], 2u);
    EXPECT_EQ(mixer_inputs[1], 3u);
}

TEST(Unit_Dsp, SerializeThenDeserializeRoundTripsABranchingGraph)
{
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    SushiDSP::RigGraph original(256, 6);
    auto source_ts = original.add_node(SushiDSP::make_node("sushidsp.tubescreamer", info));
    original.node(source_ts)->set_parameter_for_control_thread("drive", 0.9f);
    auto splitter = original.add_node(SushiDSP::make_node("sushidsp.splitter", info));
    auto amp = original.add_node(SushiDSP::make_node("sushidsp.jcm800", info));
    auto dry = original.add_node(SushiDSP::make_node("sushidsp.tubescreamer", info));
    auto mixer = original.add_node(SushiDSP::make_node("sushidsp.mixer", info));
    original.connect(source_ts, splitter);
    original.connect(splitter, amp);
    original.connect(splitter, dry);
    original.connect(amp, mixer);
    original.connect(dry, mixer);
    original.node(mixer)->set_parameter_for_control_thread("gain_0", 0.3f);
    original.node(mixer)->set_parameter_for_control_thread("gain_1", 0.7f);
    original.compile(info);

    const std::string json_text = SushiDSP::serialize_rig_to_json(original);

    SushiDSP::RigGraph rebuilt(256, 6);
    SushiDSP::deserialize_rig_from_json(rebuilt, json_text, info);
    rebuilt.compile(info);

    ASSERT_EQ(rebuilt.nodes().size(), original.nodes().size());
    bool found_mixer_with_gains = false;
    for (SushiDSP::NodeHandle handle : rebuilt.nodes())
    {
        SushiDSP::AudioNode* node = rebuilt.node(handle);
        if (std::abs(node->get_parameter("gain_0") - 0.3f) < 1e-6f &&
            std::abs(node->get_parameter("gain_1") - 0.7f) < 1e-6f)
        {
            found_mixer_with_gains = true;
        }
    }
    EXPECT_TRUE(found_mixer_with_gains);

    // Rebuilt handles line up 1:1 with the original's. See tests/session/rig_preset/README.md.
    for (SushiDSP::NodeHandle handle : original.nodes())
    {
        EXPECT_EQ(rebuilt.upstream_of(handle), original.upstream_of(handle))
            << "upstream_of mismatch for handle " << handle;
    }
}

TEST(Unit_Dsp, DeserializeRigFromJsonThrowsOnUnknownNodeType)
{
    const std::string json_text = R"({
        "nodes": [
            { "id": 0, "type": "sushidsp.nonexistent", "params": {}, "inputs": [] }
        ]
    })";

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    SushiDSP::RigGraph graph(256, 4);
    EXPECT_THROW(SushiDSP::deserialize_rig_from_json(graph, json_text, info), std::runtime_error);
}

TEST(Unit_Dsp, DeserializeRigFromJsonRejectsMalformedEntriesWithoutTouchingTheGraph)
{
    SushiDSP::PrepareInfo info{48000.0, 256, 1};

    // A rejected preset leaves the graph unmodified. See tests/session/rig_preset/README.md.
    const std::vector<std::string> malformed = {
        // Missing "id" — the shape of a host_gui-format preset, the likeliest wrong file.
        R"({"nodes":[{"type":"sushidsp.tubescreamer","params":{},"inputs":[]}]})",
        // Missing "params".
        R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","inputs":[]}]})",
        // Missing "inputs".
        R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{}}]})",
        // "inputs" is not an array.
        R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":3}]})",
        // Duplicate id.
        R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
                    {"id":0,"type":"sushidsp.jcm800","params":{},"inputs":[]}]})",
        // Id 7 is declared by no entry. See tests/session/rig_preset/README.md.
        R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
                    {"id":1,"type":"sushidsp.jcm800","params":{},"inputs":[7]}]})",
        // A valid first entry followed by a broken one: the first must NOT be added.
        R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
                    {"type":"sushidsp.jcm800","params":{},"inputs":[]}]})",
    };

    for (const std::string& json_text : malformed)
    {
        SushiDSP::RigGraph graph(256, 4);
        EXPECT_THROW(SushiDSP::deserialize_rig_from_json(graph, json_text, info),
                     std::runtime_error)
            << "should have thrown std::runtime_error for: " << json_text;
        EXPECT_TRUE(graph.nodes().empty()) << "graph must be left unmodified for: " << json_text;
    }
}

TEST(Unit_Dsp, SerializeRigToJsonThrowsWhenNodeTypeIsNotRegistered)
{
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(std::make_unique<UnregisteredNode>());
    graph.compile(info);

    EXPECT_THROW(SushiDSP::serialize_rig_to_json(graph), std::runtime_error);
}

TEST(Unit_Dsp, SerializeRigToJsonCallsAnnotateNodeOncePerNode)
{
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    SushiDSP::RigGraph graph(256, 4);
    auto ts = graph.add_node(SushiDSP::make_node("sushidsp.tubescreamer", info));
    auto amp = graph.add_node(SushiDSP::make_node("sushidsp.jcm800", info));
    graph.connect(ts, amp);
    graph.compile(info);

    int call_count = 0;
    const std::string json_text = SushiDSP::serialize_rig_to_json(
        graph,
        [&call_count](SushiDSP::NodeHandle, nlohmann::json& node_json)
        {
            ++call_count;
            node_json["marker"] = "annotated";
        });

    EXPECT_EQ(call_count, 2);
    const nlohmann::json root = nlohmann::json::parse(json_text);
    for (const auto& node_json : root.at("nodes"))
    {
        EXPECT_EQ(node_json.at("marker").get<std::string>(), "annotated");
    }
}

TEST(Unit_Dsp, DeserializeRigFromJsonCallsOnNodeLoadedWithFullEntry)
{
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    const std::string json_text = R"({
        "nodes": [
            { "id": 0, "type": "sushidsp.tubescreamer", "params": {}, "inputs": [], "marker": "hello" }
        ]
    })";

    SushiDSP::RigGraph graph(256, 4);
    int call_count = 0;
    std::string seen_marker;
    SushiDSP::deserialize_rig_from_json(
        graph, json_text, info,
        [&call_count, &seen_marker](SushiDSP::NodeHandle, const nlohmann::json& node_json)
        {
            ++call_count;
            seen_marker = node_json.value("marker", std::string());
        });

    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(seen_marker, "hello");
}

TEST(Unit_Dsp, SerializeAndDeserializeStillWorkWithNoCallback)
{
    // Call sites without a callback behave unchanged. See tests/session/rig_preset/README.md.
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(SushiDSP::make_node("sushidsp.tubescreamer", info));
    graph.compile(info);

    const std::string json_text = SushiDSP::serialize_rig_to_json(graph);
    SushiDSP::RigGraph rebuilt(256, 4);
    SushiDSP::deserialize_rig_from_json(rebuilt, json_text, info);

    EXPECT_EQ(rebuilt.nodes().size(), 1u);
}
