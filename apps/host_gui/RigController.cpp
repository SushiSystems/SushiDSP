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
#include <exception>
#include <memory>
#include <set>
#include <utility>

#include <nlohmann/json.hpp>

#include "SushiDSP/io/IrLoader.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
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
{
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
    // A bypassed slot has no live node, so the descriptor id is what says the slot is an amp.
    const auto it = slots_.find(slot);
    if (it == slots_.end() || it->second.descriptor_id != SushiDSP::jcm800_descriptor().id)
    {
        return false;
    }

    SushiDSP::Ir impulse_response = SushiDSP::IrLoader::load(path, info_.sample_rate);

    const auto handle_it = slot_to_handle_.find(slot);
    if (handle_it != slot_to_handle_.end() && active_graph_ != nullptr)
    {
        auto* amp = dynamic_cast<SushiDSP::Jcm800*>(active_graph_->node(handle_it->second));
        if (amp != nullptr && !amp->set_cabinet(std::move(impulse_response)))
        {
            return false;
        }
    }

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
    auto* amp = dynamic_cast<SushiDSP::Jcm800*>(active_graph_->node(handle_it->second));
    if (amp == nullptr)
    {
        it->second.cabinet_path.clear();
        return;
    }
    try
    {
        if (!amp->set_cabinet(SushiDSP::IrLoader::load(it->second.cabinet_path, info_.sample_rate)))
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
    if (active_graph_ == nullptr)
    {
        return;
    }
    for (const auto& [slot, handle] : slot_to_handle_)
    {
        if (auto* amp = dynamic_cast<SushiDSP::Jcm800*>(active_graph_->node(handle)))
        {
            amp->reclaim_cabinets();
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

std::string RigController::save_to_json() const
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
    const std::size_t node_count = nlohmann::json::parse(json_text).at("nodes").size();
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
        for (const auto& [param_id, value] : slots_.at(id).parameters)
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
