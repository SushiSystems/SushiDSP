/**************************************************************************/
/* session.hpp                                                            */
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

/**
 *  @file session.hpp
 *  @brief Declares the session: up to eight tracks, each a lane of the timeline with a rig of its own.
 *  @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "SushiDSP/core/audio_node.hpp"
#include "rig_controller.hpp"
#include "session_timeline.hpp"

/// @brief Holds a track's strip: what the mixer does to the track's rig output.
struct TrackStrip
{
        float level_db = 0.0f;     ///< The fader, in decibels.
        float pan = 0.0f;          ///< The balance, -1 hard left to +1 hard right.
        bool muted = false;        ///< True when the track is dropped from the mix.
        bool soloed = false;       ///< True when the track is one of the only ones heard.
        bool rig_bypassed = false; ///< True when the track's audio does not run through its rig.
};

/// @brief Works out the two gains a strip gives its track: the fader, the balance, and silence
/// when mute or another track's solo drops it.
/// @param strip The track's strip.
/// @param any_soloed True when any track of the session is soloed.
/// @return The left and the right linear gain.
std::array<float, 2> strip_gains(const TrackStrip& strip, bool any_soloed) noexcept;

/// @brief Owns the one timeline and the tracks on it. A track is a lane of the timeline, a rig
/// of its own built on that timeline, and a strip. The GUI thread edits it; the audio thread
/// reads it through audio(), a fixed table of atomics. A session starts with one track.
/// @see docs/agent/2026_10_13_TRACK_RIGS_AND_PLUGIN/SPEC.md, section 4
class Session
{
    public:
        /// @brief Caps the tracks a session carries, which is the lanes the timeline carries.
        static constexpr std::size_t K_MAX_TRACKS = SessionTimeline::K_MAX_TRACKS;

        /// @brief Counts the blocks a retired rig outlives before it is freed.
        static constexpr std::uint64_t K_RETIRE_BLOCKS = 2;

        /// @brief Holds what the audio thread reads of one track.
        struct AudioTrack
        {
                std::atomic<RigController*> rig{nullptr}; ///< The track's rig; null for no track.
                std::atomic<float> gain_left{0.0f};       ///< The strip's left gain.
                std::atomic<float> gain_right{0.0f};      ///< The strip's right gain.
                std::atomic<bool> rig_bypassed{false};    ///< True when the rig is passed by.
        };

        /// @brief Holds everything the audio thread reads of the session.
        struct AudioTable
        {
                std::array<AudioTrack, K_MAX_TRACKS> tracks; ///< The tracks, in lane order.
                std::atomic<std::uint32_t> selected{0};       ///< The track the live input feeds.
                std::atomic<std::uint64_t> blocks{0};         ///< Counts the blocks rendered.
        };

        /// @brief Builds a session of one empty track.
        /// @param info The sample rate, block size and channel count every rig is prepared at.
        explicit Session(const SushiDSP::PrepareInfo& info);

        ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        /// @brief Counts the tracks.
        std::size_t track_count() const noexcept;

        /// @brief Adds a track with an empty rig at the end.
        /// @param error_out Receives the reason when the session is full.
        /// @return True when the session carries one more track.
        bool add_track(std::string& error_out);

        /// @brief Removes a track, its lane and its rig; the tracks after it move down one. The
        /// last track is never removed.
        /// @param index The track; out of range is refused.
        /// @param delete_files True also deletes the files only that lane's clips read.
        /// @param error_out Receives the reason when the track is not removed.
        /// @return True when the track is gone.
        bool remove_track(std::size_t index, bool delete_files, std::string& error_out);

        /// @brief Reports which track the rig view shows and the live input feeds.
        std::size_t selected() const noexcept;

        /// @brief Selects a track; an index past the last selects the last.
        void select(std::size_t index);

        /// @brief Returns a track's rig.
        /// @pre index is below track_count().
        RigController& rig(std::size_t index);

        /// @brief Returns a track's rig, for a reader.
        /// @pre index is below track_count().
        const RigController& rig(std::size_t index) const;

        /// @brief Returns the selected track's rig.
        RigController& selected_rig();

        /// @brief Reads a track's strip.
        /// @pre index is below track_count().
        const TrackStrip& strip(std::size_t index) const;

        /// @brief Replaces a track's strip and republishes every track's gains.
        /// @param index The track; out of range does nothing.
        void set_strip(std::size_t index, const TrackStrip& strip);

        /// @brief Reads the path of the preset a track's rig was loaded from; empty when none.
        /// @pre index is below track_count().
        const std::string& preset_path(std::size_t index) const;

        /// @brief Records the path of the preset a track's rig was loaded from.
        /// @param index The track; out of range does nothing.
        void set_preset_path(std::size_t index, const std::string& path);

        /// @brief Sets the HRTF set every track's rooms read, now and for tracks added later.
        /// @param path The set's file.
        /// @param error_out Receives the reason when a rig refuses the file.
        /// @return False when any rig refused it.
        bool set_hrtf_path(const std::string& path, std::string& error_out);

        /// @brief Returns the timeline every track shares.
        SessionTimeline& timeline() noexcept;

        /// @brief Returns the timeline, for a reader.
        const SessionTimeline& timeline() const noexcept;

        /// @brief Makes the tracks as many as the timeline's lanes, after a preset changed them:
        /// new tracks get empty rigs, tracks past the lanes are retired.
        void match_timeline();

        /// @brief Frees the rigs of removed tracks once the audio thread has moved past them.
        /// Call once per GUI frame.
        void reclaim();

        /// @brief Counts the changes to which rig the rig view shows: a selection, a track added
        /// or removed, a rig replaced. A view that holds slot ids drops them when this moves.
        std::uint64_t rig_revision() const noexcept;

        /// @brief Says a rig was replaced from a preset, which moves rig_revision().
        void touch_rigs() noexcept;

        /// @brief Returns what the audio thread reads.
        AudioTable& audio() noexcept;

        /// @brief Reports the sample rate, block size and width the session was built at.
        const SushiDSP::PrepareInfo& info() const noexcept;

    private:
        /// @brief Holds one track as the GUI thread edits it.
        struct Track
        {
                std::unique_ptr<RigController> rig; ///< The track's rig.
                TrackStrip strip;                    ///< The track's strip.
                std::string preset_path;             ///< Where the rig's preset came from.
        };

        /// @brief Holds a rig taken off a track and the block count it was retired at.
        struct Retired
        {
                std::unique_ptr<RigController> rig; ///< The rig waiting to be freed.
                std::uint64_t at_block = 0;          ///< The table's block count at retirement.
        };

        /// @brief Writes every track's rig, gains and bypass into the audio table.
        void publish();

        /// @brief Appends a track with an empty rig; the lane must already exist.
        void push_track();

        /// @brief Takes a track off and retires its rig; the lane is the caller's.
        void retire(std::size_t index);

        /// @brief Stores what every rig is prepared at.
        SushiDSP::PrepareInfo info_;

        /// @brief Owns the timeline every track's rig is built on.
        std::shared_ptr<SessionTimeline> timeline_;

        /// @brief Stores the tracks, in lane order.
        std::vector<Track> tracks_;

        /// @brief Stores the rigs of removed tracks until the audio thread is past them.
        std::vector<Retired> retired_;

        /// @brief Stores the HRTF set's file a new track's rig is given; empty for none.
        std::string hrtf_path_;

        /// @brief Indexes the selected track.
        std::size_t selected_ = 0;

        /// @brief Counts the changes rig_revision() reports.
        std::uint64_t rig_revision_ = 0;

        /// @brief Stores what the audio thread reads.
        AudioTable audio_;
};
