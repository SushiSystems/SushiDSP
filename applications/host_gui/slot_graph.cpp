/**************************************************************************/
/* slot_graph.cpp                                                         */
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

/** @file slot_graph.cpp
 *  @brief Implements the rig's slot-and-edge graph and the republishing it drives.
 *  @author Mustafa Garip
 */

#include "slot_graph.hpp"

#include <algorithm>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>

#include "SushiDSP/core/cabinet_host.hpp"
#include "SushiDSP/io/ir_loader.hpp"
#include "SushiDSP/log/log.hpp"
#include "SushiDSP/node_registry/node_registry.hpp"
#include "SushiDSP/room/hrtf_set_loader.hpp"
#include "SushiDSP/room/starter_scene.hpp"
#include "SushiDSP/routing/mixer_node.hpp"
#include "SushiDSP/routing/splitter_node.hpp"

#include "host_gui_log_category.hpp"

namespace
{
    /// @brief Finds a parameter's position in its descriptor's parameters[] array - which is
    /// exactly the index SushiDSP::ParameterChange carries and AudioNode::set_parameter_for_audio_thread()
    /// expects.
    /// @param descriptor_id The owning node's NodeDescriptor id.
    /// @param param_id The parameter's id.
    /// @param out_index Receives the index on success.
    /// @return True if the descriptor and the parameter were both found.
    bool find_parameter_index(const std::string& descriptor_id, const char* param_id,
                              std::uint16_t& out_index)
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
} // namespace

SlotGraph::SlotGraph(const SushiDSP::PrepareInfo& info) : info_(info)
{
    publish();
}

void SlotGraph::seed_defaults(Slot& slot) const
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

SlotGraph::SlotId SlotGraph::add_node(const std::string& descriptor_id, bool publish_now)
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

void SlotGraph::connect(SlotId from, SlotId to, bool publish_now, std::uint32_t from_port)
{
    const bool present = std::any_of(edges_.begin(), edges_.end(),
                                     [from, to, from_port](const Edge& edge)
                                     {
                                         return edge.from == from && edge.to == to &&
                                                edge.from_port == from_port;
                                     });
    if (present)
    {
        return;
    }
    edges_.push_back({from, to, from_port});
    if (publish_now)
    {
        publish();
    }
}

void SlotGraph::disconnect(SlotId from, SlotId to, bool publish_now)
{
    edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
                                [from, to](const Edge& edge)
                                {
                                    return edge.from == from && edge.to == to;
                                }),
                 edges_.end());
    if (publish_now)
    {
        publish();
    }
}

void SlotGraph::remove(SlotId slot, bool publish_now)
{
    // The neighbors are found before the edges are erased; see applications/host_gui/README.md.
    std::optional<SlotId> upstream;
    std::optional<SlotId> downstream;
    for (const Edge& edge : edges_)
    {
        if (edge.to == slot)
        {
            upstream = edge.from;
        }
        if (edge.from == slot)
        {
            downstream = edge.to;
        }
    }

    edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
                                [slot](const Edge& edge)
                                {
                                    return edge.from == slot || edge.to == slot;
                                }),
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

SlotGraph::SlotId SlotGraph::append(const std::string& descriptor_id)
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

SlotGraph::SlotId SlotGraph::insert_before(std::optional<SlotId> following,
                                           const std::string& descriptor_id)
{
    const SlotId id = next_slot_id_++;
    Slot slot;
    slot.descriptor_id = descriptor_id;
    seed_defaults(slot);
    slots_[id] = std::move(slot);

    if (following.has_value())
    {
        // The edge that pointed at `following` now feeds the new slot; see applications/host_gui/README.md.
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

std::vector<SlotGraph::SlotId> SlotGraph::downstream_of(SlotId slot) const
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

SlotGraph::BranchEnd SlotGraph::walk_branch(SlotId start) const
{
    SlotId current = start;
    std::set<SlotId> visited;
    while (visited.insert(current).second)
    {
        if (upstream_of(current).size() >= 2)
        {
            return {current, true};
        }
        const std::vector<SlotId> next = downstream_of(current);
        if (next.size() != 1)
        {
            return {current, false};
        }
        current = next.front();
    }
    // A cycle walked back onto a slot already left behind; stop rather than circle it forever.
    return {current, false};
}

std::optional<SlotGraph::SlotId> SlotGraph::add_branch(SlotId splitter,
                                                       const std::string& descriptor_id)
{
    const auto splitter_slot = slots_.find(splitter);
    if (splitter_slot == slots_.end() ||
        splitter_slot->second.descriptor_id != SushiDSP::splitter_descriptor().id ||
        !descriptor_is_registered(descriptor_id))
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
    const bool rejoin_exists = end.is_join && end_slot != slots_.end() &&
                               end_slot->second.descriptor_id == SushiDSP::mixer_descriptor().id;

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

void SlotGraph::move_before(SlotId moved, std::optional<SlotId> following)
{
    if (following.has_value() && *following == moved)
    {
        return;
    }

    // `moved` is detached as remove() does, but its slot stays alive; see applications/host_gui/README.md.
    std::optional<SlotId> upstream;
    std::optional<SlotId> downstream;
    for (const Edge& edge : edges_)
    {
        if (edge.to == moved)
        {
            upstream = edge.from;
        }
        if (edge.from == moved)
        {
            downstream = edge.to;
        }
    }
    edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
                                [moved](const Edge& edge)
                                {
                                    return edge.from == moved || edge.to == moved;
                                }),
                 edges_.end());
    if (upstream.has_value() && downstream.has_value())
    {
        connect(*upstream, *downstream, false);
    }
    slot_order_.erase(std::remove(slot_order_.begin(), slot_order_.end(), moved),
                      slot_order_.end());

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

void SlotGraph::set_parameter(SlotId slot, const char* param_id, float value)
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
        const SushiDSP::ParameterChange change{handle_it->second, parameter_index, value};
        if (!active_graph_->push_parameter_change(change))
        {
            ++dropped_parameter_changes_;
        }
    }
}

float SlotGraph::get_parameter(SlotId slot, const char* param_id) const
{
    const auto it = slots_.find(slot);
    if (it == slots_.end())
    {
        return 0.0f;
    }
    const auto param_it = it->second.parameters.find(param_id);
    return param_it != it->second.parameters.end() ? param_it->second : 0.0f;
}

bool SlotGraph::set_cabinet(SlotId slot, const std::string& path)
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
        auto* host = dynamic_cast<SushiDSP::CabinetHost*>(active_graph_->node(handle_it->second));
        if (host == nullptr || !host->set_cabinet(std::move(impulse_response)))
        {
            return false;
        }
    }

    // A bypassed slot has no live node to ask, so the path is kept; see applications/host_gui/README.md.
    it->second.cabinet_path = path;
    return true;
}

void SlotGraph::apply_cabinet_to_live_node(SlotId slot)
{
    const auto it = slots_.find(slot);
    const auto handle_it = slot_to_handle_.find(slot);
    if (it == slots_.end() || it->second.cabinet_path.empty() ||
        handle_it == slot_to_handle_.end() || active_graph_ == nullptr)
    {
        return;
    }
    auto* host = dynamic_cast<SushiDSP::CabinetHost*>(active_graph_->node(handle_it->second));
    if (host == nullptr)
    {
        it->second.cabinet_path.clear();
        return;
    }
    try
    {
        if (!host->set_cabinet(
                SushiDSP::IrLoader::load(it->second.cabinet_path, info_.sample_rate)))
        {
            SD_LOG_WARN(K_HOST_GUI_LOG_CATEGORY,
                        "the node of slot {} refused cabinet IR '{}' at {} Hz; the slot forgets "
                        "the path",
                        slot, it->second.cabinet_path, info_.sample_rate);
            it->second.cabinet_path.clear();
        }
    }
    catch (const std::exception& error)
    {
        SD_LOG_WARN(K_HOST_GUI_LOG_CATEGORY,
                    "loading cabinet IR '{}' for slot {} at {} Hz failed: {}; the slot forgets "
                    "the path",
                    it->second.cabinet_path, slot, info_.sample_rate, error.what());
        it->second.cabinet_path.clear();
    }
}

void SlotGraph::reclaim()
{
    graph_slot_.reclaim();
    if (active_graph_ == nullptr)
    {
        return;
    }
    for (const auto& [slot, handle] : slot_to_handle_)
    {
        if (auto* host = dynamic_cast<SushiDSP::CabinetHost*>(active_graph_->node(handle)))
        {
            host->reclaim_cabinets();
        }
        if (auto* room = dynamic_cast<SushiDSP::room::RoomHost*>(active_graph_->node(handle)))
        {
            room->reclaim_rooms();
            std::string reason;
            publish_room(slot, false, reason);
        }
    }
}

std::size_t SlotGraph::dropped_parameter_changes() const
{
    return dropped_parameter_changes_;
}

std::vector<SlotGraph::SlotId> SlotGraph::slot_order() const
{
    return slot_order_;
}

std::string SlotGraph::descriptor_id(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() ? it->second.descriptor_id : std::string();
}

std::string SlotGraph::cabinet_path(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() ? it->second.cabinet_path : std::string();
}

bool SlotGraph::hosts_cabinet(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() && it->second.hosts_cabinet;
}

bool SlotGraph::hosts_room(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() && it->second.hosts_room;
}

SushiDSP::room::RoomHost* SlotGraph::room_host(SlotId slot)
{
    const auto handle_it = slot_to_handle_.find(slot);
    if (handle_it == slot_to_handle_.end() || active_graph_ == nullptr)
    {
        return nullptr;
    }
    return dynamic_cast<SushiDSP::room::RoomHost*>(active_graph_->node(handle_it->second));
}

SushiDSP::Room::Arrangement SlotGraph::room_arrangement(SlotId slot) const
{
    const auto it = slots_.find(slot);
    if (it == slots_.end() || !it->second.room_arrangement.has_value())
    {
        return SushiDSP::Room::Arrangement::from_scene(SushiDSP::room::starter_scene());
    }
    return *it->second.room_arrangement;
}

bool SlotGraph::set_room_arrangement(SlotId slot, const SushiDSP::Room::Arrangement& arrangement,
                                     std::string& error_out)
{
    error_out.clear();
    const auto it = slots_.find(slot);
    if (it == slots_.end())
    {
        error_out = "no slot with that id";
        return false;
    }
    // The arrangement is refused here, on the frame of the drag; see applications/host_gui/README.md.
    if (!arrangement.is_valid(error_out))
    {
        return false;
    }
    it->second.room_arrangement = arrangement;
    it->second.room_pending = true;
    return publish_room(slot, false, error_out);
}

SushiDSP::room::RoomScene SlotGraph::room_scene(SlotId slot) const
{
    const SushiDSP::Room::Arrangement arrangement = room_arrangement(slot);
    SushiDSP::room::RoomScene scene = arrangement.shell();
    scene.listener_position = arrangement.listener().position;
    scene.listener_yaw_rad = arrangement.listener().yaw_rad;
    if (!arrangement.sources().empty())
    {
        scene.source_position = arrangement.sources().front().pose.position;
        scene.source_yaw_rad = arrangement.sources().front().pose.yaw_rad;
        scene.cabinet = arrangement.sources().front().cabinet;
    }
    return scene;
}

bool SlotGraph::set_room_scene(SlotId slot, const SushiDSP::room::RoomScene& scene,
                               std::string& error_out)
{
    error_out.clear();
    if (slots_.find(slot) == slots_.end())
    {
        error_out = "no slot with that id";
        return false;
    }
    if (!scene.is_valid(error_out))
    {
        return false;
    }
    SushiDSP::Room::Arrangement arrangement = room_arrangement(slot);
    arrangement.set_shell(scene);
    arrangement.set_listener({scene.listener_position, scene.listener_yaw_rad});
    if (!arrangement.sources().empty())
    {
        arrangement.move_source(arrangement.sources().front().id,
                                {scene.source_position, scene.source_yaw_rad});
        arrangement.set_source_cabinet(arrangement.sources().front().id, scene.cabinet);
    }
    return set_room_arrangement(slot, arrangement, error_out);
}

std::size_t SlotGraph::room_publishes(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() ? it->second.room_publishes : 0u;
}

bool SlotGraph::publish_room(SlotId slot, bool force, std::string& error_out)
{
    const auto it = slots_.find(slot);
    if (it == slots_.end() || !it->second.room_pending)
    {
        return true;
    }
    SushiDSP::room::RoomHost* host = room_host(slot);
    if (host == nullptr)
    {
        return true;
    }

    const Clock::time_point now = Clock::now();
    if (!force && it->second.room_published_at.has_value())
    {
        const std::chrono::duration<double> since = now - *it->second.room_published_at;
        if (since.count() < 1.0 / K_ROOM_PUBLISH_HZ)
        {
            return true;
        }
    }

    it->second.room_pending = false;
    it->second.room_published_at = now;
    ++it->second.room_publishes;
    return host->set_arrangement(*it->second.room_arrangement, error_out);
}

void SlotGraph::apply_room_to_live_node(SlotId slot)
{
    const auto it = slots_.find(slot);
    SushiDSP::room::RoomHost* host = room_host(slot);
    if (it == slots_.end() || host == nullptr)
    {
        return;
    }
    if (!hrtf_.empty())
    {
        host->set_hrtf(hrtf_);
    }
    if (!it->second.room_arrangement.has_value())
    {
        it->second.room_arrangement = host->arrangement();
        return;
    }
    std::string reason;
    it->second.room_pending = true;
    if (!publish_room(slot, true, reason))
    {
        // A stored arrangement the fresh node refuses is dropped; see applications/host_gui/README.md.
        it->second.room_arrangement = host->arrangement();
    }
}

bool SlotGraph::set_hrtf_path(const std::string& path, std::string& error_out)
{
    error_out.clear();
    SushiDSP::room::HrtfSet set;
    if (!SushiDSP::room::HrtfSetLoader::load(path, info_.sample_rate, set, error_out))
    {
        hrtf_ = SushiDSP::room::HrtfSet{};
        hrtf_path_.clear();
        for (SlotId slot : slot_order_)
        {
            if (SushiDSP::room::RoomHost* host = room_host(slot))
            {
                host->set_hrtf(SushiDSP::room::HrtfSet{});
            }
        }
        return false;
    }

    hrtf_ = std::move(set);
    hrtf_path_ = path;
    bool every_room_took_it = true;
    for (SlotId slot : slot_order_)
    {
        if (SushiDSP::room::RoomHost* host = room_host(slot))
        {
            every_room_took_it = host->set_hrtf(hrtf_) && every_room_took_it;
        }
    }
    if (!every_room_took_it)
    {
        error_out = "a room refused the HRTF set '" + path + "'";
    }
    return every_room_took_it;
}

std::string SlotGraph::hrtf_path() const
{
    return hrtf_path_;
}

std::vector<SlotGraph::SlotId> SlotGraph::upstream_of(SlotId slot) const
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

std::vector<std::pair<SlotGraph::SlotId, SlotGraph::SlotId>> SlotGraph::edges() const
{
    std::vector<std::pair<SlotId, SlotId>> result;
    result.reserve(edges_.size());
    for (const Edge& edge : edges_)
    {
        result.push_back({edge.from, edge.to});
    }
    return result;
}

std::uint32_t SlotGraph::port_of(SlotId from, SlotId to) const
{
    for (const Edge& edge : edges_)
    {
        if (edge.from == from && edge.to == to)
        {
            return edge.from_port;
        }
    }
    return 0;
}

std::vector<SlotGraph::EdgeRecord> SlotGraph::edge_records() const
{
    std::vector<EdgeRecord> result;
    result.reserve(edges_.size());
    for (const Edge& edge : edges_)
    {
        result.push_back({edge.from, edge.to, edge.from_port});
    }
    return result;
}

std::optional<SlotGraph::SlotSnapshot> SlotGraph::snapshot(SlotId slot) const
{
    const auto it = slots_.find(slot);
    if (it == slots_.end())
    {
        return std::nullopt;
    }
    SlotSnapshot result;
    result.descriptor_id = it->second.descriptor_id;
    result.parameters = it->second.parameters;
    result.cabinet_path = it->second.cabinet_path;
    result.bypassed = it->second.bypassed;
    result.room_arrangement = it->second.room_arrangement;
    return result;
}

SlotGraph::SlotId SlotGraph::restore(const SlotSnapshot& snapshot, bool publish_now)
{
    const SlotId id = add_node(snapshot.descriptor_id, false);
    Slot& slot = slots_.at(id);
    // Restored values overlay the defaults add_node() seeded; see applications/host_gui/README.md.
    for (const auto& [param_id, value] : snapshot.parameters)
    {
        slot.parameters[param_id] = value;
    }
    slot.cabinet_path = snapshot.cabinet_path;
    slot.bypassed = snapshot.bypassed;
    slot.room_arrangement = snapshot.room_arrangement;
    if (publish_now)
    {
        publish();
    }
    return id;
}

void SlotGraph::clear_edges(bool publish_now)
{
    edges_.clear();
    if (publish_now)
    {
        publish();
    }
}

void SlotGraph::clear()
{
    slots_.clear();
    edges_.clear();
    slot_order_.clear();
    next_slot_id_ = 0;
    shape_.reset();
    legacy_room_.reset();
}

void SlotGraph::set_shape(const RigShape& shape)
{
    shape_ = shape;
}

const std::optional<RigShape>& SlotGraph::shape() const noexcept
{
    return shape_;
}

void SlotGraph::set_legacy_room(const LegacyRoom& legacy)
{
    legacy_room_ = legacy;
}

std::optional<LegacyRoom> SlotGraph::take_legacy_room()
{
    std::optional<LegacyRoom> taken = std::move(legacy_room_);
    legacy_room_.reset();
    return taken;
}

SushiDSP::RigGraph* SlotGraph::active_graph()
{
    return active_graph_;
}

SushiDSP::RtGraphSlot& SlotGraph::graph_slot()
{
    return graph_slot_;
}

void SlotGraph::set_ir_folder(const std::string& path)
{
    ir_folder_ = path;
}

std::string SlotGraph::ir_folder() const
{
    return ir_folder_;
}

void SlotGraph::set_input_source(RigInputSource source)
{
    input_source_.store(source, std::memory_order_relaxed);
}

std::unique_ptr<SushiDSP::RigGraph>
SlotGraph::build_graph(bool apply_bypass,
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

    // Fan-out is counted per output port, as RigGraph::connect() counts it.
    std::map<std::pair<SlotId, std::uint32_t>, std::size_t> fan_out;
    std::unordered_map<SlotId, std::size_t> fan_in;
    for (const auto& edge : resolved)
    {
        if (++fan_out[{edge.first, port_of(edge.first, edge.second)}] >
                SushiDSP::RigGraph::K_MAX_FAN_IN_OUT ||
            ++fan_in[edge.second] > SushiDSP::RigGraph::K_MAX_FAN_IN_OUT)
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
        graph->connect(handles_out.at(edge.first), handles_out.at(edge.second),
                       port_of(edge.first, edge.second));
    }

    // A slot nothing feeds is a root; it reads the host input channel its slot names.
    std::set<SlotId> fed;
    for (const auto& edge : resolved)
    {
        fed.insert(edge.second);
    }
    for (const auto& [id, handle] : handles_out)
    {
        if (fed.count(id) == 0)
        {
            graph->bind_input(handle, slots_.at(id).input_channel);
        }
    }

    graph->compile(info_);
    return graph;
}

std::vector<std::pair<SlotGraph::SlotId, SlotGraph::SlotId>>
SlotGraph::direct_edges(const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const
{
    std::vector<std::pair<SlotId, SlotId>> result;
    std::set<std::pair<SlotId, SlotId>> seen;
    for (const Edge& edge : edges_)
    {
        if (live.count(edge.from) != 0 && live.count(edge.to) != 0 &&
            seen.insert({edge.from, edge.to}).second)
        {
            result.push_back({edge.from, edge.to});
        }
    }
    return result;
}

std::vector<std::pair<SlotGraph::SlotId, SlotGraph::SlotId>>
SlotGraph::bridged_edges(const std::unordered_map<SlotId, SushiDSP::NodeHandle>& live) const
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
        std::vector<SlotId> frontier{edge.to};
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
                if (seen.insert({edge.from, current}).second)
                {
                    result.push_back({edge.from, current});
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

void SlotGraph::publish()
{
    std::unordered_map<SlotId, SushiDSP::NodeHandle> fresh_handles;
    auto graph = build_graph(true, fresh_handles);

    // A slot the registry no longer knows is dropped here; see applications/host_gui/README.md.
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

    edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
                                [this](const Edge& edge)
                                {
                                    return slots_.count(edge.from) == 0 ||
                                           slots_.count(edge.to) == 0;
                                }),
                 edges_.end());

    for (const auto& [id, handle] : fresh_handles)
    {
        SushiDSP::AudioNode* node = graph->node(handle);
        if (node == nullptr)
        {
            continue;
        }
        Slot& slot = slots_.at(id);
        slot.hosts_cabinet = dynamic_cast<SushiDSP::CabinetHost*>(node) != nullptr;
        slot.hosts_room = dynamic_cast<SushiDSP::room::RoomHost*>(node) != nullptr;
        slot.room_published_at.reset();
        slot.room_publishes = 0;
        for (const auto& [param_id, value] : slot.parameters)
        {
            node->set_parameter_for_control_thread(param_id.c_str(), value);
        }
    }

    slot_to_handle_ = std::move(fresh_handles);
    active_graph_ = graph.get();

    for (SlotId id : slot_order_)
    {
        apply_cabinet_to_live_node(id);
        apply_room_to_live_node(id);
    }

    graph_slot_.publish(std::move(graph));
}

void SlotGraph::set_input_channel(SlotId slot, std::uint32_t channel, bool publish_now)
{
    const auto it = slots_.find(slot);
    if (it == slots_.end() || it->second.input_channel == channel)
    {
        return;
    }
    it->second.input_channel = channel;
    if (publish_now)
    {
        publish();
    }
}

void SlotGraph::set_bypassed(SlotId slot, bool bypassed)
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

bool SlotGraph::is_bypassed(SlotId slot) const
{
    const auto it = slots_.find(slot);
    return it != slots_.end() && it->second.bypassed;
}
