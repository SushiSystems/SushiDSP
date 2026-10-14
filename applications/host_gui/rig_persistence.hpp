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

#include <cstdint>
#include <string>
#include <unordered_map>

#include <SushiDSP/foundation/core/audio_node.hpp>

#include "session_timeline.hpp"
#include "slot_graph.hpp"

/// @brief Says how much of a preset a save writes and a load reads.
enum class PresetScope : std::uint8_t
{
    Session, ///< The rig and the session beside it: the lanes, their clips, the loop and the tempo.
    Rig      ///< The rig alone; the session is neither written nor touched.
};

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

        /// @brief Serializes the current rig to JSON via SushiDSP::serialize_rig_to_json(), as
        /// format 2. Cabinet IR is included via the `"cabinet_ir"` JSON field and a room's
        /// arrangement via `"room"`. Every slot is written, a bypassed one included, and a bypassed
        /// slot carries `"bypassed": true`. Beside `"nodes"` sit `"format"`, the `"edges"` with
        /// their ports, the rig's `"shape"` when one was recorded, and a non-empty ir_folder() as
        /// `"ir_folder"`.
        /// @throws std::runtime_error if the unbypassed graph does not compile, which is what the
        /// preset records; a caller writing to a file must serialize before it truncates the file.
        std::string save_to_json(PresetScope scope = PresetScope::Session) const;

        /// @brief Replaces the entire rig from JSON and republishes. Fresh SlotIds are assigned in the
        /// preset's node order. A format 1 preset loads with its room taken out of the chain and kept
        /// for take_legacy_room().
        /// @throws std::runtime_error if json_text is malformed, if its format is newer than this build
        /// reads, or if a room it carries is refused.
        /// @see applications/host_gui/README.md, "Preset format"
        void load_from_json(const std::string& json_text,
                            PresetScope scope = PresetScope::Session);

    private:
        /// @brief Serializes the slots and edges through SushiDSP::serialize_rig_to_json(), with no
        /// rig-level key of the host's own. save_to_json() adds those on top.
        /// @param handles_out Receives the id each slot's node carries in the document.
        /// @return The `"nodes"` document.
        /// @throws std::runtime_error if the unbypassed graph does not compile.
        std::string serialize_nodes(
            std::unordered_map<SlotGraph::SlotId, SushiDSP::NodeHandle>& handles_out) const;

        SushiDSP::PrepareInfo info_;
        SlotGraph& graph_;
        SessionTimeline& session_;
};
