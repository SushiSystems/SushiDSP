/**************************************************************************/
/* slot_graph.hpp                                                         */
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

/** @file slot_graph.hpp
 *  @brief Declares the rig's authoritative slot-and-edge graph and the node work hanging off a slot.
 *  @author Mustafa Garip
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/foundation/core/rt_graph_slot.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/room_host.hpp>
#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include "rig_shape.hpp"
#include <SushiDSP/nodes/room/room_scene.hpp>

#include "ui/geometry.hpp"

/// @brief Names the signal the host assembles into the rig's input buffer. Track and Both are
/// what re-amping is made of - a dry take driven through the rig instead of, or on top of, the
/// live instrument. See the stereo tracks design spec in the version history section 6.
enum class RigInputSource : std::uint32_t
{
    LiveInput = 0,
    Track = 1,
    Both = 2
};

/// @brief Owns the authoritative rig topology - slots carrying a descriptor id and parameter
/// values, plus the edges between them - and republishes a freshly compiled SushiDSP::RigGraph
/// through an SushiDSP::RtGraphSlot on every structural edit. App-local glue, GUI thread only.
/// A slot is named by a SlotId, stable across every insert, move and remove, and unrelated to
/// the SushiDSP::NodeHandle RigGraph::add_node() assigns fresh on each rebuild.
/// @see the stereo tracks design spec in the version history
class SlotGraph
{
    public:
        /// @brief The slot identity type, owned by ui::Geometry so the layout layer can name it
        /// without including this header.
        using SlotId = ui::SlotId;

        /// @brief Holds what a slot needs to be rebuilt: descriptor, parameters, cabinet, bypass, room.
        struct SlotSnapshot
        {
                std::string descriptor_id;
                std::map<std::string, float> parameters;
                std::string cabinet_path;
                bool bypassed = false;
                std::optional<SushiDSP::Room::Arrangement> room_arrangement;
        };

        explicit SlotGraph(const SushiDSP::PrepareInfo& info);

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
        void connect(SlotId from, SlotId to, bool publish_now = true,
                     std::uint32_t from_port = 0);

        /// @brief Removes a single edge, if present, and republishes.
        /// @param publish_now Pass false to batch several structural edits (e.g. building a
        /// multi-node shape where every individual add_node()/connect() call would otherwise leave
        /// the graph in a state RigGraph::compile() rejects) into one rebuild; call publish() once
        /// when done. Every existing single-call use is unaffected — the default republishes
        /// immediately, exactly as before.
        void disconnect(SlotId from, SlotId to, bool publish_now = true);

        /// @brief Removes a live slot and every edge touching it, then republishes. A slot with one
        /// upstream and one downstream edge leaves its two neighbours joined, so the chain around the
        /// gap keeps working.
        /// @param slot The slot to remove.
        /// @param publish_now False batches this edit with the ones after it, so a shape
        /// RigGraph::compile() would reject halfway through is published only once it is whole.
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

        /// @brief Chooses the host input channel a slot reads when the graph roots it.
        /// @param slot The slot; an unknown slot is a no-op.
        /// @param channel The host input channel, below RigGraph::K_MAX_CHANNELS.
        /// @param publish_now Publishes the graph when true.
        void set_input_channel(SlotId slot, std::uint32_t channel, bool publish_now = true);

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

        /// @brief Adds a node as a second parallel branch out of `splitter`, creating the mixer that
        /// rejoins the branches when the rig does not already have one, and republishes once for the
        /// whole edit.
        /// @param splitter The splitter to branch from.
        /// @param descriptor_id The NodeDescriptor id to construct for the new branch.
        /// @return The new slot's id, or std::nullopt when `splitter` is not a live splitter or
        /// `descriptor_id` is not registered.
        /// @throws std::runtime_error if the resulting graph does not compile.
        std::optional<SlotId> add_branch(SlotId splitter, const std::string& descriptor_id);

        /// @brief Moves `moved` so it sits immediately before `following` in chain order (or at the
        /// very front if `following` is std::nullopt), and republishes.
        void move_before(SlotId moved, std::optional<SlotId> following);

        /// @brief Sets a parameter on `slot`. Does NOT republish (no structural change) and never
        /// touches the live node directly: the value is stored in this graph's own slot state
        /// and queued on the active RigGraph's lock-free parameter ring, where the audio thread
        /// applies it at the top of its next process() call. If the ring is full the change is
        /// dropped and dropped_parameter_changes() is incremented; the call never blocks.
        void set_parameter(SlotId slot, const char* param_id, float value);

        /// @brief Reads this graph's stored value for a parameter on `slot`.
        float get_parameter(SlotId slot, const char* param_id) const;

        /// @brief Installs a cabinet impulse response on the amplifier at `slot`, loading it at the
        /// prepared sample rate and remembering the path so a rebuild or a preset reproduces it. A
        /// bypassed slot keeps the path and takes the cabinet on the publish that brings it back.
        /// @param slot The slot to install the cabinet on.
        /// @param path The impulse-response file.
        /// @return True when the node took the impulse response, or the bypassed slot stored it.
        /// @throws std::runtime_error if the file cannot be opened or parsed.
        bool set_cabinet(SlotId slot, const std::string& path);

        /// @brief Reclaims retired graphs and the retired cabinets of every live node that hosts
        /// one. Control thread only; call once per GUI frame.
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

        /// @brief A slot's cabinet impulse response path, or "" if it has none or hosts no cabinet.
        std::string cabinet_path(SlotId slot) const;

        /// @brief Reports whether a slot's node accepts a cabinet impulse response.
        /// @param slot The slot.
        /// @return True when the node this slot last published implements SushiDSP::CabinetHost.
        /// A slot added while bypassed has never had a node and reads false until it goes live.
        bool hosts_cabinet(SlotId slot) const;

        /// @brief Bounds how many scenes a second one slot's room takes; a drag inside 1 / K_ROOM_PUBLISH_HZ
        /// of the last publish keeps the newest scene for reclaim() to publish.
        static constexpr double K_ROOM_PUBLISH_HZ = 30.0;

        /// @brief Reports whether a slot's node renders a room.
        /// @param slot The slot.
        /// @return True when the node this slot last published implements SushiDSP::room::RoomHost.
        /// A slot added while bypassed has never had a node and reads false until it goes live.
        bool hosts_room(SlotId slot) const;

        /// @brief The room node in a slot, or nullptr when that slot holds something else or is
        /// bypassed. Control thread only.
        SushiDSP::room::RoomHost* room_host(SlotId slot);

        /// @brief The arrangement a slot's room carries, the stored one whether or not the last
        /// drag has reached the node yet; a slot with none reads the bare starter room.
        /// @param slot The slot.
        /// @return The arrangement.
        SushiDSP::Room::Arrangement room_arrangement(SlotId slot) const;

        /// @brief Stores an arrangement for a slot and publishes it at most K_ROOM_PUBLISH_HZ times
        /// a second, keeping the newest for reclaim() when a publish lands inside the interval. A
        /// bypassed slot keeps the arrangement and takes it on the publish that brings it back.
        /// Control thread.
        /// @param slot The slot.
        /// @param arrangement The shell, the listener, the cabinets and the microphones.
        /// @param error_out Receives the reason when the arrangement is refused; "" otherwise.
        /// @return True when the arrangement was stored, which needs it to be valid.
        bool set_room_arrangement(SlotId slot, const SushiDSP::Room::Arrangement& arrangement,
                                  std::string& error_out);

        /// @brief The shell a slot's room carries, with the listener's pose and the first
        /// cabinet's; a slot with no arrangement reads the default scene.
        SushiDSP::room::RoomScene room_scene(SlotId slot) const;

        /// @brief Replaces the shell, the listener's pose and the first cabinet's pose of a slot's
        /// arrangement from one scene, and publishes as set_room_arrangement() does. Microphones
        /// and further cabinets are left as they are. Control thread.
        /// @param error_out Receives the reason when the scene is refused; "" otherwise.
        /// @return True when the scene was stored, which needs the scene to be valid.
        bool set_room_scene(SlotId slot, const SushiDSP::room::RoomScene& scene,
                            std::string& error_out);

        /// @brief Counts the scenes handed to a slot's room node since the slot went live, so a
        /// test or a status line can see how many of a drag's edits reached the node.
        std::size_t room_publishes(SlotId slot) const;

        /// @brief Reads an HRTF asset at the prepared sample rate and installs it into every room
        /// the rig holds now or builds later; the way cabinet_path() survives a rebuild. A file that
        /// will not load leaves every room on its spherical-head fallback. Control thread.
        /// @param error_out Receives the loader's reason on failure; "" otherwise.
        /// @return True when the set is in place.
        bool set_hrtf_path(const std::string& path, std::string& error_out);

        /// @brief The HRTF asset in place, or "" when every room runs on its fallback.
        std::string hrtf_path() const;

        /// @brief Lists the ids of every slot with an edge into `slot`, in connect() call order.
        std::vector<SlotId> upstream_of(SlotId slot) const;

        /// @brief Every edge the rig holds, in stored (connect() call) order.
        std::vector<std::pair<SlotId, SlotId>> edges() const;

        /// @brief One edge with the upstream output port it reads.
        struct EdgeRecord
        {
                SlotId from;             ///< Holds the upstream slot.
                SlotId to;               ///< Holds the downstream slot.
                std::uint32_t from_port; ///< Holds the upstream output port.
        };

        /// @brief Every edge the graph holds, with its port, in stored order.
        /// @return The records.
        std::vector<EdgeRecord> edge_records() const;


        /// @brief Records the rig's shape: its stages, its DI choice and its hidden slots. Whoever
        /// lays the rig out writes it after each layout, and a preset carries it.
        /// @param shape The shape, naming this graph's slot ids.
        void set_shape(const RigShape& shape);

        /// @brief Reads the shape last recorded, or loaded from a preset.
        /// @return The shape, or nullopt for a rig nothing has laid out.
        const std::optional<RigShape>& shape() const noexcept;

        /// @brief Records the room a preset written before the cabinet section carried.
        /// @param legacy The old room.
        void set_legacy_room(const LegacyRoom& legacy);

        /// @brief Hands over the old room a loaded preset carried, once.
        /// @return The old room, or nullopt when there is none left to hand over.
        std::optional<LegacyRoom> take_legacy_room();

        /// @brief Captures what a live slot needs to be rebuilt.
        /// @param slot The slot to capture.
        /// @return The snapshot, or std::nullopt when the slot is not live.
        std::optional<SlotSnapshot> snapshot(SlotId slot) const;

        /// @brief Re-adds a slot from a snapshot, unconnected, restoring its parameters over the
        /// seeded defaults, its cabinet path, its bypass flag and its room scene, applying them on
        /// the publish that builds the node.
        /// @param snapshot The captured slot state.
        /// @param publish_now Pass false to batch this with other structural edits; call publish()
        /// once when done.
        /// @return The new slot's id.
        SlotId restore(const SlotSnapshot& snapshot, bool publish_now = true);

        /// @brief Drops every edge the rig holds, leaving every slot live but unconnected.
        /// @param publish_now Pass false to batch this with other structural edits; call publish()
        /// once when done.
        void clear_edges(bool publish_now = true);

        /// @brief Drops every slot and every edge and restarts slot numbering at zero, without
        /// publishing; the caller restores the slots it wants and publishes once.
        void clear();

        /// @brief Builds a fresh RigGraph from the current slots and edges and compiles it, skipping
        /// any slot whose descriptor_id the node registry does not know.
        /// @param apply_bypass True leaves bypassed slots out and bridges around them; false adds
        /// every live slot and honours every edge, the shape a preset records.
        /// @param handles_out Receives the slot-to-handle mapping for the graph that was built.
        /// @return The compiled graph.
        /// @throws std::runtime_error if the edges exceed RigGraph::K_MAX_FAN_IN_OUT or the graph does
        /// not compile.
        std::unique_ptr<SushiDSP::RigGraph>
        build_graph(bool apply_bypass,
                    std::unordered_map<SlotId, SushiDSP::NodeHandle>& handles_out) const;

        /// @brief The currently active, published RigGraph (for the RT thread to acquire from).
        SushiDSP::RigGraph* active_graph();

        /// @brief The RtGraphSlot the audio callback should acquire_for_audio_thread() from.
        SushiDSP::RtGraphSlot& graph_slot();

        /// @brief Remembers the folder the cabinet browser should open in. Saved with the preset, so
        /// a tone reopens where its impulse responses live. Does not republish: this changes no
        /// audio. The path is stored raw, machine-local as it is, because the machine owner asked
        /// for exactly that; a preset carried to another machine finds no such folder and the
        /// browser falls back.
        void set_ir_folder(const std::string& path);

        /// @brief The remembered impulse-response folder, or "" when the preset carried none.
        std::string ir_folder() const;

        /// @brief Chooses the signal the host feeds the rig. Control thread; the audio thread reads
        /// the choice on its next block.
        /// @param source The live instrument, the re-amp take, or both summed.
        void set_input_source(RigInputSource source);

        /// @brief Reports the signal the host feeds the rig. Either thread; real-time safe.
        /// @return The current choice.
        RigInputSource input_source() const noexcept
        {
            return input_source_.load(std::memory_order_relaxed);
        }

    private:
        /// @brief Names a moment on the clock the room publish interval is measured on.
        using Clock = std::chrono::steady_clock;

        struct Slot
        {
                std::string descriptor_id;
                std::map<std::string, float> parameters;
                std::string cabinet_path;
                bool bypassed = false;
                bool hosts_cabinet = false;

                /// @brief Holds the host input channel the slot reads when it is a root of the graph.
                std::uint32_t input_channel = 0;

                /// @brief Records that the node this slot last published implements RoomHost.
                bool hosts_room = false;

                /// @brief Holds the arrangement the slot's room renders, once one was set or loaded.
                std::optional<SushiDSP::Room::Arrangement> room_arrangement;

                /// @brief Records that room_arrangement changed since the node last took it.
                bool room_pending = false;

                /// @brief Stamps the last scene handed to the node, for the publish interval.
                std::optional<Clock::time_point> room_published_at;

                /// @brief Counts the scenes handed to the node since the slot went live.
                std::size_t room_publishes = 0;
        };
        struct Edge
        {
                SlotId from;
                SlotId to;

                /// @brief Holds the upstream output port the edge reads; 0 is the main output.
                std::uint32_t from_port = 0;
        };

        /// @brief The slot a forward walk out of a splitter's branch stopped on.
        struct BranchEnd
        {
                SlotId slot;
                bool is_join;
        };

        /// @brief Lists the ids of every slot `slot` has an edge into, in connect() call order.
        /// @param slot The upstream slot to query.
        /// @return The downstream ids.
        std::vector<SlotId> downstream_of(SlotId slot) const;

        /// @brief Walks forward from `start`, one successor per step, stopping at the first slot two
        /// or more edges enter or at the first slot that does not have exactly one downstream edge.
        /// @param start The branch's first slot.
        /// @return The slot the walk stopped on, flagged as a join when two or more edges enter it.
        BranchEnd walk_branch(SlotId start) const;

        void apply_cabinet_to_live_node(SlotId slot);
        void seed_defaults(Slot& slot) const;

        /// @brief Hands a slot's stored arrangement to its live room node when the interval since
        /// the last publish has run, or when force is set, and stamps the publish.
        /// @param error_out Receives the node's reason when it refuses the arrangement.
        /// @return False when the node refused it; true when it took it or nothing was due.
        bool publish_room(SlotId slot, bool force, std::string& error_out);

        /// @brief Installs the HRTF set in place and the stored arrangement into a freshly built
        /// room node, adopting the node's own arrangement when the slot stores none.
        void apply_room_to_live_node(SlotId slot);

        /// @brief Lists every edge whose endpoints both reached the graph. Duplicate edges are
        /// dropped, so a preset that names one twice still builds one edge.
        /// @param live The slot-to-handle mapping for the graph being built.
        /// @return The edges, as (from, to) pairs.
        std::vector<std::pair<SlotId, SlotId>>
        direct_edges(const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const;

        /// @brief Lists every edge with bypassed slots collapsed away: an edge into a bypassed slot
        /// is followed through to each live slot reachable past it, so a run of bypassed slots
        /// bridges in one step. Duplicate edges are dropped.
        /// @param live The slot-to-handle mapping for the graph being built.
        /// @return The bridged edges, as (from, to) pairs.
        std::vector<std::pair<SlotId, SlotId>>
        bridged_edges(const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const;

        SushiDSP::PrepareInfo info_;
        /// @brief Finds the upstream port the stored edge from `from` to `to` reads.
        /// @return The port, or 0 when no stored edge joins the pair.
        std::uint32_t port_of(SlotId from, SlotId to) const;

        /// @brief Holds the shape last recorded or loaded.
        std::optional<RigShape> shape_;

        /// @brief Holds the old room a loaded preset carried, until a host takes it.
        std::optional<LegacyRoom> legacy_room_;

        std::unordered_map<SlotId, Slot> slots_;
        std::vector<Edge> edges_;
        std::string ir_folder_;
        std::vector<SlotId> slot_order_;

        /// @brief Holds the measured HRTF set every room takes; empty when none loaded.
        SushiDSP::room::HrtfSet hrtf_;

        /// @brief Names the file hrtf_ was read from; "" when hrtf_ is empty.
        std::string hrtf_path_;
        SlotId next_slot_id_ = 0;
        std::unordered_map<SlotId, SushiDSP::NodeHandle> slot_to_handle_;
        SushiDSP::RtGraphSlot graph_slot_;
        SushiDSP::RigGraph* active_graph_ = nullptr;
        std::size_t dropped_parameter_changes_ = 0;
        std::atomic<RigInputSource> input_source_{RigInputSource::LiveInput};
};
