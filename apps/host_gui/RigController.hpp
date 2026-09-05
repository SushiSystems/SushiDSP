/**************************************************************************/
/* RigController.hpp                                                      */
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

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"

/// @brief Owns the authoritative rig topology (a set of slots, each with a descriptor id and
/// current parameter values, plus edges between them) and rebuilds + publishes a fresh
/// SushiDSP::RigGraph through an SushiDSP::RtGraphSlot on every structural edit. Not real-time
/// safe — every method here runs on the GUI/main thread only. Not part of the SushiDSP::
/// namespace: this is app-local glue, not a library product.
///
/// Slot identity is a SlotId, stable across every edit (insert, move, remove) — unlike a chain
/// index, which shifts meaning whenever the chain changes shape. A SlotId is unrelated to
/// SushiDSP::NodeHandle, which RigGraph::add_node() assigns fresh on every rebuild.
class RigController
{
public:
    using SlotId = std::uint32_t;

    explicit RigController(const SushiDSP::PrepareInfo& info);

    /// @brief Adds a new, unconnected slot and republishes.
    /// @param descriptor_id The NodeDescriptor id to construct.
    /// @param publish_now Pass false to batch several structural edits (e.g. building a
    /// multi-node shape where every individual add_node()/connect() call would otherwise leave
    /// the graph in a state RigGraph::compile() rejects) into one rebuild; call publish() once
    /// when done. Every existing single-call use is unaffected — the default republishes
    /// immediately, exactly as before.
    /// @return The new slot's id.
    SlotId add_node(const std::string& descriptor_id, bool publish_now = true);

    /// @brief Connects one slot's output to another's input and republishes. An edge the rig
    /// already holds is a no-op: the rig never carries the same edge twice.
    /// @param publish_now Pass false to batch several structural edits (e.g. building a
    /// multi-node shape where every individual add_node()/connect() call would otherwise leave
    /// the graph in a state RigGraph::compile() rejects) into one rebuild; call publish() once
    /// when done. Every existing single-call use is unaffected — the default republishes
    /// immediately, exactly as before.
    void connect(SlotId from, SlotId to, bool publish_now = true);

    /// @brief Removes a single edge, if present, and republishes.
    /// @param publish_now Pass false to batch several structural edits (e.g. building a
    /// multi-node shape where every individual add_node()/connect() call would otherwise leave
    /// the graph in a state RigGraph::compile() rejects) into one rebuild; call publish() once
    /// when done. Every existing single-call use is unaffected — the default republishes
    /// immediately, exactly as before.
    void disconnect(SlotId from, SlotId to, bool publish_now = true);

    /// @brief Removes a live slot and every edge touching it, then republishes. If the removed
    /// slot sat in the middle of a linear run (one upstream edge, one downstream edge), its
    /// neighbors are reconnected directly to each other, preserving a working chain around the
    /// gap. This bridging is specific to removing a live slot through this method: publish() may
    /// separately drop an unrelated slot whose descriptor_id is unknown to the node registry,
    /// and such a slot's edges are erased without bridging around it (unreachable in practice —
    /// registry-driven edits and rig_preset's own validation both keep an unknown type from ever
    /// reaching this controller).
    /// @param publish_now Pass false to batch several structural edits (e.g. building a
    /// multi-node shape where every individual add_node()/connect() call would otherwise leave
    /// the graph in a state RigGraph::compile() rejects) into one rebuild; call publish() once
    /// when done. Every existing single-call use is unaffected — the default republishes
    /// immediately, exactly as before.
    void remove(SlotId slot, bool publish_now = true);

    /// @brief Rebuilds a fresh RigGraph from the current slots and edges, compiles it, and
    /// publishes it through the RtGraphSlot. Called automatically by every edit that publishes;
    /// call it explicitly after a batch of add_node()/connect()/disconnect()/remove() calls made
    /// with publish_now = false.
    /// @throws std::runtime_error if the resulting graph does not compile (e.g. it has no sink).
    void publish();

    /// @brief Marks a slot bypassed or active and republishes. A bypassed slot keeps its
    /// parameters, its cabinet path and its edges; the published graph simply leaves it out and
    /// connects its upstream neighbours straight to its downstream neighbours.
    /// @param slot The slot; a slot that is not live is ignored.
    /// @param bypassed True to bypass.
    /// @throws std::runtime_error if the bridged graph does not compile; the flag is left
    /// unchanged and the previous graph keeps playing.
    void set_bypassed(SlotId slot, bool bypassed);

    /// @brief Reports whether a slot is bypassed.
    /// @param slot The slot.
    /// @return True when the slot is live and bypassed.
    bool is_bypassed(SlotId slot) const;

    /// @brief Adds a new slot connected after the chain's current tail (or as the sole slot, if
    /// the rig is empty) and republishes. Convenience for the common linear-append case.
    /// @return The new slot's id.
    SlotId append(const std::string& descriptor_id);

    /// @brief Adds a new slot immediately before `following` in chain order (rewiring the one
    /// edge that must move), or at the very front if `following` is std::nullopt, and
    /// republishes. Convenience for a linear "insert at this rack position" edit.
    /// @return The new slot's id.
    SlotId insert_before(std::optional<SlotId> following, const std::string& descriptor_id);

    /// @brief Moves `moved` so it sits immediately before `following` in chain order (or at the
    /// very front if `following` is std::nullopt), and republishes.
    void move_before(SlotId moved, std::optional<SlotId> following);

    /// @brief Sets a parameter on `slot`. Does NOT republish (no structural change) and never
    /// touches the live node directly: the value is stored in this controller's own slot state
    /// and queued on the active RigGraph's lock-free parameter ring, where the audio thread
    /// applies it at the top of its next process() call. If the ring is full the change is
    /// dropped and dropped_parameter_changes() is incremented; the call never blocks.
    void set_parameter(SlotId slot, const char* param_id, float value);

    /// @brief Reads this controller's stored value for a parameter on `slot`.
    float get_parameter(SlotId slot, const char* param_id) const;

    /// @brief Installs a cabinet impulse response on the JCM800 at `slot`. Loads the file at the
    /// controller's prepared sample rate, hands it to the live node when the slot has one, and
    /// remembers the path so a later rebuild (or a preset save) can reproduce it. A bypassed
    /// slot has no live node, so it keeps the path and receives the cabinet on the publish that
    /// brings it back.
    /// @return True if `slot` is a JCM800 and the path was stored.
    /// @throws std::runtime_error if the file cannot be opened or parsed.
    bool set_cabinet(SlotId slot, const std::string& path);

    /// @brief Reclaims retired graphs and every live JCM800's retired cabinet. Control thread
    /// only; call once per GUI frame.
    void reclaim();

    /// @brief Counts parameter changes dropped because the audio thread's ring was full.
    std::size_t dropped_parameter_changes() const;

    /// @brief Every live slot's id, in a stable (insertion-derived) enumeration order — for a
    /// purely linear chain (the only shape any current GUI edit path constructs), this is also
    /// the chain's visual left-to-right order.
    std::vector<SlotId> slot_order() const;

    /// @brief A slot's NodeDescriptor id.
    /// @return The id, or "" if slot is not live.
    std::string descriptor_id(SlotId slot) const;

    /// @brief A slot's cabinet impulse response path, or "" if it has none or isn't a JCM800.
    std::string cabinet_path(SlotId slot) const;

    /// @brief Lists the ids of every slot with an edge into `slot`, in connect() call order.
    std::vector<SlotId> upstream_of(SlotId slot) const;

    /// @brief The currently active, published RigGraph (for the RT thread to acquire from).
    SushiDSP::RigGraph* active_graph();

    /// @brief The RtGraphSlot the audio callback should acquire_for_audio_thread() from.
    SushiDSP::RtGraphSlot& graph_slot();

    /// @brief Serializes the current rig to JSON via SushiDSP::serialize_rig_to_json(). Cabinet
    /// IR is included via the `"cabinet_ir"` JSON field. Every slot is written, a bypassed one
    /// included, and a bypassed slot carries `"bypassed": true`.
    /// @throws std::runtime_error if the unbypassed graph does not compile, which is what the
    /// preset records; a caller writing to a file must serialize before it truncates the file.
    std::string save_to_json() const;

    /// @brief Replaces the entire rig from JSON via SushiDSP::deserialize_rig_from_json() and
    /// republishes. Fresh SlotIds are assigned in the parsed graph's node order — preset-local
    /// continuity across a save/load round trip is not preserved (the JSON schema was never
    /// designed to carry a GUI-local SlotId, only a preset-local integer id).
    /// @throws std::runtime_error if json_text is malformed (same contract as
    /// SushiDSP::deserialize_rig_from_json()).
    void load_from_json(const std::string& json_text);

private:
    struct Slot
    {
        std::string                     descriptor_id;
        std::map<std::string, float>    parameters;
        std::string                     cabinet_path;
        bool                            bypassed = false;
    };
    struct Edge { SlotId from; SlotId to; };

    void apply_cabinet_to_live_node(SlotId slot);
    void seed_defaults(Slot& slot) const;

    /// @brief Builds a fresh RigGraph from the current slots and edges and compiles it. Const: a
    /// slot whose descriptor_id the node registry does not know is skipped rather than erased,
    /// so handles_out names exactly the slots that reached the graph.
    /// @param apply_bypass True to leave bypassed slots out and bridge around them; false to add
    /// every live slot and honour every edge, which is the shape a preset records.
    /// @param handles_out Receives the slot-to-handle mapping for the graph that was built.
    /// @return The compiled graph.
    /// @throws std::runtime_error if bridging would exceed RigGraph::kMaxFanInOut, or if the
    /// graph does not compile.
    std::unique_ptr<SushiDSP::RigGraph> build_graph(bool apply_bypass,
        std::unordered_map<SlotId, SushiDSP::NodeHandle>& handles_out) const;

    /// @brief Lists every edge whose endpoints both reached the graph. Duplicate edges are
    /// dropped, so a preset that names one twice still builds one edge.
    /// @param live The slot-to-handle mapping for the graph being built.
    /// @return The edges, as (from, to) pairs.
    std::vector<std::pair<SlotId, SlotId>> direct_edges(
        const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const;

    /// @brief Lists every edge with bypassed slots collapsed away: an edge into a bypassed slot
    /// is followed through to each live slot reachable past it, so a run of bypassed slots
    /// bridges in one step. Duplicate edges are dropped.
    /// @param live The slot-to-handle mapping for the graph being built.
    /// @return The bridged edges, as (from, to) pairs.
    std::vector<std::pair<SlotId, SlotId>> bridged_edges(
        const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const;

    SushiDSP::PrepareInfo                          info_;
    std::unordered_map<SlotId, Slot>               slots_;
    std::vector<Edge>                              edges_;
    std::vector<SlotId>                             slot_order_;
    SlotId                                          next_slot_id_ = 0;
    std::unordered_map<SlotId, SushiDSP::NodeHandle> slot_to_handle_;
    SushiDSP::RtGraphSlot                          graph_slot_;
    SushiDSP::RigGraph*                            active_graph_ = nullptr;
    std::size_t                                     dropped_parameter_changes_ = 0;
};
