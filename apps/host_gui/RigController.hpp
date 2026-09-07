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

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "SushiDSP/capture/RecordingBus.hpp"
#include "SushiDSP/capture/Take.hpp"
#include "SushiDSP/capture/TrackPlayer.hpp"
#include "SushiDSP/capture/Transport.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"

#include "ui/Geometry.hpp"

/// @brief Names the signal the host assembles into the rig's input buffer. Track and Both are
/// what re-amping is made of - a dry take driven through the rig instead of, or on top of, the
/// live instrument. See docs/superpowers/specs/2026-09-07-stereo-tracks-design.md section 6.
enum class RigInputSource : std::uint32_t
{
    LiveInput = 0,
    Track = 1,
    Both = 2
};

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
    /// @brief The slot identity type, owned by ui::Geometry so the layout layer can name it
    /// without including this header.
    using SlotId = ui::SlotId;

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
    /// gap. This bridging is specific to edges_, the topology this controller keeps: publish()
    /// may separately drop an unrelated slot whose descriptor_id is unknown to the node registry,
    /// and that slot's edges are erased from edges_ without bridging, though the graph published
    /// on that same call does bridge across it, because bridged_edges() cannot tell a slot the
    /// registry rejected from a bypassed one (unreachable in practice — registry-driven edits and
    /// rig_preset's own validation both keep an unknown type from ever reaching this controller).
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
    /// touches the live node directly: the value is stored in this controller's own slot state
    /// and queued on the active RigGraph's lock-free parameter ring, where the audio thread
    /// applies it at the top of its next process() call. If the ring is full the change is
    /// dropped and dropped_parameter_changes() is incremented; the call never blocks.
    void set_parameter(SlotId slot, const char* param_id, float value);

    /// @brief Reads this controller's stored value for a parameter on `slot`.
    float get_parameter(SlotId slot, const char* param_id) const;

    /// @brief Installs a cabinet impulse response on the amplifier at `slot`. Loads the file at
    /// the controller's prepared sample rate, hands it to the live node when the slot has one,
    /// and remembers the path so a later rebuild (or a preset save) can reproduce it. A bypassed
    /// slot has no live node, so it keeps the path and receives the cabinet on the publish that
    /// brings it back - and the node, not the descriptor id, is what refuses it there.
    /// @return True if `slot`'s node hosts a cabinet (SushiDSP::ICabinetHost) and took the IR, or
    /// if the slot is bypassed and the path was stored; false for an unknown slot or a live node
    /// that hosts no cabinet.
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
    /// @return True when the node this slot last published implements SushiDSP::ICabinetHost.
    /// A slot added while bypassed has never had a node and reads false until it goes live.
    bool hosts_cabinet(SlotId slot) const;

    /// @brief Lists the ids of every slot with an edge into `slot`, in connect() call order.
    std::vector<SlotId> upstream_of(SlotId slot) const;

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


    /// @brief Reports the sample rate every take, the transport and the rig were prepared at.
    /// @return The rate in Hz.
    double sample_rate() const { return info_.sample_rate; }

    /// @brief Reports how many channels the rig's terminal node is adapted to.
    /// @return The host output width the published graph writes.
    std::uint32_t output_channels() const { return info_.channels; }

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

    /// @brief Reads a WAV file into a playback track and remembers where it came from. Control
    /// thread only.
    /// @param index Track index; out of range does nothing and reports failure.
    /// @param path Filesystem path to the WAV file.
    /// @param error_out Receives the failure message when the load fails.
    /// @return True when the track now holds the file's audio.
    bool load_track(std::size_t index, const std::string& path, std::string& error_out);

    /// @brief Empties a playback track and forgets its file, and drops the re-amp take when that
    /// track was feeding it. Control thread only.
    /// @param index Track index; out of range does nothing.
    void clear_track(std::size_t index);

    /// @brief Reports whether a track holds audio.
    /// @param index Track index.
    /// @return True when load_track() has succeeded on it and clear_track() has not run since.
    bool track_is_loaded(std::size_t index) const;

    /// @brief Reports a track's display name, which defaults to its file's stem.
    /// @param index Track index.
    /// @return The name, or "" for an index out of range.
    std::string track_name(std::size_t index) const;

    /// @brief Renames a track. Control thread only.
    /// @param index Track index; out of range does nothing.
    /// @param name The new display name.
    void set_track_name(std::size_t index, const std::string& name);

    /// @brief Reports the file a track was loaded from.
    /// @param index Track index.
    /// @return The path, or "" when the track holds nothing.
    std::string track_path(std::size_t index) const;

    /// @brief Loads a track's file a second time and publishes it as the take the rig's input is
    /// assembled from. Control thread only; does not change the input source.
    /// @param index Track index; an empty or out-of-range track clears the re-amp take instead.
    /// @param error_out Receives the failure message when the load fails.
    /// @return True when the re-amp take now holds that track's audio.
    bool set_reamp_track(std::size_t index, std::string& error_out);

    /// @brief Drops the re-amp take, so the re-amp leg contributes silence. Control thread only.
    void clear_reamp_track();

    /// @brief Reports which track feeds the re-amp leg.
    /// @return The track index, or std::nullopt when no take is loaded there.
    std::optional<std::size_t> reamp_track() const { return reamp_track_; }

    /// @brief Remembers the folder recorded takes are written into. Saved with the preset.
    /// @param path The folder.
    void set_take_folder(const std::string& path);

    /// @brief The remembered take folder, or "" when the preset carried none.
    std::string take_folder() const;

    /// @brief Opens the processed and dry files for a take and puts the transport into Recording.
    /// Control thread only.
    /// @param take_name Base name both file names are built from.
    /// @param error_out Receives the failure message when either file cannot be opened.
    /// @return True when both files opened.
    bool arm_recording(const std::string& take_name, std::string& error_out);

    /// @brief Closes both take files and leaves the transport where it is. Control thread only.
    void stop_recording();

    /// @brief The shared sample clock the audio callback advances once per block.
    SushiDSP::Transport& transport() noexcept { return transport_; }

    /// @brief The shared sample clock, for a reader.
    const SushiDSP::Transport& transport() const noexcept { return transport_; }

    /// @brief The playback tracks the callback mixes into the rig's output.
    SushiDSP::TrackPlayer& player() noexcept { return player_; }

    /// @brief The playback tracks, for a reader.
    const SushiDSP::TrackPlayer& player() const noexcept { return player_; }

    /// @brief The single-track player the callback adds into the rig's input buffer.
    SushiDSP::TrackPlayer& reamp_player() noexcept { return reamp_player_; }

    /// @brief The pair of recorders the callback writes both legs of a take into.
    SushiDSP::RecordingBus& recording_bus() noexcept { return recording_bus_; }

    /// @brief The pair of recorders, for a reader.
    const SushiDSP::RecordingBus& recording_bus() const noexcept { return recording_bus_; }

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
    struct Slot
    {
        std::string                     descriptor_id;
        std::map<std::string, float>    parameters;
        std::string                     cabinet_path;
        bool                            bypassed = false;
        bool                            hosts_cabinet = false;
    };
    struct Edge { SlotId from; SlotId to; };

    /// @brief The slot a forward walk out of a splitter's branch stopped on.
    struct BranchEnd
    {
        SlotId slot;
        bool   is_join;
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

    /// @brief Builds a fresh RigGraph from the current slots and edges and compiles it. Const: a
    /// slot whose descriptor_id the node registry does not know is skipped rather than erased,
    /// so handles_out names exactly the slots that reached the graph.
    /// @param apply_bypass True to leave bypassed slots out and bridge around them; false to add
    /// every live slot and honour every edge, which is the shape a preset records.
    /// @param handles_out Receives the slot-to-handle mapping for the graph that was built.
    /// @return The compiled graph.
    /// @throws std::runtime_error if the resolved edges would exceed RigGraph::kMaxFanInOut, or
    /// if the graph does not compile.
    std::unique_ptr<SushiDSP::RigGraph> build_graph(bool apply_bypass,
        std::unordered_map<SlotId, SushiDSP::NodeHandle>& handles_out) const;

    /// @brief Lists every edge whose endpoints both reached the graph. Duplicate edges are
    /// dropped, so a preset that names one twice still builds one edge.
    /// @param live The slot-to-handle mapping for the graph being built.
    /// @return The edges, as (from, to) pairs.
    std::vector<std::pair<SlotId, SlotId>> direct_edges(
        const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const;

    /// @brief Serializes the slots and edges through SushiDSP::serialize_rig_to_json(), with no
    /// rig-level key of this controller's own. save_to_json() adds those on top.
    /// @return The `"nodes"` document.
    /// @throws std::runtime_error if the unbypassed graph does not compile.
    std::string serialize_nodes() const;

    /// @brief Lists every edge with bypassed slots collapsed away: an edge into a bypassed slot
    /// is followed through to each live slot reachable past it, so a run of bypassed slots
    /// bridges in one step. Duplicate edges are dropped.
    /// @param live The slot-to-handle mapping for the graph being built.
    /// @return The bridged edges, as (from, to) pairs.
    std::vector<std::pair<SlotId, SlotId>> bridged_edges(
        const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const;

    /// @brief One playback track's control-thread metadata; the audible state lives in player_.
    struct TrackInfo
    {
        std::string name;
        std::string path;
        bool        loaded = false;
    };

    /// @brief Reads a WAV file into a take, or reports why it could not be read.
    /// @param path Filesystem path to the WAV file.
    /// @param error_out Receives the failure message when the load fails.
    /// @return The take, or nullptr on failure.
    static std::unique_ptr<SushiDSP::Take> load_take(const std::string& path,
                                                     std::string& error_out);

    /// @brief Strips a path down to its file name without an extension.
    /// @param path The path to reduce.
    /// @return The stem, or "" for a path that ends in a separator.
    static std::string path_stem(const std::string& path);

    SushiDSP::PrepareInfo                          info_;
    std::unordered_map<SlotId, Slot>               slots_;
    std::vector<Edge>                              edges_;
    std::string                                     ir_folder_;
    std::vector<SlotId>                             slot_order_;
    SlotId                                          next_slot_id_ = 0;
    std::unordered_map<SlotId, SushiDSP::NodeHandle> slot_to_handle_;
    SushiDSP::RtGraphSlot                          graph_slot_;
    SushiDSP::RigGraph*                            active_graph_ = nullptr;
    std::size_t                                     dropped_parameter_changes_ = 0;
    SushiDSP::Transport                             transport_;
    SushiDSP::TrackPlayer                           player_;
    SushiDSP::TrackPlayer                           reamp_player_;
    SushiDSP::RecordingBus                          recording_bus_;
    std::array<TrackInfo, SushiDSP::TrackPlayer::kMaxTracks> tracks_;
    std::optional<std::size_t>                      reamp_track_;
    std::string                                     take_folder_;
    std::atomic<RigInputSource>                     input_source_{ RigInputSource::LiveInput };
};
