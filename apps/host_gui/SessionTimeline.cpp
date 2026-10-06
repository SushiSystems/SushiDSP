/**************************************************************************/
/* SessionTimeline.cpp                                                    */
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

/** @file SessionTimeline.cpp
 *  @brief Implements the host's lanes, clips, re-amp leg and take recording.
 *  @author Mustafa Garip
 */

#include "SessionTimeline.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iterator>
#include <memory>
#include <set>
#include <system_error>
#include <utility>

namespace
{
    /// @brief Holds the colours a new lane takes, in the order add_track() hands them out.
    constexpr SessionTimeline::TrackColor kTrackPalette[] = {
        0x4E9BE8FFu, 0x59C08AFFu, 0xE8B14EFFu, 0xE0655EFFu,
        0xA77BE8FFu, 0x4EC8C8FFu, 0xE07FB4FFu, 0x9AA7B4FFu,
    };
}

SessionTimeline::SessionTimeline(const SushiDSP::PrepareInfo& info)
    : recording_bus_(std::max(1u, info.channels), info.sample_rate)
    , tracks_(kMaxTracks)
    , metronome_(info.sample_rate)
{
    for (std::size_t index = 0; index < tracks_.size(); ++index)
    {
        tracks_[index].color = palette_color(index);
    }
    musical_clock_.set_sample_rate(info.sample_rate);
    // Both players are prepared here, before any speed is set; see apps/host_gui/README.md.
    player_.prepare(info.max_block);
    reamp_player_.prepare(info.max_block);
}

void SessionTimeline::reclaim()
{
    player_.reclaim();
    reamp_player_.reclaim();
}

std::unique_ptr<SushiDSP::Take> SessionTimeline::load_take(const std::string& path,
                                                           std::string& error_out)
{
    error_out.clear();
    try
    {
        return std::make_unique<SushiDSP::Take>(SushiDSP::Take::load(path));
    }
    catch (const std::exception& error)
    {
        error_out = error.what();
        return nullptr;
    }
}

std::string SessionTimeline::path_stem(const std::string& path)
{
    const std::size_t cut = path.find_last_of("/\\");
    const std::string name = cut == std::string::npos ? path : path.substr(cut + 1);
    const std::size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

std::string SessionTimeline::join_take_path(const std::string& folder,
                                            const std::string& file_name)
{
    if (folder.empty())
    {
        return file_name;
    }
    const char last = folder.back();
    if (last == '/' || last == '\\')
    {
        return folder + file_name;
    }
    return folder + '/' + file_name;
}

const SushiDSP::Take* SessionTimeline::take_for_path(const std::string& path,
                                                     std::string&       error_out)
{
    error_out.clear();
    const auto found = takes_by_path_.find(path);
    if (found != takes_by_path_.end())
    {
        return found->second;
    }

    std::unique_ptr<SushiDSP::Take> take = load_take(path, error_out);
    if (take == nullptr)
    {
        return nullptr;
    }

    const SushiDSP::Take* borrowed = player_.takes().insert(std::move(take));
    takes_by_path_[path] = borrowed;
    return borrowed;
}

void SessionTimeline::forget_unused_takes()
{
    std::set<std::string> named;
    for (const TrackInfo& track : tracks_)
    {
        for (const ClipPlacement& placement : track.clips)
        {
            named.insert(placement.path);
        }
    }

    for (auto entry = takes_by_path_.begin(); entry != takes_by_path_.end();)
    {
        entry = named.count(entry->first) == 0 ? takes_by_path_.erase(entry) : std::next(entry);
    }
}

bool SessionTimeline::publish_lane(std::size_t track, std::vector<ClipPlacement> clips,
                                   std::string& error_out)
{
    error_out.clear();
    if (track >= tracks_.size())
    {
        error_out = "track index out of range";
        return false;
    }

    std::sort(clips.begin(), clips.end(),
              [](const ClipPlacement& left, const ClipPlacement& right)
              { return left.start_frame < right.start_frame; });

    std::vector<SushiDSP::Clip> lane;
    lane.reserve(clips.size());
    for (ClipPlacement& placement : clips)
    {
        const SushiDSP::Take* take = take_for_path(placement.path, error_out);
        if (take == nullptr)
        {
            forget_unused_takes();
            return false;
        }

        // A zero length means the take's whole remainder; see apps/host_gui/README.md.
        const std::uint64_t frames = static_cast<std::uint64_t>(take->frames());
        placement.take_offset = std::min(placement.take_offset, frames);
        const std::uint64_t available = frames - placement.take_offset;
        placement.length_frames = placement.length_frames == 0
            ? available
            : std::min(placement.length_frames, available);

        lane.push_back(SushiDSP::Clip{ take, placement.start_frame, placement.take_offset,
                                       placement.length_frames });
    }

    std::unique_ptr<SushiDSP::TrackTimeline> timeline;
    try
    {
        timeline = std::make_unique<SushiDSP::TrackTimeline>(player_.takes(), lane);
    }
    catch (const std::exception& error)
    {
        error_out = error.what();
        forget_unused_takes();
        return false;
    }

    tracks_[track].clips = std::move(clips);
    player_.set_timeline(track, std::move(timeline));
    forget_unused_takes();
    return true;
}

bool SessionTimeline::place_clip(std::size_t track, const std::string& path,
                                 std::uint64_t start_frame, std::string& error_out)
{
    error_out.clear();
    if (track >= tracks_.size())
    {
        error_out = "track index out of range";
        return false;
    }
    if (tracks_[track].clips.size() >= SushiDSP::TrackTimeline::kMaxClips)
    {
        error_out = "lane " + std::to_string(track) + " already holds "
            + std::to_string(SushiDSP::TrackTimeline::kMaxClips) + " clips";
        return false;
    }

    std::vector<ClipPlacement> clips = tracks_[track].clips;
    ClipPlacement              placement;
    placement.path = path;
    placement.start_frame = start_frame;
    clips.push_back(placement);

    if (!publish_lane(track, std::move(clips), error_out))
    {
        return false;
    }
    if (tracks_[track].name.empty() || tracks_[track].name == default_track_name(track))
    {
        tracks_[track].name = path_stem(path);
    }
    return true;
}

bool SessionTimeline::remove_clip(std::size_t track, std::size_t clip_index,
                                  std::string& error_out)
{
    error_out.clear();
    if (track >= tracks_.size() || clip_index >= tracks_[track].clips.size())
    {
        error_out = "no clip at that index";
        return false;
    }

    std::vector<ClipPlacement> clips = tracks_[track].clips;
    clips.erase(clips.begin() + static_cast<std::ptrdiff_t>(clip_index));
    return publish_lane(track, std::move(clips), error_out);
}

bool SessionTimeline::move_clip(std::size_t track, std::size_t clip_index,
                                std::uint64_t start_frame, std::string& error_out)
{
    error_out.clear();
    if (track >= tracks_.size() || clip_index >= tracks_[track].clips.size())
    {
        error_out = "no clip at that index";
        return false;
    }

    std::vector<ClipPlacement> clips = tracks_[track].clips;
    clips[clip_index].start_frame = start_frame;
    return publish_lane(track, std::move(clips), error_out);
}

std::size_t SessionTimeline::clip_count(std::size_t track) const
{
    return track < tracks_.size() ? tracks_[track].clips.size() : 0u;
}

SessionTimeline::ClipPlacement SessionTimeline::clip(std::size_t track,
                                                     std::size_t clip_index) const
{
    if (track >= tracks_.size() || clip_index >= tracks_[track].clips.size())
    {
        return ClipPlacement{};
    }
    return tracks_[track].clips[clip_index];
}

bool SessionTimeline::load_track(std::size_t index, const std::string& path,
                                 std::string& error_out)
{
    error_out.clear();
    if (index >= tracks_.size())
    {
        error_out = "track index out of range";
        return false;
    }

    const std::string previous_name = tracks_[index].name;
    const std::string previous_path = track_path(index);

    std::vector<ClipPlacement> single(1);
    single[0].path = path;
    if (!publish_lane(index, std::move(single), error_out))
    {
        return false;
    }

    if (previous_name.empty() || previous_name == default_track_name(index)
        || previous_name == path_stem(previous_path))
    {
        tracks_[index].name = path_stem(path);
    }
    return true;
}

SessionTimeline::TrackColor SessionTimeline::palette_color(std::size_t index) noexcept
{
    return kTrackPalette[index % palette_size()];
}

std::size_t SessionTimeline::palette_size() noexcept
{
    return std::size(kTrackPalette);
}

std::string SessionTimeline::default_track_name(std::size_t index)
{
    return "Track " + std::to_string(index + 1);
}

bool SessionTimeline::add_track(std::string& error_out)
{
    error_out.clear();
    if (tracks_.size() >= kMaxTracks)
    {
        error_out = "the arrangement already carries " + std::to_string(kMaxTracks) + " tracks";
        return false;
    }

    const std::size_t index = tracks_.size();
    TrackInfo         track;
    track.name = default_track_name(index);
    track.color = palette_color(index);
    tracks_.push_back(std::move(track));

    std::string ignored;
    publish_lane(index, std::vector<ClipPlacement>{}, ignored);
    player_.set_gain(index, 1.0f);
    player_.set_pan(index, 0.0f);
    player_.set_mute(index, false);
    player_.set_solo(index, false);
    return true;
}

std::vector<std::string> SessionTimeline::files_only_track_reads(std::size_t index) const
{
    if (index >= tracks_.size())
    {
        return std::vector<std::string>();
    }

    std::set<std::string> elsewhere;
    for (std::size_t lane = 0; lane < tracks_.size(); ++lane)
    {
        if (lane == index)
        {
            continue;
        }
        for (const ClipPlacement& placement : tracks_[lane].clips)
        {
            elsewhere.insert(placement.path);
        }
    }

    std::set<std::string>    named;
    std::vector<std::string> files;
    for (const ClipPlacement& placement : tracks_[index].clips)
    {
        if (elsewhere.count(placement.path) == 0 && named.insert(placement.path).second)
        {
            files.push_back(placement.path);
        }
    }
    return files;
}

bool SessionTimeline::erase_track(std::size_t index, std::string& error_out)
{
    error_out.clear();
    if (index >= tracks_.size())
    {
        error_out = "track index out of range";
        return false;
    }

    struct MixState
    {
        float gain;   ///< Holds the lane's linear gain.
        float pan;    ///< Holds the lane's pan position.
        bool  muted;  ///< Marks the lane as dropped from the mix.
        bool  soloed; ///< Marks the lane as soloed.
    };

    std::vector<MixState> mix;
    mix.reserve(tracks_.size());
    for (std::size_t lane = 0; lane < tracks_.size(); ++lane)
    {
        mix.push_back(MixState{ player_.gain(lane), player_.pan(lane), player_.muted(lane),
                                player_.soloed(lane) });
    }

    const std::size_t vacated = tracks_.size() - 1;
    const auto        cut = static_cast<std::ptrdiff_t>(index);
    tracks_.erase(tracks_.begin() + cut);
    mix.erase(mix.begin() + cut);

    // Every lane that moved down is republished even after one of them failed.
    bool        republished = true;
    std::string lane_error;
    for (std::size_t lane = index; lane < tracks_.size(); ++lane)
    {
        std::vector<ClipPlacement> clips = tracks_[lane].clips;
        if (!publish_lane(lane, std::move(clips), lane_error))
        {
            republished = false;
            if (error_out.empty())
            {
                error_out = lane_error;
            }
        }
        player_.set_gain(lane, mix[lane].gain);
        player_.set_pan(lane, mix[lane].pan);
        player_.set_mute(lane, mix[lane].muted);
        player_.set_solo(lane, mix[lane].soloed);
    }

    player_.set_timeline(vacated, std::make_unique<SushiDSP::TrackTimeline>(
        player_.takes(), std::vector<SushiDSP::Clip>{}));
    player_.set_gain(vacated, 1.0f);
    player_.set_pan(vacated, 0.0f);
    player_.set_mute(vacated, false);
    player_.set_solo(vacated, false);
    forget_unused_takes();

    if (reamp_track_.has_value())
    {
        if (*reamp_track_ == index)
        {
            clear_reamp_track();
        }
        else if (*reamp_track_ > index)
        {
            reamp_track_ = *reamp_track_ - 1;
        }
    }
    if (record_track_.has_value())
    {
        if (*record_track_ == index)
        {
            record_track_ = std::nullopt;
        }
        else if (*record_track_ > index)
        {
            record_track_ = *record_track_ - 1;
        }
    }
    return republished;
}

bool SessionTimeline::remove_track(std::size_t index, std::string& error_out)
{
    return erase_track(index, error_out);
}

bool SessionTimeline::remove_track_and_files(std::size_t index, std::string& error_out)
{
    error_out.clear();
    if (index >= tracks_.size())
    {
        error_out = "track index out of range";
        return false;
    }

    const std::vector<std::string> files = files_only_track_reads(index);
    const bool                     erased = erase_track(index, error_out);

    bool deleted = true;
    for (const std::string& path : files)
    {
        std::error_code code;
        std::filesystem::remove(std::filesystem::u8path(path), code);
        if (code)
        {
            deleted = false;
            if (error_out.empty())
            {
                error_out = "could not delete '" + path + "': " + code.message();
            }
        }
    }
    return erased && deleted;
}

SessionTimeline::TrackColor SessionTimeline::track_color(std::size_t index) const
{
    return index < tracks_.size() ? tracks_[index].color : TrackColor{ 0 };
}

void SessionTimeline::set_track_color(std::size_t index, TrackColor color)
{
    if (index < tracks_.size())
    {
        tracks_[index].color = color;
    }
}

void SessionTimeline::clear_track(std::size_t index)
{
    if (index >= tracks_.size())
    {
        return;
    }
    tracks_[index] = TrackInfo{};
    tracks_[index].color = palette_color(index);
    std::string error;
    publish_lane(index, std::vector<ClipPlacement>{}, error);
    player_.set_gain(index, 1.0f);
    player_.set_pan(index, 0.0f);
    player_.set_mute(index, false);
    player_.set_solo(index, false);
    if (reamp_track_.has_value() && *reamp_track_ == index)
    {
        clear_reamp_track();
    }
}

bool SessionTimeline::track_is_loaded(std::size_t index) const
{
    return index < tracks_.size() && !tracks_[index].clips.empty();
}

std::string SessionTimeline::track_name(std::size_t index) const
{
    return index < tracks_.size() ? tracks_[index].name : std::string();
}

void SessionTimeline::set_track_name(std::size_t index, const std::string& name)
{
    if (index < tracks_.size())
    {
        tracks_[index].name = name;
    }
}

std::string SessionTimeline::track_path(std::size_t index) const
{
    if (index >= tracks_.size() || tracks_[index].clips.empty())
    {
        return std::string();
    }
    return tracks_[index].clips.front().path;
}

bool SessionTimeline::set_reamp_track(std::size_t index, std::string& error_out)
{
    error_out.clear();
    if (index >= tracks_.size() || tracks_[index].clips.empty())
    {
        clear_reamp_track();
        return false;
    }

    // The re-amp leg mirrors the lane's frames; see apps/host_gui/README.md.
    reamp_takes_by_path_.clear();
    std::vector<SushiDSP::Clip> lane;
    lane.reserve(tracks_[index].clips.size());
    for (const ClipPlacement& placement : tracks_[index].clips)
    {
        const SushiDSP::Take* take = nullptr;
        const auto            found = reamp_takes_by_path_.find(placement.path);
        if (found != reamp_takes_by_path_.end())
        {
            take = found->second;
        }
        else
        {
            std::unique_ptr<SushiDSP::Take> loaded = load_take(placement.path, error_out);
            if (loaded == nullptr)
            {
                reamp_takes_by_path_.clear();
                return false;
            }
            take = reamp_player_.takes().insert(std::move(loaded));
            reamp_takes_by_path_[placement.path] = take;
        }
        lane.push_back(SushiDSP::Clip{ take, placement.start_frame, placement.take_offset,
                                       placement.length_frames });
    }

    std::unique_ptr<SushiDSP::TrackTimeline> timeline;
    try
    {
        timeline = std::make_unique<SushiDSP::TrackTimeline>(reamp_player_.takes(), lane);
    }
    catch (const std::exception& error)
    {
        error_out = error.what();
        reamp_takes_by_path_.clear();
        return false;
    }

    reamp_player_.set_timeline(0, std::move(timeline));
    reamp_player_.set_mute(0, false);
    reamp_track_ = index;
    return true;
}

void SessionTimeline::clear_reamp_track()
{
    reamp_takes_by_path_.clear();
    reamp_player_.set_timeline(0, std::make_unique<SushiDSP::TrackTimeline>(
        reamp_player_.takes(), std::vector<SushiDSP::Clip>{}));
    reamp_track_ = std::nullopt;
}

void SessionTimeline::set_take_folder(const std::string& path)
{
    take_folder_ = path;
}

std::string SessionTimeline::take_folder() const
{
    return take_folder_;
}

void SessionTimeline::set_record_track(std::optional<std::size_t> track)
{
    record_track_ = track.has_value() && *track < tracks_.size() ? track : std::nullopt;
}

bool SessionTimeline::arm_recording(const std::string& take_name, std::string& error_out)
{
    error_out.clear();
    if (take_name.empty())
    {
        error_out = "a take needs a name";
        return false;
    }

    // The stamp is read before the bus opens, so no accepted block precedes the clip's start.
    const std::uint64_t armed_at = transport_.position_frames();
    if (!recording_bus_.arm(take_folder_, take_name))
    {
        error_out = "could not open the take files in '" + take_folder_ + "'";
        return false;
    }
    armed_frame_ = armed_at;
    record_take_name_ = take_name;
    transport_.record();
    return true;
}

void SessionTimeline::stop_recording(std::string* error_out)
{
    recording_bus_.stop();

    const std::string take_name = record_take_name_;
    record_take_name_.clear();
    if (!record_track_.has_value() || take_name.empty())
    {
        return;
    }

    std::string error;
    const bool  placed = place_clip(*record_track_,
                                    join_take_path(take_folder_, take_name + "-dry.wav"),
                                    armed_frame_, error);
    if (!placed && error_out != nullptr)
    {
        *error_out = error;
    }
}

bool SessionTimeline::poll_recording(std::string& error_out)
{
    if (!recording_bus_.failed())
    {
        return false;
    }

    recording_bus_.stop();
    transport_.stop();
    error_out = "recording stopped: a take file could not be written";
    return true;
}
