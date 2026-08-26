/**************************************************************************/
/* test_rig_preset.cpp                                                    */
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
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"
#include "SushiDSP/rig_preset/RigPreset.hpp"

TEST(Unit_Dsp, SerializeRigToJsonProducesOneEntryPerNodeWithInputs)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
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
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };

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
    SushiDSP::deserialize_rig_from_json(graph, json_text);
    graph.compile(info);

    const auto handles = graph.nodes();
    ASSERT_EQ(handles.size(), 5u);

    bool found_drive_075 = false;
    bool found_drive_010 = false;
    for (SushiDSP::NodeHandle handle : handles)
    {
        SushiDSP::IAudioNode* node = graph.node(handle);
        const float drive = node->get_parameter("drive");
        if (std::abs(drive - 0.75f) < 1e-6f) { found_drive_075 = true; }
        if (std::abs(drive - 0.1f) < 1e-6f) { found_drive_010 = true; }
    }
    EXPECT_TRUE(found_drive_075);
    EXPECT_TRUE(found_drive_010);
}

TEST(Unit_Dsp, SerializeThenDeserializeRoundTripsABranchingGraph)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    SushiDSP::RigGraph original(256, 6);
    auto source_ts = original.add_node(SushiDSP::make_node("sushidsp.tubescreamer", info));
    original.node(source_ts)->set_parameter("drive", 0.9f);
    auto splitter = original.add_node(SushiDSP::make_node("sushidsp.splitter", info));
    auto amp = original.add_node(SushiDSP::make_node("sushidsp.jcm800", info));
    auto dry = original.add_node(SushiDSP::make_node("sushidsp.tubescreamer", info));
    auto mixer = original.add_node(SushiDSP::make_node("sushidsp.mixer", info));
    original.connect(source_ts, splitter);
    original.connect(splitter, amp);
    original.connect(splitter, dry);
    original.connect(amp, mixer);
    original.connect(dry, mixer);
    original.node(mixer)->set_parameter("gain_0", 0.3f);
    original.node(mixer)->set_parameter("gain_1", 0.7f);
    original.compile(info);

    const std::string json_text = SushiDSP::serialize_rig_to_json(original);

    SushiDSP::RigGraph rebuilt(256, 6);
    SushiDSP::deserialize_rig_from_json(rebuilt, json_text);
    rebuilt.compile(info);

    ASSERT_EQ(rebuilt.nodes().size(), original.nodes().size());
    bool found_mixer_with_gains = false;
    for (SushiDSP::NodeHandle handle : rebuilt.nodes())
    {
        SushiDSP::IAudioNode* node = rebuilt.node(handle);
        if (std::abs(node->get_parameter("gain_0") - 0.3f) < 1e-6f
            && std::abs(node->get_parameter("gain_1") - 0.7f) < 1e-6f)
        {
            found_mixer_with_gains = true;
        }
    }
    EXPECT_TRUE(found_mixer_with_gains);
}
