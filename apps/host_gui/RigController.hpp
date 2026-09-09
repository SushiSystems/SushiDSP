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

#include "SushiDSP/capture/Metronome.hpp"
#include "SushiDSP/capture/MusicalClock.hpp"
#include "SushiDSP/capture/RecordingBus.hpp"
#include "SushiDSP/capture/Take.hpp"
#include "SushiDSP/capture/TrackPlayer.hpp"
#include "SushiDSP/capture/TrackTimeline.hpp"
#include "SushiDSP/capture/Transport.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"
#include "SushiDSP/room/HrtfSet.hpp"
#include "SushiDSP/room/IRoomHost.hpp"
#include "SushiDSP/room/RoomScene.hpp"

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

/// @brief Owns the authoritative rig topology - slots carrying a descriptor id and parameter
/// values, plus the edges between them - and republishes a freshly compiled SushiDSP::RigGraph
/// through an SushiDSP::RtGraphSlot on every structural edit. App-local glue, GUI thread only.
/// A slot is named by a SlotId, stable across every insert, move and remove, and unrelated to
/// the SushiDSP::NodeHandle RigGraph::add_node() assigns fresh on each rebuild.
/// @see docs/superpowers/specs/2026-09-07-stereo-tracks-design.md
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
    /// @return True when the node this slot last published implements SushiDSP::ICabinetHost.
    /// A slot added while bypassed has never had a node and reads false until it goes live.
    bool hosts_cabinet(SlotId slot) const;

    /// @brief Bounds how many scenes a second one slot's room takes; a drag inside 1 / kRoomPublishHz
    /// of the last publish keeps the newest scene for reclaim() to publish.
    static constexpr double kRoomPublishHz = 30.0;

    /// @brief Reports whether a slot's node renders a room.
    /// @param slot The slot.
    /// @return True when the node this slot last published implements SushiDSP::room::IRoomHost.
    /// A slot added while bypassed has never had a node and reads false until it goes live.
    bool hosts_room(SlotId slot) const;

    /// @brief The room node in a slot, or nullptr when that slot holds something else or is
    /// bypassed. Control thread only.
    SushiDSP::room::IRoomHost* room_host(SlotId slot);

    /// @brief The scene a slot's room carries, the stored one whether or not the last drag has
    /// reached the node yet; a slot with no scene reads the default scene.
    SushiDSP::room::RoomScene room_scene(SlotId slot) const;

    /// @brief Stores a scene for a slot and publishes it at most kRoomPublishHz times a second,
    /// keeping the newest for reclaim() when a publish lands inside the interval. A bypassed
    /// slot keeps the scene and takes it on the publish that brings it back. Control thread.
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

    /// @brief Names one run of a take on a lane: the file it reads, where on the timeline it
    /// lands, where in the take it starts and how much of the take it plays.
    struct ClipPlacement
    {
        std::string   path;              ///< Holds the file the clip's take was read from.
        std::uint64_t start_frame = 0;   ///< Holds the timeline frame the clip's first sample lands on.
        std::uint64_t take_offset = 0;   ///< Holds the first take frame the clip plays.
        std::uint64_t length_frames = 0; ///< Holds how many take frames the clip plays.
    };

    /// @brief Replaces a track's clips with one clip of a WAV file starting at frame 0. Control
    /// thread only.
    /// @param index Track index; out of range does nothing and reports failure.
    /// @param path Filesystem path to the WAV file.
    /// @param error_out Receives the failure message when the load fails.
    /// @return True when the track now holds the file's audio.
    bool load_track(std::size_t index, const std::string& path, std::string& error_out);

    /// @brief Loads a WAV file and adds it to a lane as one clip beginning at a timeline frame,
    /// sharing the take with every clip already reading the same path. Control thread only.
    /// @param track Lane index; out of range does nothing and reports failure.
    /// @param path Filesystem path to the WAV file.
    /// @param start_frame Timeline frame the clip's first sample lands on.
    /// @param error_out Receives the failure message when the file will not parse, the lane
    /// already holds SushiDSP::TrackTimeline::kMaxClips clips, or the clip would overlap one the
    /// lane already holds.
    /// @return True when the lane now holds the clip; the lane is left as it was otherwise.
    bool place_clip(std::size_t track, const std::string& path, std::uint64_t start_frame,
                    std::string& error_out);

    /// @brief Removes one clip from a lane and republishes the lane. Control thread only.
    /// @param track Lane index; out of range does nothing and reports failure.
    /// @param clip_index The clip's index in start_frame order.
    /// @param error_out Receives the failure message when either index is out of range.
    /// @return True when the lane no longer holds that clip.
    bool remove_clip(std::size_t track, std::size_t clip_index, std::string& error_out);

    /// @brief Moves one clip to a new timeline frame and republishes its lane. Control thread
    /// only.
    /// @param track Lane index; out of range does nothing and reports failure.
    /// @param clip_index The clip's index in start_frame order.
    /// @param start_frame The timeline frame the clip's first sample is to land on.
    /// @param error_out Receives the failure message when either index is out of range or the
    /// move would overlap another clip on the lane.
    /// @return True when the clip now starts there; the lane is left as it was otherwise.
    bool move_clip(std::size_t track, std::size_t clip_index, std::uint64_t start_frame,
                   std::string& error_out);

    /// @brief Reports how many clips a lane holds.
    /// @param track Lane index.
    /// @return The count, or 0 for an index out of range.
    std::size_t clip_count(std::size_t track) const;

    /// @brief Reports one of a lane's clips, in start_frame order.
    /// @param track Lane index.
    /// @param clip_index The clip's index in start_frame order.
    /// @return The clip, or a default-built one when either index is out of range.
    ClipPlacement clip(std::size_t track, std::size_t clip_index) const;

    /// @brief The tempo and the meter every bar line, beat line and click is read from.
    SushiDSP::MusicalClock& musical_clock() noexcept { return musical_clock_; }

    /// @brief The tempo and the meter, for a reader.
    const SushiDSP::MusicalClock& musical_clock() const noexcept { return musical_clock_; }

    /// @brief The click the host adds to the monitor path once the recorders have been written.
    SushiDSP::Metronome& metronome() noexcept { return metronome_; }

    /// @brief The click, for a reader.
    const SushiDSP::Metronome& metronome() const noexcept { return metronome_; }

    /// @brief Empties a playback track of every clip, and drops the re-amp take when that track
    /// was feeding it. Control thread only.
    /// @param index Track index; out of range does nothing.
    void clear_track(std::size_t index);

    /// @brief Reports whether a track holds audio.
    /// @param index Track index.
    /// @return True when the track holds at least one clip.
    bool track_is_loaded(std::size_t index) const;

    /// @brief Reports a track's display name, which defaults to its file's stem.
    /// @param index Track index.
    /// @return The name, or "" for an index out of range.
    std::string track_name(std::size_t index) const;

    /// @brief Renames a track. Control thread only.
    /// @param index Track index; out of range does nothing.
    /// @param name The new display name.
    void set_track_name(std::size_t index, const std::string& name);

    /// @brief Reports the file a track's first clip was loaded from.
    /// @param index Track index.
    /// @return The path, or "" when the track holds no clip.
    std::string track_path(std::size_t index) const;

    /// @brief Loads a track's clips a second time and publishes them at the same timeline
    /// frames, so a take recorded at a bar re-amps at that bar rather than at frame 0. Control
    /// thread only; does not change the input source.
    /// @param index Track index; an empty or out-of-range track clears the re-amp take instead.
    /// @param error_out Receives the failure message when a clip's file will not load.
    /// @return True when the re-amp leg now holds that track's arrangement.
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

    /// @brief Chooses the lane a stopped recording's dry file becomes a clip on. Control thread.
    /// @param track Lane index, or std::nullopt to have a recording place no clip; an index out
    /// of range is read as std::nullopt.
    void set_record_track(std::optional<std::size_t> track);

    /// @brief Reports the lane a stopped recording's dry file becomes a clip on.
    /// @return The lane index, or std::nullopt when a recording places no clip.
    std::optional<std::size_t> record_track() const { return record_track_; }

    /// @brief Reports the transport frame the last arm_recording() stamped, which is the frame a
    /// stopped recording's clip begins at.
    /// @return The frame, or 0 when nothing has been armed.
    std::uint64_t armed_frame() const noexcept { return armed_frame_; }

    /// @brief Opens the processed and dry files for a take, stamps the transport frame the take
    /// begins at, and puts the transport into Recording. Control thread only.
    /// @param take_name Base name both file names are built from.
    /// @param error_out Receives the failure message when either file cannot be opened.
    /// @return True when both files opened.
    bool arm_recording(const std::string& take_name, std::string& error_out);

    /// @brief Closes both take files, leaves the transport where it is, and places the dry file
    /// on record_track() as a clip beginning at armed_frame(). Control thread only.
    /// @param error_out Receives the message when the dry file will not become a clip; a null
    /// pointer discards it.
    void stop_recording(std::string* error_out = nullptr);

    /// @brief Stops both legs and the transport when either recorder has lost its file, so a
    /// disk failure never leaves a recording that only appears to continue. Control thread;
    /// call it once per GUI frame.
    /// @param error_out Receives the message when a leg failed; left untouched otherwise.
    /// @return True when this call stopped a failed recording.
    bool poll_recording(std::string& error_out);

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
    /// @brief Names a moment on the clock the room publish interval is measured on.
    using Clock = std::chrono::steady_clock;

    struct Slot
    {
        std::string                     descriptor_id;
        std::map<std::string, float>    parameters;
        std::string                     cabinet_path;
        bool                            bypassed = false;
        bool                            hosts_cabinet = false;

        /// @brief Records that the node this slot last published implements IRoomHost.
        bool                            hosts_room = false;

        /// @brief Holds the scene the slot's room renders, once one was set or loaded.
        std::optional<SushiDSP::room::RoomScene> room_scene;

        /// @brief Records that room_scene changed since the node last took it.
        bool                            room_scene_pending = false;

        /// @brief Stamps the last scene handed to the node, for the publish interval.
        std::optional<Clock::time_point> room_published_at;

        /// @brief Counts the scenes handed to the node since the slot went live.
        std::size_t                     room_publishes = 0;
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

    /// @brief Hands a slot's stored scene to its live room node when the interval since the
    /// last publish has run, or when force is set, and stamps the publish.
    /// @param error_out Receives the node's reason when it refuses the scene.
    /// @return False when the node refused the scene; true when it took it or nothing was due.
    bool publish_room_scene(SlotId slot, bool force, std::string& error_out);

    /// @brief Installs the HRTF set in place and the stored scene into a freshly built room
    /// node, adopting the node's own scene when the slot stores none.
    void apply_room_to_live_node(SlotId slot);

    /// @brief Builds a fresh RigGraph from the current slots and edges and compiles it, skipping
    /// any slot whose descriptor_id the node registry does not know.
    /// @param apply_bypass True leaves bypassed slots out and bridges around them; false adds
    /// every live slot and honours every edge, the shape a preset records.
    /// @param handles_out Receives the slot-to-handle mapping for the graph that was built.
    /// @return The compiled graph.
    /// @throws std::runtime_error if the edges exceed RigGraph::kMaxFanInOut or the graph does
    /// not compile.
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
        std::string                name;  ///< Holds the lane's display name.
        std::vector<ClipPlacement> clips; ///< Holds the lane's clips, in start_frame order.
    };

    /// @brief Returns the take a path was loaded into, loading the file the first time a clip
    /// names it so two clips of one file hold one copy of its samples.
    /// @param path Filesystem path to the WAV file.
    /// @param error_out Receives the failure message when the file will not parse.
    /// @return The borrowed take, or nullptr when the file will not parse.
    const SushiDSP::Take* take_for_path(const std::string& path, std::string& error_out);

    /// @brief Builds a lane's timeline from a clip list and publishes it, leaving the lane as it
    /// was when the list will not build.
    /// @param track Lane index; out of range does nothing and reports failure.
    /// @param clips The lane's new clips, in any order.
    /// @param error_out Receives the failure message when a file will not parse or the timeline
    /// refuses the list.
    /// @return True when the lane now holds those clips.
    bool publish_lane(std::size_t track, std::vector<ClipPlacement> clips,
                      std::string& error_out);

    /// @brief Drops every path no lane's clip names any more, so takes_by_path_ never hands out
    /// a take the library is free to collect.
    void forget_unused_takes();

    /// @brief Joins a take folder and a file name the way RecordingBus does, so a stopped
    /// recording's clip names the file the recorder actually wrote.
    /// @param folder Directory the file lives in; may be empty.
    /// @param file_name Name of the file inside that directory.
    /// @return The joined path.
    static std::string join_take_path(const std::string& folder, const std::string& file_name);

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

    /// @brief Holds the measured HRTF set every room takes; empty when none loaded.
    SushiDSP::room::HrtfSet                         hrtf_;

    /// @brief Names the file hrtf_ was read from; "" when hrtf_ is empty.
    std::string                                     hrtf_path_;
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

    /// @brief Maps a clip's path to the take player_'s library owns for it, holding only paths
    /// some lane's clip still names.
    std::unordered_map<std::string, const SushiDSP::Take*> takes_by_path_;

    /// @brief Maps a re-amped clip's path to the take reamp_player_'s own library owns for it.
    std::unordered_map<std::string, const SushiDSP::Take*> reamp_takes_by_path_;

    /// @brief Holds the tempo and the meter the ruler and the click read.
    SushiDSP::MusicalClock                          musical_clock_;

    /// @brief Holds the click the host adds after both recorder legs are written.
    SushiDSP::Metronome                             metronome_;

    /// @brief Holds the lane a stopped recording's dry file becomes a clip on.
    std::optional<std::size_t>                      record_track_;

    /// @brief Holds the name the armed take's two files were built from.
    std::string                                     record_take_name_;

    /// @brief Holds the transport frame arm_recording() stamped.
    std::uint64_t                                   armed_frame_ = 0;
};
