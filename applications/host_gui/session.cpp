/**************************************************************************/
/* session.cpp                                                            */
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
 *  @file session.cpp
 *  @brief Implements the session's tracks, their strips and the table the audio thread reads.
 *  @author Mustafa Garip & Sushi Systems
 */

#include "session.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

std::array<float, 2> strip_gains(const TrackStrip& strip, bool any_soloed) noexcept
{
    const bool audible = any_soloed ? strip.soloed : !strip.muted;
    if (!audible)
    {
        return {0.0f, 0.0f};
    }
    const float level = std::pow(10.0f, strip.level_db / 20.0f);
    const float pan = std::clamp(strip.pan, -1.0f, 1.0f);
    // A balance: the centre is unity on both sides, and a turn takes the far side down.
    return {level * (pan > 0.0f ? 1.0f - pan : 1.0f), level * (pan < 0.0f ? 1.0f + pan : 1.0f)};
}

Session::Session(const SushiDSP::PrepareInfo& info)
    : info_(info), timeline_(std::make_shared<SessionTimeline>(info))
{
    // The timeline opens with every lane it can carry; a session opens with one track.
    std::string ignored;
    while (timeline_->track_count() > 1)
    {
        timeline_->remove_track(timeline_->track_count() - 1, ignored);
    }
    if (timeline_->track_count() == 0)
    {
        timeline_->add_track(ignored);
    }
    match_timeline();
}

Session::~Session()
{
    for (AudioTrack& track : audio_.tracks)
    {
        track.rig.store(nullptr, std::memory_order_release);
    }
}

std::size_t Session::track_count() const noexcept
{
    return tracks_.size();
}

bool Session::add_track(std::string& error_out)
{
    if (tracks_.size() >= K_MAX_TRACKS)
    {
        error_out = "A session carries at most " + std::to_string(K_MAX_TRACKS) + " tracks.";
        return false;
    }
    if (!timeline_->add_track(error_out))
    {
        return false;
    }
    push_track();
    ++rig_revision_;
    publish();
    return true;
}

bool Session::remove_track(std::size_t index, bool delete_files, std::string& error_out)
{
    if (index >= tracks_.size())
    {
        error_out = "No track " + std::to_string(index + 1) + ".";
        return false;
    }
    if (tracks_.size() == 1)
    {
        error_out = "A session keeps at least one track.";
        return false;
    }
    const bool removed = delete_files ? timeline_->remove_track_and_files(index, error_out)
                                      : timeline_->remove_track(index, error_out);
    // A lane whose files would not delete is gone all the same, so the track goes with it.
    if (!removed && timeline_->track_count() == tracks_.size())
    {
        return false;
    }
    retire(index);
    selected_ = std::min(selected_ > index ? selected_ - 1 : selected_, tracks_.size() - 1);
    ++rig_revision_;
    publish();
    return removed;
}

std::size_t Session::selected() const noexcept
{
    return selected_;
}

void Session::select(std::size_t index)
{
    const std::size_t chosen = std::min(index, tracks_.size() - 1);
    rig_revision_ += chosen != selected_ ? 1u : 0u;
    selected_ = chosen;
    audio_.selected.store(static_cast<std::uint32_t>(selected_), std::memory_order_release);
}

RigController& Session::rig(std::size_t index)
{
    return *tracks_[index].rig;
}

const RigController& Session::rig(std::size_t index) const
{
    return *tracks_[index].rig;
}

RigController& Session::selected_rig()
{
    return *tracks_[selected_].rig;
}

const TrackStrip& Session::strip(std::size_t index) const
{
    return tracks_[index].strip;
}

void Session::set_strip(std::size_t index, const TrackStrip& strip)
{
    if (index < tracks_.size())
    {
        tracks_[index].strip = strip;
        publish();
    }
}

const std::string& Session::preset_path(std::size_t index) const
{
    return tracks_[index].preset_path;
}

void Session::set_preset_path(std::size_t index, const std::string& path)
{
    if (index < tracks_.size())
    {
        tracks_[index].preset_path = path;
    }
}

bool Session::set_hrtf_path(const std::string& path, std::string& error_out)
{
    hrtf_path_ = path;
    bool taken = true;
    for (Track& track : tracks_)
    {
        taken = track.rig->set_hrtf_path(path, error_out) && taken;
    }
    return taken;
}

SessionTimeline& Session::timeline() noexcept
{
    return *timeline_;
}

const SessionTimeline& Session::timeline() const noexcept
{
    return *timeline_;
}

void Session::match_timeline()
{
    const std::size_t lanes = std::min(timeline_->track_count(), K_MAX_TRACKS);
    while (tracks_.size() < lanes)
    {
        push_track();
    }
    while (tracks_.size() > std::max<std::size_t>(lanes, 1))
    {
        retire(tracks_.size() - 1);
    }
    selected_ = std::min(selected_, tracks_.size() - 1);
    ++rig_revision_;
    publish();
}

std::uint64_t Session::rig_revision() const noexcept
{
    return rig_revision_;
}

void Session::touch_rigs() noexcept
{
    ++rig_revision_;
}

void Session::reclaim()
{
    for (Track& track : tracks_)
    {
        track.rig->reclaim();
    }
    const std::uint64_t now = audio_.blocks.load(std::memory_order_acquire);
    retired_.erase(std::remove_if(retired_.begin(), retired_.end(),
                                  [now](const Retired& entry)
                                  {
                                      return now >= entry.at_block + K_RETIRE_BLOCKS;
                                  }),
                   retired_.end());
}

Session::AudioTable& Session::audio() noexcept
{
    return audio_;
}

const SushiDSP::PrepareInfo& Session::info() const noexcept
{
    return info_;
}

void Session::publish()
{
    const bool any_soloed = std::any_of(tracks_.begin(), tracks_.end(),
                                        [](const Track& track)
                                        {
                                            return track.strip.soloed;
                                        });
    for (std::size_t index = 0; index < K_MAX_TRACKS; ++index)
    {
        AudioTrack& entry = audio_.tracks[index];
        if (index >= tracks_.size())
        {
            entry.rig.store(nullptr, std::memory_order_release);
            continue;
        }
        const std::array<float, 2> gains = strip_gains(tracks_[index].strip, any_soloed);
        entry.gain_left.store(gains[0], std::memory_order_release);
        entry.gain_right.store(gains[1], std::memory_order_release);
        entry.rig_bypassed.store(tracks_[index].strip.rig_bypassed, std::memory_order_release);
        entry.rig.store(tracks_[index].rig.get(), std::memory_order_release);
    }
    audio_.selected.store(static_cast<std::uint32_t>(selected_), std::memory_order_release);
}

void Session::push_track()
{
    Track track;
    track.rig = std::make_unique<RigController>(info_, timeline_);
    if (!hrtf_path_.empty())
    {
        std::string ignored;
        track.rig->set_hrtf_path(hrtf_path_, ignored);
    }
    tracks_.push_back(std::move(track));
}

void Session::retire(std::size_t index)
{
    // The table drops the rig first, so no block that starts after this reads it.
    audio_.tracks[index].rig.store(nullptr, std::memory_order_release);
    retired_.push_back(Retired{std::move(tracks_[index].rig),
                               audio_.blocks.load(std::memory_order_acquire)});
    tracks_.erase(tracks_.begin() + static_cast<std::ptrdiff_t>(index));
}
