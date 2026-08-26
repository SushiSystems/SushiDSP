/**************************************************************************/
/* RigPreset.hpp                                                          */
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

#pragma once

#include <string>

#include "SushiDSP/core/RigGraph.hpp"

namespace SushiDSP
{
    /// @brief Serializes a compiled RigGraph's topology and every declared parameter to a JSON
    /// string: `{"nodes": [{"id", "type", "params", "inputs"}, ...]}`, where each node's
    /// `inputs` lists the preset-local `id`s of its upstream nodes in connect() order.
    /// @param graph The graph to serialize. Must have been compile()'d (uses node_registry() to
    /// look up each live node's declared parameters by matching NodeDescriptor::id).
    /// @return The JSON text, pretty-printed.
    /// @throws std::runtime_error if a live node's type is not found in node_registry() (should
    /// not happen for any node constructed via make_node(), but a hand-built RigGraph using a
    /// non-registered IAudioNode subclass cannot be serialized).
    std::string serialize_rig_to_json(const RigGraph& graph);

    /// @brief Parses a JSON string produced by serialize_rig_to_json() (or hand-written to the
    /// same schema) and rebuilds it into graph via add_node()/connect() — does not call
    /// compile(); the caller compiles afterward once, the same as any other graph edit.
    /// @param graph An empty or existing RigGraph to add the parsed nodes/edges into.
    /// @param json_text The JSON text to parse.
    /// @throws std::runtime_error if the JSON is malformed, or references a `type` not found in
    /// node_registry().
    void deserialize_rig_from_json(RigGraph& graph, const std::string& json_text);
}
