/**************************************************************************/
/* SessionTimeline.cpp                                                    */
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

/** @file SessionTimeline.cpp
 *  @brief Implements the host's lanes, clips, re-amp leg and take recording.
 *  @author Mustafa Garip
 */

#include "SessionTimeline.hpp"

#include <algorithm>
#include <exception>
#include <iterator>
#include <memory>
#include <set>
#include <utility>

SessionTimeline::SessionTimeline(const SushiDSP::PrepareInfo& info)
    : recording_bus_(std::max(1u, info.channels), info.sample_rate)
    , metronome_(info.sample_rate)
{
    musical_clock_.set_sample_rate(info.sample_rate);
    // A lane converts a clip only through a sized reader, so both players are prepared here and
    // no caller can set a speed on an unprepared one.
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

        // A zero length is the take's whole remainder, which is what a preset row naming only a
        // file means and what a freshly dropped file means.
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
    if (tracks_[track].name.empty())
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

    if (previous_name.empty() || previous_name == path_stem(previous_path))
    {
        tracks_[index].name = path_stem(path);
    }
    return true;
}

void SessionTimeline::clear_track(std::size_t index)
{
    if (index >= tracks_.size())
    {
        return;
    }
    tracks_[index] = TrackInfo{};
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

    // The re-amp leg mirrors the lane's frames, so a take recorded at a bar drives the rig at
    // that bar. See docs/superpowers/specs/2026-09-08-arrangement-view-design.md §5.
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

    // The stamp is read before the bus opens, so no block the bus accepts belongs to an earlier
    // frame than the one a stopped recording's clip begins at.
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
