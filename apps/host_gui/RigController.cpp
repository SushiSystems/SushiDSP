/**************************************************************************/
/* RigController.cpp                                                      */
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

#include "RigController.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <exception>
#include <memory>
#include <set>
#include <utility>

#include <nlohmann/json.hpp>

#include "SushiDSP/io/IrLoader.hpp"
#include "SushiDSP/core/ICabinetHost.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"
#include "SushiDSP/rig_preset/RigPreset.hpp"
#include "SushiDSP/routing/MixerNode.hpp"
#include "SushiDSP/routing/SplitterNode.hpp"

namespace
{
    /// @brief Finds a parameter's position in its descriptor's parameters[] array - which is
    /// exactly the index SushiDSP::ParameterChange carries and IAudioNode::set_parameter(index)
    /// expects.
    /// @param descriptor_id The owning node's NodeDescriptor id.
    /// @param param_id The parameter's id.
    /// @param out_index Receives the index on success.
    /// @return True if the descriptor and the parameter were both found.
    bool find_parameter_index(const std::string& descriptor_id, const char* param_id, std::uint16_t& out_index)
    {
        for (const auto& entry : SushiDSP::node_registry())
        {
            if (descriptor_id != entry.descriptor->id)
            {
                continue;
            }
            for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
            {
                if (std::strcmp(param_id, entry.descriptor->parameters[p].id) == 0)
                {
                    out_index = static_cast<std::uint16_t>(p);
                    return true;
                }
            }
            return false;
        }
        return false;
    }

    /// @brief Reports whether the node registry can construct a descriptor id.
    /// @param descriptor_id The id to look for.
    /// @return True when the registry holds an entry with that id.
    bool descriptor_is_registered(const std::string& descriptor_id)
    {
        for (const auto& entry : SushiDSP::node_registry())
        {
            if (descriptor_id == entry.descriptor->id)
            {
                return true;
            }
        }
        return false;
    }
}

RigController::RigController(const SushiDSP::PrepareInfo& info)
    : info_(info)
    , recording_bus_(std::max(1u, info.channels), info.sample_rate)
    , metronome_(info.sample_rate)
{
    musical_clock_.set_sample_rate(info.sample_rate);
    // A lane converts a clip only through a sized reader, so both players are prepared here and
    // no caller can set a speed on an unprepared one.
    player_.prepare(info.max_block);
    reamp_player_.prepare(info.max_block);
    publish();
}

void RigController::seed_defaults(Slot& slot) const
{
    for (const auto& entry : SushiDSP::node_registry())
    {
        if (slot.descriptor_id == entry.descriptor->id)
        {
            for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
            {
                const auto& param = entry.descriptor->parameters[p];
                slot.parameters[param.id] = param.default_value;
            }
            break;
        }
    }
}

RigController::SlotId RigController::add_node(const std::string& descriptor_id, bool publish_now)
{
    const SlotId id = next_slot_id_++;
    Slot slot;
    slot.descriptor_id = descriptor_id;
    seed_defaults(slot);
    slots_[id] = std::move(slot);
    slot_order_.push_back(id);
    if (publish_now)
    {
        publish();
    }
    return id;
}

void RigController::connect(SlotId from, SlotId to, bool publish_now)
{
    const bool present = std::any_of(edges_.begin(), edges_.end(),
        [from, to](const Edge& edge) { return edge.from == from && edge.to == to; });
    if (present)
    {
        return;
    }
    edges_.push_back({ from, to });
    if (publish_now)
    {
        publish();
    }
}

void RigController::disconnect(SlotId from, SlotId to, bool publish_now)
{
    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [from, to](const Edge& edge) { return edge.from == from && edge.to == to; }),
        edges_.end());
    if (publish_now)
    {
        publish();
    }
}

void RigController::remove(SlotId slot, bool publish_now)
{
    // Find slot's one upstream and one downstream neighbor (if any) before erasing its edges,
    // so a middle-of-chain removal can reconnect around the gap — matches today's behavior of
    // never leaving two previously-adjacent slots disconnected by a removal.
    std::optional<SlotId> upstream;
    std::optional<SlotId> downstream;
    for (const Edge& edge : edges_)
    {
        if (edge.to == slot) { upstream = edge.from; }
        if (edge.from == slot) { downstream = edge.to; }
    }

    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [slot](const Edge& edge) { return edge.from == slot || edge.to == slot; }),
        edges_.end());

    if (upstream.has_value() && downstream.has_value())
    {
        connect(*upstream, *downstream, false);
    }

    slots_.erase(slot);
    slot_order_.erase(std::remove(slot_order_.begin(), slot_order_.end(), slot), slot_order_.end());
    if (publish_now)
    {
        publish();
    }
}

RigController::SlotId RigController::append(const std::string& descriptor_id)
{
    const SlotId id = next_slot_id_++;
    Slot slot;
    slot.descriptor_id = descriptor_id;
    seed_defaults(slot);
    slots_[id] = std::move(slot);

    if (!slot_order_.empty())
    {
        connect(slot_order_.back(), id, false);
    }
    slot_order_.push_back(id);
    publish();
    return id;
}

RigController::SlotId RigController::insert_before(std::optional<SlotId> following, const std::string& descriptor_id)
{
    const SlotId id = next_slot_id_++;
    Slot slot;
    slot.descriptor_id = descriptor_id;
    seed_defaults(slot);
    slots_[id] = std::move(slot);

    if (following.has_value())
    {
        // Retarget whichever edge currently points at `following` (if any) to point at the new
        // slot instead, then connect the new slot to `following`.
        for (Edge& edge : edges_)
        {
            if (edge.to == *following)
            {
                edge.to = id;
            }
        }
        connect(id, *following, false);

        const auto position = std::find(slot_order_.begin(), slot_order_.end(), *following);
        slot_order_.insert(position, id);
    }
    else if (!slot_order_.empty())
    {
        connect(id, slot_order_.front(), false);
        slot_order_.insert(slot_order_.begin(), id);
    }
    else
    {
        slot_order_.push_back(id);
    }

    publish();
    return id;
}

std::vector<RigController::SlotId> RigController::downstream_of(SlotId slot) const
{
    std::vector<SlotId> result;
    for (const Edge& edge : edges_)
    {
        if (edge.from == slot)
        {
            result.push_back(edge.to);
        }
    }
    return result;
}

RigController::BranchEnd RigController::walk_branch(SlotId start) const
{
    SlotId current = start;
    std::set<SlotId> visited;
    while (visited.insert(current).second)
    {
        if (upstream_of(current).size() >= 2)
        {
            return { current, true };
        }
        const std::vector<SlotId> next = downstream_of(current);
        if (next.size() != 1)
        {
            return { current, false };
        }
        current = next.front();
    }
    // A cycle walked back onto a slot already left behind; stop rather than circle it forever.
    return { current, false };
}

std::optional<RigController::SlotId> RigController::add_branch(SlotId splitter,
                                                               const std::string& descriptor_id)
{
    const auto splitter_slot = slots_.find(splitter);
    if (splitter_slot == slots_.end()
        || splitter_slot->second.descriptor_id != SushiDSP::splitter_descriptor().id
        || !descriptor_is_registered(descriptor_id))
    {
        return std::nullopt;
    }

    const std::vector<SlotId> down = downstream_of(splitter);
    if (down.empty())
    {
        // A dangling splitter's one branch is still the graph's only sink, so it needs no mixer.
        const SlotId only_branch = add_node(descriptor_id, false);
        connect(splitter, only_branch, false);
        publish();
        return only_branch;
    }

    const BranchEnd end = walk_branch(down.front());
    const SlotId branch = add_node(descriptor_id, false);

    const auto end_slot = slots_.find(end.slot);
    const bool rejoin_exists = end.is_join && end_slot != slots_.end()
        && end_slot->second.descriptor_id == SushiDSP::mixer_descriptor().id;

    if (rejoin_exists)
    {
        connect(splitter, branch, false);
        connect(branch, end.slot, false);

        // The rejoin stays last in chain order, so append() still lands past every branch.
        slot_order_.erase(std::remove(slot_order_.begin(), slot_order_.end(), branch),
                          slot_order_.end());
        slot_order_.insert(std::find(slot_order_.begin(), slot_order_.end(), end.slot), branch);
    }
    else
    {
        // A branch nothing rejoins is a second sink, which RigGraph::compile() rejects.
        const std::vector<SlotId> after = downstream_of(end.slot);
        const SlotId mixer = add_node(SushiDSP::mixer_descriptor().id, false);
        if (!after.empty())
        {
            disconnect(end.slot, after.front(), false);
        }
        connect(end.slot, mixer, false);
        connect(splitter, branch, false);
        connect(branch, mixer, false);
        if (!after.empty())
        {
            connect(mixer, after.front(), false);
        }
    }

    publish();
    return branch;
}

void RigController::move_before(SlotId moved, std::optional<SlotId> following)
{
    if (following.has_value() && *following == moved)
    {
        return;
    }

    // Detach `moved` the same way remove() does (reconnect its old neighbors around it), but
    // keep `moved`'s own slot alive.
    std::optional<SlotId> upstream;
    std::optional<SlotId> downstream;
    for (const Edge& edge : edges_)
    {
        if (edge.to == moved) { upstream = edge.from; }
        if (edge.from == moved) { downstream = edge.to; }
    }
    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [moved](const Edge& edge) { return edge.from == moved || edge.to == moved; }),
        edges_.end());
    if (upstream.has_value() && downstream.has_value())
    {
        connect(*upstream, *downstream, false);
    }
    slot_order_.erase(std::remove(slot_order_.begin(), slot_order_.end(), moved), slot_order_.end());

    if (following.has_value())
    {
        for (Edge& edge : edges_)
        {
            if (edge.to == *following)
            {
                edge.to = moved;
            }
        }
        connect(moved, *following, false);
        const auto position = std::find(slot_order_.begin(), slot_order_.end(), *following);
        slot_order_.insert(position, moved);
    }
    else if (!slot_order_.empty())
    {
        connect(moved, slot_order_.front(), false);
        slot_order_.insert(slot_order_.begin(), moved);
    }
    else
    {
        slot_order_.push_back(moved);
    }

    publish();
}

void RigController::set_parameter(SlotId slot, const char* param_id, float value)
{
    const auto it = slots_.find(slot);
    if (it == slots_.end())
    {
        return;
    }
    it->second.parameters[param_id] = value;

    std::uint16_t parameter_index = 0;
    if (!find_parameter_index(it->second.descriptor_id, param_id, parameter_index))
    {
        return;
    }

    const auto handle_it = slot_to_handle_.find(slot);
    if (active_graph_ != nullptr && handle_it != slot_to_handle_.end())
    {
        const SushiDSP::ParameterChange change{ handle_it->second, parameter_index, value };
        if (!active_graph_->push_parameter_change(change))
        {
            ++dropped_parameter_changes_;
        }
    }
}

float RigController::get_parameter(SlotId slot, const char* param_id) const
{
    const auto it = slots_.find(slot);
    if (it == slots_.end())
    {
        return 0.0f;
    }
    const auto param_it = it->second.parameters.find(param_id);
    return param_it != it->second.parameters.end() ? param_it->second : 0.0f;
}

bool RigController::set_cabinet(SlotId slot, const std::string& path)
{
    const auto it = slots_.find(slot);
    if (it == slots_.end())
    {
        return false;
    }

    SushiDSP::Ir impulse_response = SushiDSP::IrLoader::load(path, info_.sample_rate);

    const auto handle_it = slot_to_handle_.find(slot);
    if (handle_it != slot_to_handle_.end() && active_graph_ != nullptr)
    {
        auto* host = dynamic_cast<SushiDSP::ICabinetHost*>(active_graph_->node(handle_it->second));
        if (host == nullptr || !host->set_cabinet(std::move(impulse_response)))
        {
            return false;
        }
    }

    // A bypassed slot has no live node to ask, so the path is kept and the node decides on the
    // publish that brings it back; apply_cabinet_to_live_node() drops it if the node refuses.
    it->second.cabinet_path = path;
    return true;
}

void RigController::apply_cabinet_to_live_node(SlotId slot)
{
    const auto it = slots_.find(slot);
    const auto handle_it = slot_to_handle_.find(slot);
    if (it == slots_.end() || it->second.cabinet_path.empty()
        || handle_it == slot_to_handle_.end() || active_graph_ == nullptr)
    {
        return;
    }
    auto* host = dynamic_cast<SushiDSP::ICabinetHost*>(active_graph_->node(handle_it->second));
    if (host == nullptr)
    {
        it->second.cabinet_path.clear();
        return;
    }
    try
    {
        if (!host->set_cabinet(SushiDSP::IrLoader::load(it->second.cabinet_path, info_.sample_rate)))
        {
            it->second.cabinet_path.clear();
        }
    }
    catch (const std::exception&)
    {
        it->second.cabinet_path.clear();
    }
}

void RigController::reclaim()
{
    graph_slot_.reclaim();
    player_.reclaim();
    reamp_player_.reclaim();
    if (active_graph_ == nullptr)
    {
        return;
    }
    for (const auto& [slot, handle] : slot_to_handle_)
    {
        if (auto* host = dynamic_cast<SushiDSP::ICabinetHost*>(active_graph_->node(handle)))
        {
            host->reclaim_cabinets();
        }
    }
}

std::size_t RigController::dropped_parameter_changes() const
{
    return dropped_parameter_changes_;
}

std::vector<RigController::SlotId> RigController::slot_order() const
{
    return slot_order_;
}

std::string RigController::descriptor_id(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() ? it->second.descriptor_id : std::string();
}

std::string RigController::cabinet_path(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() ? it->second.cabinet_path : std::string();
}

bool RigController::hosts_cabinet(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() && it->second.hosts_cabinet;
}

std::vector<RigController::SlotId> RigController::upstream_of(SlotId slot) const
{
    std::vector<SlotId> result;
    for (const Edge& edge : edges_)
    {
        if (edge.to == slot)
        {
            result.push_back(edge.from);
        }
    }
    return result;
}

SushiDSP::RigGraph* RigController::active_graph()
{
    return active_graph_;
}

SushiDSP::RtGraphSlot& RigController::graph_slot()
{
    return graph_slot_;
}

void RigController::set_ir_folder(const std::string& path)
{
    ir_folder_ = path;
}

std::string RigController::ir_folder() const
{
    return ir_folder_;
}


std::unique_ptr<SushiDSP::Take> RigController::load_take(const std::string& path,
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

std::string RigController::path_stem(const std::string& path)
{
    const std::size_t cut = path.find_last_of("/\\");
    const std::string name = cut == std::string::npos ? path : path.substr(cut + 1);
    const std::size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

void RigController::set_input_source(RigInputSource source)
{
    input_source_.store(source, std::memory_order_relaxed);
}

std::string RigController::join_take_path(const std::string& folder,
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

const SushiDSP::Take* RigController::take_for_path(const std::string& path,
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

void RigController::forget_unused_takes()
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

bool RigController::publish_lane(std::size_t track, std::vector<ClipPlacement> clips,
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

bool RigController::place_clip(std::size_t track, const std::string& path,
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

bool RigController::remove_clip(std::size_t track, std::size_t clip_index,
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

bool RigController::move_clip(std::size_t track, std::size_t clip_index,
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

std::size_t RigController::clip_count(std::size_t track) const
{
    return track < tracks_.size() ? tracks_[track].clips.size() : 0u;
}

RigController::ClipPlacement RigController::clip(std::size_t track, std::size_t clip_index) const
{
    if (track >= tracks_.size() || clip_index >= tracks_[track].clips.size())
    {
        return ClipPlacement{};
    }
    return tracks_[track].clips[clip_index];
}

bool RigController::load_track(std::size_t index, const std::string& path, std::string& error_out)
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

void RigController::clear_track(std::size_t index)
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

bool RigController::track_is_loaded(std::size_t index) const
{
    return index < tracks_.size() && !tracks_[index].clips.empty();
}

std::string RigController::track_name(std::size_t index) const
{
    return index < tracks_.size() ? tracks_[index].name : std::string();
}

void RigController::set_track_name(std::size_t index, const std::string& name)
{
    if (index < tracks_.size())
    {
        tracks_[index].name = name;
    }
}

std::string RigController::track_path(std::size_t index) const
{
    if (index >= tracks_.size() || tracks_[index].clips.empty())
    {
        return std::string();
    }
    return tracks_[index].clips.front().path;
}

bool RigController::set_reamp_track(std::size_t index, std::string& error_out)
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

void RigController::clear_reamp_track()
{
    reamp_takes_by_path_.clear();
    reamp_player_.set_timeline(0, std::make_unique<SushiDSP::TrackTimeline>(
        reamp_player_.takes(), std::vector<SushiDSP::Clip>{}));
    reamp_track_ = std::nullopt;
}

void RigController::set_take_folder(const std::string& path)
{
    take_folder_ = path;
}

std::string RigController::take_folder() const
{
    return take_folder_;
}

void RigController::set_record_track(std::optional<std::size_t> track)
{
    record_track_ = track.has_value() && *track < tracks_.size() ? track : std::nullopt;
}

bool RigController::arm_recording(const std::string& take_name, std::string& error_out)
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

void RigController::stop_recording(std::string* error_out)
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

bool RigController::poll_recording(std::string& error_out)
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

std::string RigController::save_to_json() const
{
    nlohmann::json document = nlohmann::json::parse(serialize_nodes());
    if (!ir_folder_.empty())
    {
        document["ir_folder"] = ir_folder_;
    }
    if (!take_folder_.empty())
    {
        document["take_folder"] = take_folder_;
    }

    nlohmann::json tracks_json = nlohmann::json::array();
    for (std::size_t index = 0; index < tracks_.size(); ++index)
    {
        if (tracks_[index].clips.empty())
        {
            continue;
        }
        nlohmann::json track_json = nlohmann::json::object();
        track_json["index"] = index;
        track_json["name"] = tracks_[index].name;
        track_json["path"] = track_path(index);
        track_json["gain"] = player_.gain(index);
        track_json["pan"] = player_.pan(index);
        track_json["muted"] = player_.muted(index);
        track_json["soloed"] = player_.soloed(index);

        nlohmann::json clips_json = nlohmann::json::array();
        for (const ClipPlacement& placement : tracks_[index].clips)
        {
            nlohmann::json clip_json = nlohmann::json::object();
            clip_json["path"] = placement.path;
            clip_json["start_frame"] = placement.start_frame;
            clip_json["take_offset"] = placement.take_offset;
            clip_json["length_frames"] = placement.length_frames;
            clips_json.push_back(clip_json);
        }
        track_json["clips"] = clips_json;
        tracks_json.push_back(track_json);
    }
    if (!tracks_json.empty())
    {
        document["tracks"] = tracks_json;
    }

    if (transport_.has_loop())
    {
        nlohmann::json loop_json = nlohmann::json::object();
        loop_json["begin_frames"] = transport_.loop_begin_frames();
        loop_json["end_frames"] = transport_.loop_end_frames();
        document["loop"] = loop_json;
    }

    // The playback speed is a practice setting, not a session's, so it is written nowhere. See
    // docs/superpowers/specs/2026-09-08-arrangement-view-design.md §8.
    nlohmann::json tempo_json = nlohmann::json::object();
    tempo_json["bpm"] = musical_clock_.beats_per_minute();
    tempo_json["beats_per_bar"] = musical_clock_.beats_per_bar();
    tempo_json["beat_unit"] = musical_clock_.beat_unit();
    tempo_json["metronome_enabled"] = metronome_.enabled();
    tempo_json["metronome_level"] = metronome_.level();
    document["tempo"] = tempo_json;

    return document.dump(2);
}

std::string RigController::serialize_nodes() const
{
    if (slot_order_.empty())
    {
        return "{\"nodes\":[]}";
    }

    std::unordered_map<SlotId, SushiDSP::NodeHandle> handles;
    const std::unique_ptr<SushiDSP::RigGraph> graph = build_graph(false, handles);

    std::unordered_map<SushiDSP::NodeHandle, SlotId> slot_by_handle;
    for (const auto& [slot, handle] : handles)
    {
        slot_by_handle[handle] = slot;
    }

    return SushiDSP::serialize_rig_to_json(*graph,
        [this, &slot_by_handle](SushiDSP::NodeHandle handle, nlohmann::json& node_json)
        {
            const auto it = slot_by_handle.find(handle);
            if (it == slot_by_handle.end())
            {
                return;
            }
            const Slot& slot = slots_.at(it->second);

            nlohmann::json params_json = nlohmann::json::object();
            for (const auto& [param_id, value] : slot.parameters)
            {
                params_json[param_id] = value;
            }
            node_json["params"] = params_json;

            if (!slot.cabinet_path.empty())
            {
                node_json["cabinet_ir"] = slot.cabinet_path;
            }
            if (slot.bypassed)
            {
                node_json["bypassed"] = true;
            }
        });
}

void RigController::load_from_json(const std::string& json_text)
{
    // Size the scratch graph's buffer pool from the preset's own node count (plus the same +1
    // headroom publish() uses). RigGraph::compile() only assert()s the pool's capacity, so a
    // fixed guess would hand the audio thread null buffers on any preset larger than that guess.
    const nlohmann::json document = nlohmann::json::parse(json_text);
    const std::size_t node_count = document.at("nodes").size();
    SushiDSP::RigGraph scratch(info_.max_block, static_cast<std::uint32_t>(node_count + 1));
    std::unordered_map<SushiDSP::NodeHandle, std::string> cabinet_paths_by_handle;
    std::unordered_map<SushiDSP::NodeHandle, bool> bypassed_by_handle;
    SushiDSP::deserialize_rig_from_json(scratch, json_text, info_,
        [&cabinet_paths_by_handle, &bypassed_by_handle](
            SushiDSP::NodeHandle handle, const nlohmann::json& node_json)
        {
            if (node_json.contains("cabinet_ir"))
            {
                cabinet_paths_by_handle[handle] = node_json.at("cabinet_ir").get<std::string>();
            }
            if (node_json.contains("bypassed"))
            {
                bypassed_by_handle[handle] = node_json.at("bypassed").get<bool>();
            }
        });
    scratch.compile(info_);

    slots_.clear();
    edges_.clear();
    slot_order_.clear();
    next_slot_id_ = 0;
    ir_folder_ = document.contains("ir_folder") ? document.at("ir_folder").get<std::string>()
                                                : std::string();
    take_folder_ = document.contains("take_folder")
        ? document.at("take_folder").get<std::string>()
        : std::string();

    // Every track is emptied to its defaults first, so a preset brings its own mixer state.
    for (std::size_t index = 0; index < tracks_.size(); ++index)
    {
        clear_track(index);
    }
    if (document.contains("tracks"))
    {
        std::string track_error;
        for (const nlohmann::json& track_json : document.at("tracks"))
        {
            const std::size_t index = track_json.value("index", tracks_.size());
            if (index >= tracks_.size())
            {
                continue;
            }
            const std::string row_path = track_json.value("path", std::string());

            std::vector<ClipPlacement> clips;
            if (track_json.contains("clips"))
            {
                for (const nlohmann::json& clip_json : track_json.at("clips"))
                {
                    ClipPlacement placement;
                    placement.path = clip_json.value("path", std::string());
                    placement.start_frame = clip_json.value("start_frame", std::uint64_t{ 0 });
                    placement.take_offset = clip_json.value("take_offset", std::uint64_t{ 0 });
                    placement.length_frames = clip_json.value("length_frames", std::uint64_t{ 0 });
                    if (!placement.path.empty())
                    {
                        clips.push_back(placement);
                    }
                }
            }
            else if (!row_path.empty())
            {
                // A preset written before the timeline names one file per row, which is what one
                // clip at frame 0 means. Spec §8.
                ClipPlacement placement;
                placement.path = row_path;
                clips.push_back(placement);
            }

            publish_lane(index, std::move(clips), track_error);
            tracks_[index].name = track_json.value("name", path_stem(track_path(index)));
            player_.set_gain(index, track_json.value("gain", 1.0f));
            player_.set_pan(index, track_json.value("pan", 0.0f));
            player_.set_mute(index, track_json.value("muted", false));
            player_.set_solo(index, track_json.value("soloed", false));
        }
    }

    transport_.clear_loop();
    if (document.contains("loop"))
    {
        const nlohmann::json& loop_json = document.at("loop");
        transport_.set_loop(loop_json.value("begin_frames", std::uint64_t{ 0 }),
                            loop_json.value("end_frames", std::uint64_t{ 0 }));
    }

    // A session carries no playback speed, so every session opens at 1.0. Spec §8.
    transport_.set_speed(1.0);

    // A preset written before the timeline carries no tempo, which meant 120 BPM in 4/4 with the
    // click off. Spec §8.
    const nlohmann::json tempo_json = document.contains("tempo")
        ? document.at("tempo")
        : nlohmann::json::object();
    musical_clock_.set_tempo(tempo_json.value("bpm", 120.0));
    musical_clock_.set_meter(tempo_json.value("beats_per_bar", std::uint32_t{ 4 }),
                             tempo_json.value("beat_unit", std::uint32_t{ 4 }));
    musical_clock_.set_sample_rate(info_.sample_rate);
    metronome_.set_enabled(tempo_json.value("metronome_enabled", false));
    metronome_.set_level(tempo_json.value("metronome_level", 0.5f));

    std::unordered_map<SushiDSP::NodeHandle, SlotId> handle_to_slot;
    for (SushiDSP::NodeHandle handle : scratch.nodes())
    {
        SushiDSP::IAudioNode* node = scratch.node(handle);
        const SlotId id = next_slot_id_++;
        Slot slot;
        slot.descriptor_id = node->type_id();
        seed_defaults(slot);
        for (const auto& entry : SushiDSP::node_registry())
        {
            if (slot.descriptor_id == entry.descriptor->id)
            {
                for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
                {
                    const char* param_id = entry.descriptor->parameters[p].id;
                    slot.parameters[param_id] = node->get_parameter(param_id);
                }
                break;
            }
        }
        const auto cabinet_it = cabinet_paths_by_handle.find(handle);
        if (cabinet_it != cabinet_paths_by_handle.end())
        {
            slot.cabinet_path = cabinet_it->second;
        }
        const auto bypass_it = bypassed_by_handle.find(handle);
        if (bypass_it != bypassed_by_handle.end())
        {
            slot.bypassed = bypass_it->second;
        }
        slots_[id] = std::move(slot);
        slot_order_.push_back(id);
        handle_to_slot[handle] = id;
    }
    for (SushiDSP::NodeHandle handle : scratch.nodes())
    {
        for (SushiDSP::NodeHandle upstream_handle : scratch.upstream_of(handle))
        {
            connect(handle_to_slot.at(upstream_handle), handle_to_slot.at(handle), false);
        }
    }

    publish();
}

std::unique_ptr<SushiDSP::RigGraph> RigController::build_graph(bool apply_bypass,
    std::unordered_map<SlotId, SushiDSP::NodeHandle>& handles_out) const
{
    handles_out.clear();

    auto graph = std::make_unique<SushiDSP::RigGraph>(
        info_.max_block, static_cast<std::uint32_t>(slot_order_.size() + 1));

    for (SlotId id : slot_order_)
    {
        const Slot& slot = slots_.at(id);
        if (apply_bypass && slot.bypassed)
        {
            continue;
        }
        auto node = SushiDSP::make_node(slot.descriptor_id.c_str(), info_);
        if (node == nullptr)
        {
            continue;
        }
        handles_out[id] = graph->add_node(std::move(node));
    }

    const std::vector<std::pair<SlotId, SlotId>> resolved =
        apply_bypass ? bridged_edges(handles_out) : direct_edges(handles_out);

    std::unordered_map<SlotId, std::size_t> fan_out;
    std::unordered_map<SlotId, std::size_t> fan_in;
    for (const auto& edge : resolved)
    {
        if (++fan_out[edge.first] > SushiDSP::RigGraph::kMaxFanInOut
            || ++fan_in[edge.second] > SushiDSP::RigGraph::kMaxFanInOut)
        {
            throw std::runtime_error(
                apply_bypass
                    ? "RigController::build_graph: bridging a bypassed slot would exceed the "
                      "graph's fan-in/fan-out limit"
                    : "RigController::build_graph: the rig's own edges exceed the graph's "
                      "fan-in/fan-out limit");
        }
    }

    for (const auto& edge : resolved)
    {
        graph->connect(handles_out.at(edge.first), handles_out.at(edge.second));
    }

    graph->compile(info_);
    return graph;
}

std::vector<std::pair<RigController::SlotId, RigController::SlotId>>
RigController::direct_edges(const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const
{
    std::vector<std::pair<SlotId, SlotId>> result;
    std::set<std::pair<SlotId, SlotId>> seen;
    for (const Edge& edge : edges_)
    {
        if (live.count(edge.from) != 0 && live.count(edge.to) != 0
            && seen.insert({ edge.from, edge.to }).second)
        {
            result.push_back({ edge.from, edge.to });
        }
    }
    return result;
}

std::vector<std::pair<RigController::SlotId, RigController::SlotId>>
RigController::bridged_edges(const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const
{
    std::vector<std::pair<SlotId, SlotId>> result;
    std::set<std::pair<SlotId, SlotId>> seen;

    for (const Edge& edge : edges_)
    {
        if (live.count(edge.from) == 0)
        {
            continue;
        }

        // Walk forward from edge.to through bypassed slots until every live target is found.
        std::vector<SlotId> frontier{ edge.to };
        std::set<SlotId> visited;
        while (!frontier.empty())
        {
            const SlotId current = frontier.back();
            frontier.pop_back();
            if (!visited.insert(current).second)
            {
                continue;
            }
            if (live.count(current) != 0)
            {
                if (seen.insert({ edge.from, current }).second)
                {
                    result.push_back({ edge.from, current });
                }
                continue;
            }
            for (const Edge& next : edges_)
            {
                if (next.from == current)
                {
                    frontier.push_back(next.to);
                }
            }
        }
    }
    return result;
}

void RigController::publish()
{
    std::unordered_map<SlotId, SushiDSP::NodeHandle> fresh_handles;
    auto graph = build_graph(true, fresh_handles);

    // A slot the registry no longer knows is dropped here rather than inside build_graph, which
    // stays const so save_to_json() can call it too.
    std::vector<SlotId> surviving_order;
    surviving_order.reserve(slot_order_.size());
    for (SlotId id : slot_order_)
    {
        if (fresh_handles.count(id) != 0 || slots_.at(id).bypassed)
        {
            surviving_order.push_back(id);
        }
        else
        {
            slots_.erase(id);
        }
    }
    slot_order_ = std::move(surviving_order);

    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [this](const Edge& edge)
            { return slots_.count(edge.from) == 0 || slots_.count(edge.to) == 0; }),
        edges_.end());

    for (const auto& [id, handle] : fresh_handles)
    {
        SushiDSP::IAudioNode* node = graph->node(handle);
        if (node == nullptr)
        {
            continue;
        }
        Slot& slot = slots_.at(id);
        slot.hosts_cabinet = dynamic_cast<SushiDSP::ICabinetHost*>(node) != nullptr;
        for (const auto& [param_id, value] : slot.parameters)
        {
            node->set_parameter(param_id.c_str(), value);
        }
    }

    slot_to_handle_ = std::move(fresh_handles);
    active_graph_ = graph.get();

    for (SlotId id : slot_order_)
    {
        apply_cabinet_to_live_node(id);
    }

    graph_slot_.publish(std::move(graph));
}

void RigController::set_bypassed(SlotId slot, bool bypassed)
{
    const auto it = slots_.find(slot);
    if (it == slots_.end() || it->second.bypassed == bypassed)
    {
        return;
    }

    const bool previous = it->second.bypassed;
    it->second.bypassed = bypassed;
    try
    {
        publish();
    }
    catch (...)
    {
        it->second.bypassed = previous;
        throw;
    }
}

bool RigController::is_bypassed(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() && it->second.bypassed;
}
