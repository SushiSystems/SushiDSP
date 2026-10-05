/****************************************************************/
/* RigPreset.hpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

#include "SushiDSP/core/RigGraph.hpp"

namespace SushiDSP
{
    /// @brief Called once per serialized node, after its "id"/"type"/"params"/"inputs" fields
    /// are written, with the JSON object being built for it — lets a caller add extra fields
    /// modules/rig_preset itself has no opinion about (e.g. a Jcm800's cabinet IR path). The
    /// object is handed over mutable and unguarded, so a caller may also overwrite fields that
    /// were already written: apps/host_gui's RigController replaces "params" wholesale with its
    /// own stored values rather than the live node's.
    using AnnotateNodeFn = std::function<void(NodeHandle, nlohmann::json&)>;

    /// @brief Called once per node constructed during deserialize, with its full parsed JSON
    /// entry, after the node is add_node()-ed but before params/edges are applied — lets a
    /// caller read an extra field it wrote via AnnotateNodeFn and act on it (e.g. install a
    /// cabinet). Never receives a malformed entry: the entry has already passed every shape
    /// check in this function's first validation pass.
    using OnNodeLoadedFn = std::function<void(NodeHandle, const nlohmann::json&)>;

    /// @brief Serializes a compiled RigGraph's topology and every declared parameter to a JSON
    /// string: `{"nodes": [{"id", "type", "params", "inputs"}, ...]}`, where each node's
    /// `inputs` lists the preset-local `id`s of its upstream nodes in connect() order.
    /// @param graph The graph to serialize. Must have been compile()'d (uses node_registry() to
    /// look up each live node's declared parameters by matching NodeDescriptor::id).
    /// @param annotate_node Optional; if non-null, called once per node with the JSON object
    /// being built for it, letting the caller add extra fields or overwrite ones already
    /// written. Null means no extra fields.
    /// @return The JSON text, pretty-printed.
    /// @throws std::runtime_error if a live node's type is not found in node_registry() (should
    /// not happen for any node constructed via make_node(), but a hand-built RigGraph using a
    /// non-registered IAudioNode subclass cannot be serialized).
    std::string serialize_rig_to_json(const RigGraph& graph, AnnotateNodeFn annotate_node = nullptr);

    /// @brief Parses a JSON string produced by serialize_rig_to_json() (or hand-written to the
    /// same schema) and rebuilds it into graph via add_node()/connect() — does not call
    /// compile(); the caller compiles afterward once, the same as any other graph edit.
    /// @param graph An empty or existing RigGraph to add the parsed nodes/edges into.
    /// @param json_text The JSON text to parse.
    /// @param info The PrepareInfo each node is constructed with.
    /// @param on_node_loaded Optional; if non-null, called once per constructed node with its
    /// full parsed JSON entry. Null means no extra handling.
    /// @throws std::runtime_error if the JSON is malformed: a missing or wrong-shaped `id`,
    /// `type`, `params` or `inputs` key, a `type` not found in node_registry(), a duplicate `id`,
    /// or an `inputs` entry referencing an id no entry declares. Every entry is fully validated
    /// in a first pass before any node is added, so graph is left unmodified for any malformed
    /// entry. (nlohmann::json::parse_error still escapes for text that is not valid JSON.)
    void deserialize_rig_from_json(RigGraph& graph, const std::string& json_text,
        const PrepareInfo& info, OnNodeLoadedFn on_node_loaded = nullptr);
}
