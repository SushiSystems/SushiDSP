/**************************************************************************/
/* rig_controller.hpp                                                     */
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

/** @file rig_controller.hpp
 *  @brief Declares the host's rig facade over the slot graph, the timeline and the preset codec.
 *  @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "SushiDSP/core/audio_node.hpp"
#include "SushiDSP/core/rig_graph.hpp"
#include "SushiDSP/core/rt_graph_slot.hpp"
#include "SushiDSP/room/room_arrangement.hpp"
#include "SushiDSP/room/room_host.hpp"
#include "SushiDSP/room/room_scene.hpp"

#include "rig_persistence.hpp"
#include "session_timeline.hpp"
#include "slot_graph.hpp"

/// @brief The host's single handle on a rig: it holds the slot graph, the session timeline and
/// the preset codec, and forwards to whichever of the three owns the call. GUI thread only.
/// A slot is named by a SlotId, stable across every insert, move and remove, and unrelated to
/// the SushiDSP::NodeHandle RigGraph::add_node() assigns fresh on each rebuild.
/// @see docs/archive/superpowers/specs/2026-09-07-stereo-tracks-design.md
class RigController
{
    public:
        /// @brief The slot identity type, owned by ui::Geometry so the layout layer can name it
        /// without including this header.
        using SlotId = SlotGraph::SlotId;

        /// @brief Holds what a slot needs to be rebuilt: descriptor, parameters, cabinet, bypass, room.
        using SlotSnapshot = SlotGraph::SlotSnapshot;

        /// @brief Names one run of a take on a lane: the file it reads, where on the timeline it
        /// lands, where in the take it starts and how much of the take it plays.
        using ClipPlacement = SessionTimeline::ClipPlacement;

        explicit RigController(const SushiDSP::PrepareInfo& info);

        /// @brief Builds a rig on a timeline another owner shares with it, so several rigs read
        /// one arrangement, one transport and one set of recorders.
        /// @param info The sample rate, block size and channel count the rig is prepared at.
        /// @param session The shared timeline; never null.
        RigController(const SushiDSP::PrepareInfo& info, std::shared_ptr<SessionTimeline> session);

        /// @brief Returns the timeline this rig reads, shared or its own.
        const std::shared_ptr<SessionTimeline>& session() const noexcept
        {
            return session_owner_;
        }

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
        /// @param publish_now Pass false to batch several structural edits into one rebuild; call
        /// publish() once when done.
        void connect(SlotId from, SlotId to, bool publish_now = true,
                     std::uint32_t from_port = 0);

        /// @brief Removes a single edge, if present, and republishes.
        /// @param publish_now Pass false to batch several structural edits into one rebuild; call
        /// publish() once when done.
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
        /// call it explicitly after a batch of edits made with publish_now = false.
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
        /// touches the live node directly: the value is stored in the slot graph's own state and
        /// queued on the active RigGraph's lock-free parameter ring, where the audio thread applies
        /// it at the top of its next process() call. If the ring is full the change is dropped and
        /// dropped_parameter_changes() is incremented; the call never blocks.
        void set_parameter(SlotId slot, const char* param_id, float value);

        /// @brief Chooses the host input channel a slot reads when it is a root of the graph. Publishes
        /// when publish_now is true; otherwise the next publish() picks it up.
        /// @param slot The slot to bind.
        /// @param channel The host input channel, below RigGraph::K_MAX_CHANNELS.
        /// @param publish_now Publishes the graph when true.
        void set_input_channel(SlotId slot, std::uint32_t channel, bool publish_now = true);

        /// @brief Reads the stored value for a parameter on `slot`.
        float get_parameter(SlotId slot, const char* param_id) const;

        /// @brief Installs a cabinet impulse response on the amplifier at `slot`, loading it at the
        /// prepared sample rate and remembering the path so a rebuild or a preset reproduces it. A
        /// bypassed slot keeps the path and takes the cabinet on the publish that brings it back.
        /// @param slot The slot to install the cabinet on.
        /// @param path The impulse-response file.
        /// @return True when the node took the impulse response, or the bypassed slot stored it.
        /// @throws std::runtime_error if the file cannot be opened or parsed.
        bool set_cabinet(SlotId slot, const std::string& path);

        /// @brief Reclaims retired graphs, the retired cabinets of every live node that hosts one,
        /// and the takes both players have retired. Control thread only; call once per GUI frame.
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
        static constexpr double K_ROOM_PUBLISH_HZ = SlotGraph::K_ROOM_PUBLISH_HZ;

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
        /// a second, keeping the newest for reclaim() when a publish lands inside the interval.
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
        /// arrangement from one scene and publishes as set_room_arrangement() does. Microphones
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

        /// @brief Records the rig's shape: its stages, its DI choice and its hidden slots.
        /// @param shape The shape, naming this rig's slot ids.
        void set_shape(const RigShape& shape);

        /// @brief Reads the shape last recorded, or loaded from a preset.
        /// @return The shape, or nullopt for a rig nothing has laid out.
        const std::optional<RigShape>& shape() const noexcept;

        /// @brief Hands over the room a preset written before the cabinet section carried, once.
        /// @return The old room, or nullopt when there is none left to hand over.
        std::optional<LegacyRoom> take_legacy_room();

        /// @brief Every edge the rig holds with the upstream port it reads, in stored order.
        /// @return The records.
        std::vector<SlotGraph::EdgeRecord> edge_records() const;

        /// @brief Captures what a live slot needs to be rebuilt.
        /// @param slot The slot to capture.
        /// @return The snapshot, or std::nullopt when the slot is not live.
        std::optional<SlotSnapshot> snapshot(SlotId slot) const;

        /// @brief Re-adds a slot from a snapshot, unconnected, restoring its parameters over the
        /// seeded defaults, its cabinet path, its bypass flag and its room scene, applying them on
        /// the publish that builds the node - the way load_from_json() does today.
        /// @param snapshot The captured slot state.
        /// @param publish_now Pass false to batch this with other structural edits; call publish()
        /// once when done.
        /// @return The new slot's id.
        SlotId restore(const SlotSnapshot& snapshot, bool publish_now = true);

        /// @brief Drops every edge the rig holds, leaving every slot live but unconnected.
        /// @param publish_now Pass false to batch this with other structural edits; call publish()
        /// once when done.
        void clear_edges(bool publish_now = true);

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
        double sample_rate() const
        {
            return info_.sample_rate;
        }

        /// @brief Reports how many channels the rig's terminal node is adapted to.
        /// @return The host output width the published graph writes.
        std::uint32_t output_channels() const
        {
            return info_.channels;
        }

        /// @brief Chooses the signal the host feeds the rig. Control thread; the audio thread reads
        /// the choice on its next block.
        /// @param source The live instrument, the re-amp take, or both summed.
        void set_input_source(RigInputSource source);

        /// @brief Reports the signal the host feeds the rig. Either thread; real-time safe.
        /// @return The current choice.
        RigInputSource input_source() const noexcept
        {
            return graph_.input_source();
        }

        /// @brief Replaces a track's clips with one clip of a WAV file starting at frame 0. Control
        /// thread only.
        /// @param index Track index; out of range does nothing and reports failure.
        /// @param path Filesystem path to the WAV file.
        /// @param error_out Receives the failure message when the load fails.
        /// @return True when the track now holds the file's audio.
        bool load_track(std::size_t index, const std::string& path, std::string& error_out);

        /// @brief Loads a WAV file and adds it to a lane as one clip beginning at a timeline frame.
        /// @param track Lane index; out of range does nothing and reports failure.
        /// @param path Filesystem path to the WAV file.
        /// @param start_frame Timeline frame the clip's first sample lands on.
        /// @param error_out Receives the failure message; applications/host_gui/README.md lists the
        /// refusals.
        /// @return True when the lane now holds the clip; the lane is left as it was otherwise.
        /// @pre Called on the control thread.
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
        SushiDSP::MusicalClock& musical_clock() noexcept
        {
            return session_.musical_clock();
        }

        /// @brief The tempo and the meter, for a reader.
        const SushiDSP::MusicalClock& musical_clock() const noexcept
        {
            return session_.musical_clock();
        }

        /// @brief The click the host adds to the monitor path once the recorders have been written.
        SushiDSP::Metronome& metronome() noexcept
        {
            return session_.metronome();
        }

        /// @brief The click, for a reader.
        const SushiDSP::Metronome& metronome() const noexcept
        {
            return session_.metronome();
        }

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

        /// @brief Counts the tracks the arrangement holds.
        /// @return The track count.
        std::size_t track_count() const;

        /// @brief Appends an empty track at the end of the arrangement. Control thread only.
        /// @param error_out Receives the refusal when the arrangement is already full.
        /// @return True when a track was added; the new track's index is track_count() - 1.
        bool add_track(std::string& error_out);

        /// @brief Removes a track, leaving every file it read on disk. Control thread only.
        /// @param index Track index; the tracks after it move down by one.
        /// @param error_out Receives the refusal when the index names no track.
        /// @return True when the track was removed.
        bool remove_track(std::size_t index, std::string& error_out);

        /// @brief Removes a track and deletes the files only it was reading. Control thread only.
        /// @param index Track index; the tracks after it move down by one.
        /// @param error_out Receives the refusal, including a file that would not delete.
        /// @return True when the track and its files are gone.
        bool remove_track_and_files(std::size_t index, std::string& error_out);

        /// @brief Reports the colour a track draws in.
        /// @param index Track index.
        /// @return The colour as 0xRRGGBBAA, or the palette's first entry for an index out of range.
        SessionTimeline::TrackColor track_color(std::size_t index) const;

        /// @brief Sets a track's colour. Control thread only.
        /// @param index Track index; out of range does nothing.
        /// @param color The colour as 0xRRGGBBAA.
        void set_track_color(std::size_t index, SessionTimeline::TrackColor color);

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
        std::optional<std::size_t> reamp_track() const
        {
            return session_.reamp_track();
        }

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
        std::optional<std::size_t> record_track() const
        {
            return session_.record_track();
        }

        /// @brief Reports the transport frame the last arm_recording() stamped, which is the frame a
        /// stopped recording's clip begins at.
        /// @return The frame, or 0 when nothing has been armed.
        std::uint64_t armed_frame() const noexcept
        {
            return session_.armed_frame();
        }

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
        SushiDSP::Transport& transport() noexcept
        {
            return session_.transport();
        }

        /// @brief The shared sample clock, for a reader.
        const SushiDSP::Transport& transport() const noexcept
        {
            return session_.transport();
        }

        /// @brief The playback tracks the callback mixes into the rig's output.
        SushiDSP::TrackPlayer& player() noexcept
        {
            return session_.player();
        }

        /// @brief The playback tracks, for a reader.
        const SushiDSP::TrackPlayer& player() const noexcept
        {
            return session_.player();
        }

        /// @brief The single-track player the callback adds into the rig's input buffer.
        SushiDSP::TrackPlayer& reamp_player() noexcept
        {
            return session_.reamp_player();
        }

        /// @brief The pair of recorders the callback writes both legs of a take into.
        SushiDSP::RecordingBus& recording_bus() noexcept
        {
            return session_.recording_bus();
        }

        /// @brief The pair of recorders, for a reader.
        const SushiDSP::RecordingBus& recording_bus() const noexcept
        {
            return session_.recording_bus();
        }

        /// @brief Serializes the current rig to JSON via SushiDSP::serialize_rig_to_json(). Cabinet
        /// IR is included via the `"cabinet_ir"` JSON field. Every slot is written, a bypassed one
        /// included, and a bypassed slot carries `"bypassed": true`. A non-empty ir_folder() is
        /// written as the rig-level `"ir_folder"` key beside `"nodes"`.
        /// @throws std::runtime_error if the unbypassed graph does not compile, which is what the
        /// preset records; a caller writing to a file must serialize before it truncates the file.
        std::string save_to_json() const;

        /// @brief Serializes the rig alone, with none of the session beside it.
        /// @return The rig as preset text a track or a plugin carries.
        /// @throws std::runtime_error as save_to_json() does.
        std::string save_rig_to_json() const;

        /// @brief Replaces the rig from preset text and leaves the lanes, the loop and the tempo
        /// as they are.
        /// @param json_text A preset; its session keys are not read.
        /// @throws std::runtime_error as load_from_json() does.
        void load_rig_from_json(const std::string& json_text);

        /// @brief Replaces the entire rig from JSON via SushiDSP::deserialize_rig_from_json() and
        /// republishes. Fresh SlotIds are assigned in the parsed graph's node order — preset-local
        /// continuity across a save/load round trip is not preserved (the JSON schema was never
        /// designed to carry a GUI-local SlotId, only a preset-local integer id).
        /// @throws std::runtime_error if json_text is malformed (same contract as
        /// SushiDSP::deserialize_rig_from_json()). A preset with no `"ir_folder"` key loads with an
        /// empty folder.
        void load_from_json(const std::string& json_text);

    private:
        /// @brief Holds the rate, block size and width every part of the rig was prepared at.
        SushiDSP::PrepareInfo info_;

        /// @brief Owns the slots, the edges and the graph published to the audio thread.
        SlotGraph graph_;

        /// @brief Keeps the timeline alive: the one this rig made, or the one it was handed.
        std::shared_ptr<SessionTimeline> session_owner_;

        /// @brief Names the timeline every call forwards to: the lanes, the clips, the re-amp leg
        /// and the take recorders.
        SessionTimeline& session_;

        /// @brief Reads and writes the other two as a preset; declared last so both outlive it.
        RigPersistence persistence_;
};
