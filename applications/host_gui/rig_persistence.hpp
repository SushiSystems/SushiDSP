/**************************************************************************/
/* rig_persistence.hpp                                                    */
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

/** @file rig_persistence.hpp
 *  @brief Declares the preset codec that writes and reads a rig graph and a session timeline.
 *  @author Mustafa Garip
 */

#pragma once

#include <string>

#include "SushiDSP/core/audio_node.hpp"

#include "session_timeline.hpp"
#include "slot_graph.hpp"

/// @brief Writes a preset from a SlotGraph and a SessionTimeline and reads one back into them.
/// It reads both; neither knows it exists, so a preset field is added here and nowhere else.
/// GUI thread only.
class RigPersistence
{
    public:
        /// @brief Binds the codec to the graph and the timeline it serializes.
        /// @param info The prepare info a loaded preset's scratch graph is built at.
        /// @param graph The rig graph to write from and rebuild into.
        /// @param session The arrangement to write from and rebuild into.
        RigPersistence(const SushiDSP::PrepareInfo& info, SlotGraph& graph,
                       SessionTimeline& session);

        /// @brief Serializes the current rig to JSON via SushiDSP::serialize_rig_to_json(). Cabinet
        /// IR is included via the `"cabinet_ir"` JSON field. Every slot is written, a bypassed one
        /// included, and a bypassed slot carries `"bypassed": true`. A non-empty ir_folder() is
        /// written as the rig-level `"ir_folder"` key beside `"nodes"`.
        /// @throws std::runtime_error if the unbypassed graph does not compile, which is what the
        /// preset records; a caller writing to a file must serialize before it truncates the file.
        std::string save_to_json() const;

        /// @brief Replaces the entire rig from JSON via SushiDSP::deserialize_rig_from_json() and
        /// republishes. Fresh SlotIds are assigned in the parsed graph's node order — preset-local
        /// continuity across a save/load round trip is not preserved (the JSON schema was never
        /// designed to carry a GUI-local SlotId, only a preset-local integer id).
        /// @throws std::runtime_error if json_text is malformed (same contract as
        /// SushiDSP::deserialize_rig_from_json()). A preset with no `"ir_folder"` key loads with an
        /// empty folder.
        void load_from_json(const std::string& json_text);

    private:
        /// @brief Serializes the slots and edges through SushiDSP::serialize_rig_to_json(), with no
        /// rig-level key of the host's own. save_to_json() adds those on top.
        /// @return The `"nodes"` document.
        /// @throws std::runtime_error if the unbypassed graph does not compile.
        std::string serialize_nodes() const;

        SushiDSP::PrepareInfo info_;
        SlotGraph& graph_;
        SessionTimeline& session_;
};
