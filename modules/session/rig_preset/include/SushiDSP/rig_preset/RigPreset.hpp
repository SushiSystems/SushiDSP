/****************************************************************/
/* RigPreset.hpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file RigPreset.hpp
 * @brief Declares the functions that save a rig graph to JSON and rebuild one from it.
 * @author Mustafa Garip
 */

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

    /// @brief Serializes a compiled RigGraph's topology and every declared parameter to JSON.
    /// @param graph The graph to serialize; it must have been compile()'d.
    /// @param annotate_node Optional; called once per node with the JSON object being built for it.
    /// @return The JSON text, pretty-printed.
    /// @throws std::runtime_error if a live node's type is not found in node_registry().
    /// @see modules/session/rig_preset/README.md, "What serialization writes"
    std::string serialize_rig_to_json(const RigGraph& graph, AnnotateNodeFn annotate_node = nullptr);

    /// @brief Parses JSON in serialize_rig_to_json()'s schema and adds its nodes and edges.
    /// @param graph An empty or existing RigGraph; the caller compiles it afterward.
    /// @param on_node_loaded Optional; called once per constructed node with its parsed JSON entry.
    /// @throws std::runtime_error if an entry is malformed; graph is then left unmodified.
    /// @throws nlohmann::json::parse_error if the text is not valid JSON.
    /// @see modules/session/rig_preset/README.md, "What deserialization accepts"
    void deserialize_rig_from_json(RigGraph& graph, const std::string& json_text,
        const PrepareInfo& info, OnNodeLoadedFn on_node_loaded = nullptr);
}
