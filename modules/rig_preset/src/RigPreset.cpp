/**************************************************************************/
/* RigPreset.cpp                                                          */
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

#include "SushiDSP/rig_preset/RigPreset.hpp"

#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

#include "SushiDSP/node_registry/NodeRegistry.hpp"

namespace SushiDSP
{
    namespace
    {
        const NodeDescriptor* descriptor_for_type(const char* type)
        {
            for (const NodeEntry& entry : node_registry())
            {
                if (std::strcmp(entry.descriptor->id, type) == 0)
                {
                    return entry.descriptor;
                }
            }
            return nullptr;
        }
    }

    std::string serialize_rig_to_json(const RigGraph& graph)
    {
        nlohmann::json root;
        root["nodes"] = nlohmann::json::array();

        const std::vector<NodeHandle> handles = graph.nodes();

        for (NodeHandle handle : handles)
        {
            IAudioNode* node = graph.node(handle);
            const char* type = node->type_id();
            const NodeDescriptor* descriptor = descriptor_for_type(type);
            if (descriptor == nullptr)
            {
                throw std::runtime_error(
                    "serialize_rig_to_json: node type_id() '" + std::string(type)
                    + "' is not registered in node_registry()");
            }

            nlohmann::json node_json;
            node_json["id"] = handle;
            node_json["type"] = type;

            nlohmann::json params_json = nlohmann::json::object();
            for (std::size_t p = 0; p < descriptor->parameter_count; ++p)
            {
                const char* param_id = descriptor->parameters[p].id;
                params_json[param_id] = node->get_parameter(param_id);
            }
            node_json["params"] = params_json;

            nlohmann::json inputs_json = nlohmann::json::array();
            for (NodeHandle upstream : graph.upstream_of(handle))
            {
                inputs_json.push_back(upstream);
            }
            node_json["inputs"] = inputs_json;

            root["nodes"].push_back(node_json);
        }

        return root.dump(2);
    }

    void deserialize_rig_from_json(RigGraph& graph, const std::string& json_text,
        const PrepareInfo& info)
    {
        const nlohmann::json root = nlohmann::json::parse(json_text);

        // First pass: fully validate every entry — key presence, key shape, type resolution,
        // duplicate ids and dangling input references — before mutating graph at all, so a
        // corrupt/edited-by-hand preset (or a wrong file entirely, e.g. a host_gui preset,
        // which has "type" but no "id") never leaves the caller's live graph half-populated
        // with orphan nodes that would then break its next compile().
        std::vector<const NodeDescriptor*> descriptors;
        descriptors.reserve(root.at("nodes").size());
        std::set<int> declared_ids;
        std::size_t validate_index = 0;
        for (const auto& node_json : root.at("nodes"))
        {
            const std::string where = " (entry " + std::to_string(validate_index++) + ")";

            if (!node_json.contains("type") || !node_json.at("type").is_string())
            {
                throw std::runtime_error(
                    "deserialize_rig_from_json: missing or non-string 'type'" + where);
            }
            if (!node_json.contains("id") || !node_json.at("id").is_number_integer())
            {
                throw std::runtime_error(
                    "deserialize_rig_from_json: missing or non-integer 'id'" + where);
            }
            if (!node_json.contains("params") || !node_json.at("params").is_object())
            {
                throw std::runtime_error(
                    "deserialize_rig_from_json: missing or non-object 'params'" + where);
            }
            if (!node_json.contains("inputs") || !node_json.at("inputs").is_array())
            {
                throw std::runtime_error(
                    "deserialize_rig_from_json: missing or non-array 'inputs'" + where);
            }

            const std::string type = node_json.at("type").get<std::string>();
            const NodeDescriptor* descriptor = descriptor_for_type(type.c_str());
            if (descriptor == nullptr)
            {
                throw std::runtime_error("deserialize_rig_from_json: unknown node type '" + type + "'" + where);
            }
            descriptors.push_back(descriptor);

            const int preset_id = node_json.at("id").get<int>();
            if (!declared_ids.insert(preset_id).second)
            {
                throw std::runtime_error("deserialize_rig_from_json: duplicate node id "
                    + std::to_string(preset_id) + where);
            }
        }

        // Dangling-reference check needs the complete id set, so it runs as its own sweep.
        // Doing it here (rather than letting a map .at() throw during the build pass) keeps
        // the documented std::runtime_error contract and the graph-untouched guarantee.
        validate_index = 0;
        for (const auto& node_json : root.at("nodes"))
        {
            const std::string where = " (entry " + std::to_string(validate_index++) + ")";
            for (const auto& input_id : node_json.at("inputs"))
            {
                if (!input_id.is_number_integer())
                {
                    throw std::runtime_error(
                        "deserialize_rig_from_json: non-integer value in 'inputs'" + where);
                }
                const int referenced = input_id.get<int>();
                if (declared_ids.count(referenced) == 0)
                {
                    throw std::runtime_error("deserialize_rig_from_json: 'inputs' references unknown node id "
                        + std::to_string(referenced) + where);
                }
            }
        }

        std::map<int, NodeHandle> preset_id_to_handle;
        std::map<NodeHandle, std::vector<int>> pending_inputs;
        std::map<NodeHandle, std::map<std::string, float>> pending_params;

        std::size_t entry_index = 0;
        for (const auto& node_json : root.at("nodes"))
        {
            const int preset_id = node_json.at("id").get<int>();
            const NodeDescriptor* descriptor = descriptors[entry_index++];

            NodeHandle handle = graph.add_node(descriptor->make(info));
            preset_id_to_handle[preset_id] = handle;

            std::map<std::string, float> params;
            for (const auto& [key, value] : node_json.at("params").items())
            {
                params[key] = value.get<float>();
            }
            pending_params[handle] = std::move(params);

            std::vector<int> inputs;
            for (const auto& input_id : node_json.at("inputs"))
            {
                inputs.push_back(input_id.get<int>());
            }
            pending_inputs[handle] = std::move(inputs);
        }

        // Parameters are applied in std::map order, i.e. alphabetical by parameter id — that
        // ordering is incidental, not designed. Every set_parameter() implementation today is
        // order-independent; if one ever becomes order-dependent, this must switch to
        // preserving descriptor (or JSON-file) order instead.
        for (const auto& [handle, params] : pending_params)
        {
            IAudioNode* node = graph.node(handle);
            for (const auto& [key, value] : params)
            {
                node->set_parameter(key.c_str(), value);
            }
        }

        for (const auto& [handle, input_preset_ids] : pending_inputs)
        {
            for (int input_preset_id : input_preset_ids)
            {
                graph.connect(preset_id_to_handle.at(input_preset_id), handle);
            }
        }
    }
}
