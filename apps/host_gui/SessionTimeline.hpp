/****************************************************************/
/* SessionTimeline.hpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/** @file SessionTimeline.hpp
 *  @brief Declares the host's track, clip and recording state and the players it drives.
 *  @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "SushiDSP/capture/Metronome.hpp"
#include "SushiDSP/capture/MusicalClock.hpp"
#include "SushiDSP/capture/RecordingBus.hpp"
#include "SushiDSP/capture/Take.hpp"
#include "SushiDSP/capture/TrackPlayer.hpp"
#include "SushiDSP/capture/TrackTimeline.hpp"
#include "SushiDSP/capture/Transport.hpp"
#include "SushiDSP/core/IAudioNode.hpp"

/// @brief Owns the host's arrangement - the playback lanes, their clips, the re-amp leg, the
/// tempo and the take recorders - and keeps the audible players in step with it. App-local
/// glue, GUI thread only. It knows nothing of the rig's topology.
/// @see docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md
class SessionTimeline
{
public:
    /// @brief Names one run of a take on a lane: the file it reads, where on the timeline it
    /// lands, where in the take it starts and how much of the take it plays.
    struct ClipPlacement
    {
        std::string   path;              ///< Holds the file the clip's take was read from.
        std::uint64_t start_frame = 0;   ///< Holds the timeline frame the clip's first sample lands on.
        std::uint64_t take_offset = 0;   ///< Holds the first take frame the clip plays.
        std::uint64_t length_frames = 0; ///< Holds how many take frames the clip plays.
    };

    /// @brief A lane's colour, packed as 0xRRGGBBAA, which is the form the view draws with.
    using TrackColor = std::uint32_t;

    /// @brief Caps how many lanes the arrangement carries, which is what the player mixes.
    static constexpr std::size_t kMaxTracks = SushiDSP::TrackPlayer::kMaxTracks;

    explicit SessionTimeline(const SushiDSP::PrepareInfo& info);

    /// @brief Reports how many playback lanes the arrangement carries.
    std::size_t track_count() const noexcept { return tracks_.size(); }

    /// @brief Appends an empty lane, named default_track_name() and coloured palette_color() for
    /// its new index, at index track_count() - 1. Control thread only.
    /// @param error_out Receives the failure message when the arrangement already carries
    /// kMaxTracks lanes.
    /// @return True when the arrangement now carries one more lane.
    bool add_track(std::string& error_out);

    /// @brief Removes a lane and leaves every file its clips read on disk. Every lane after it
    /// moves down one index, so a caller holding an index above the removed one must lower it by
    /// one. Control thread only.
    /// @param index Lane index; out of range does nothing and reports failure.
    /// @param error_out Receives the failure message when the index is out of range or a lane
    /// that moved down could not be republished.
    /// @return True when the arrangement no longer carries that lane.
    bool remove_track(std::size_t index, std::string& error_out);

    /// @brief Removes a lane the way remove_track() does and then deletes from disk each file
    /// only that lane's clips read, leaving a file another lane still reads alone. Every lane
    /// after it moves down one index. Control thread only; it touches the filesystem.
    /// @param index Lane index; out of range does nothing and reports failure.
    /// @param error_out Receives the failure message when the index is out of range, a lane that
    /// moved down could not be republished, or a file could not be deleted; the lane is gone
    /// either way once the index was in range.
    /// @return True when the lane is gone and every file it alone read is deleted.
    bool remove_track_and_files(std::size_t index, std::string& error_out);

    /// @brief Reports a lane's colour.
    /// @param index Lane index.
    /// @return The colour, or 0 for an index out of range.
    TrackColor track_color(std::size_t index) const;

    /// @brief Recolours a lane. Control thread only.
    /// @param index Lane index; out of range does nothing.
    /// @param color The new colour, packed as 0xRRGGBBAA.
    void set_track_color(std::size_t index, TrackColor color);

    /// @brief Reports the colour the palette gives an index, cycling once the palette runs out.
    /// @param index Lane index; any value is answered.
    /// @return The packed colour.
    static TrackColor palette_color(std::size_t index) noexcept;

    /// @brief Reports how many colours the palette holds before it repeats.
    static std::size_t palette_size() noexcept;

    /// @brief Reports the name add_track() gives a lane at an index, which is "Track" and the
    /// one-based index.
    /// @param index Lane index.
    /// @return The name.
    static std::string default_track_name(std::size_t index);

    /// @brief Reclaims the takes both players have retired. Control thread only; call once per
    /// GUI frame.
    void reclaim();

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
    /// @param error_out Receives the failure message; apps/host_gui/README.md lists the
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

    /// @brief Builds a lane's timeline from a clip list and publishes it, leaving the lane as it
    /// was when the list will not build.
    /// @param track Lane index; out of range does nothing and reports failure.
    /// @param clips The lane's new clips, in any order.
    /// @param error_out Receives the failure message when a file will not parse or the timeline
    /// refuses the list.
    /// @return True when the lane now holds those clips.
    bool publish_lane(std::size_t track, std::vector<ClipPlacement> clips,
                      std::string& error_out);

    /// @brief The tempo and the meter every bar line, beat line and click is read from.
    SushiDSP::MusicalClock& musical_clock() noexcept { return musical_clock_; }

    /// @brief The tempo and the meter, for a reader.
    const SushiDSP::MusicalClock& musical_clock() const noexcept { return musical_clock_; }

    /// @brief The click the host adds to the monitor path once the recorders have been written.
    SushiDSP::Metronome& metronome() noexcept { return metronome_; }

    /// @brief The click, for a reader.
    const SushiDSP::Metronome& metronome() const noexcept { return metronome_; }

    /// @brief Empties a playback track of every clip, drops its name, returns it to the palette's
    /// colour for its index, and drops the re-amp take when that track was feeding it. Control
    /// thread only.
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

    /// @brief Strips a path down to its file name without an extension.
    /// @param path The path to reduce.
    /// @return The stem, or "" for a path that ends in a separator.
    static std::string path_stem(const std::string& path);

private:
    /// @brief One playback track's control-thread metadata; the audible state lives in player_.
    struct TrackInfo
    {
        std::string                name;      ///< Holds the lane's display name.
        TrackColor                 color = 0; ///< Holds the lane's colour, packed as 0xRRGGBBAA.
        std::vector<ClipPlacement> clips;     ///< Holds the lane's clips, in start_frame order.
    };

    /// @brief Drops a lane from the list, republishes every lane that moved down onto the player
    /// lane it now occupies, silences the lane the list no longer reaches, and lowers the re-amp
    /// and record indices that pointed above it.
    /// @param index Lane index; out of range does nothing and reports failure.
    /// @param error_out Receives the first failure message a republished lane produced.
    /// @return True when every lane that moved down was republished.
    bool erase_track(std::size_t index, std::string& error_out);

    /// @brief Lists the files a lane's clips read that no other lane's clips read.
    /// @param index Lane index.
    /// @return The paths, each once; empty for an index out of range.
    std::vector<std::string> files_only_track_reads(std::size_t index) const;

    /// @brief Returns the take a path was loaded into, loading the file the first time a clip
    /// names it so two clips of one file hold one copy of its samples.
    /// @param path Filesystem path to the WAV file.
    /// @param error_out Receives the failure message when the file will not parse.
    /// @return The borrowed take, or nullptr when the file will not parse.
    const SushiDSP::Take* take_for_path(const std::string& path, std::string& error_out);

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

    SushiDSP::Transport                             transport_;
    SushiDSP::TrackPlayer                           player_;
    SushiDSP::TrackPlayer                           reamp_player_;
    SushiDSP::RecordingBus                          recording_bus_;
    std::vector<TrackInfo>                          tracks_;
    std::optional<std::size_t>                      reamp_track_;
    std::string                                     take_folder_;

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
