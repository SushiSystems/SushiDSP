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

#include "SushiDSP/io/IrLoader.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"
#include "SushiDSP/rig_preset/RigPreset.hpp"

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
}

RigController::RigController(const SushiDSP::PrepareInfo& info)
    : info_(info)
{
    rebuild_and_publish();
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

RigController::SlotId RigController::add_node(const std::string& descriptor_id)
{
    const SlotId id = next_slot_id_++;
    Slot slot;
    slot.descriptor_id = descriptor_id;
    seed_defaults(slot);
    slots_[id] = std::move(slot);
    slot_order_.push_back(id);
    rebuild_and_publish();
    return id;
}

void RigController::connect(SlotId from, SlotId to)
{
    edges_.push_back({ from, to });
    rebuild_and_publish();
}

void RigController::disconnect(SlotId from, SlotId to)
{
    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [from, to](const Edge& edge) { return edge.from == from && edge.to == to; }),
        edges_.end());
    rebuild_and_publish();
}

void RigController::remove(SlotId slot)
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
        edges_.push_back({ *upstream, *downstream });
    }

    slots_.erase(slot);
    slot_order_.erase(std::remove(slot_order_.begin(), slot_order_.end(), slot), slot_order_.end());
    rebuild_and_publish();
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
        edges_.push_back({ slot_order_.back(), id });
    }
    slot_order_.push_back(id);
    rebuild_and_publish();
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
        edges_.push_back({ id, *following });

        const auto position = std::find(slot_order_.begin(), slot_order_.end(), *following);
        slot_order_.insert(position, id);
    }
    else if (!slot_order_.empty())
    {
        edges_.push_back({ id, slot_order_.front() });
        slot_order_.insert(slot_order_.begin(), id);
    }
    else
    {
        slot_order_.push_back(id);
    }

    rebuild_and_publish();
    return id;
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
        edges_.push_back({ *upstream, *downstream });
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
        edges_.push_back({ moved, *following });
        const auto position = std::find(slot_order_.begin(), slot_order_.end(), *following);
        slot_order_.insert(position, moved);
    }
    else if (!slot_order_.empty())
    {
        edges_.push_back({ moved, slot_order_.front() });
        slot_order_.insert(slot_order_.begin(), moved);
    }
    else
    {
        slot_order_.push_back(moved);
    }

    rebuild_and_publish();
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
    const auto handle_it = slot_to_handle_.find(slot);
    if (handle_it == slot_to_handle_.end() || active_graph_ == nullptr)
    {
        return false;
    }
    auto* amp = dynamic_cast<SushiDSP::Jcm800*>(active_graph_->node(handle_it->second));
    if (amp == nullptr)
    {
        return false;
    }
    if (!amp->set_cabinet(SushiDSP::IrLoader::load(path, info_.sample_rate)))
    {
        return false;
    }
    slots_[slot].cabinet_path = path;
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
    return active_graph_ != nullptr ? SushiDSP::serialize_rig_to_json(*active_graph_) : std::string("{\"nodes\":[]}");
}

void RigController::load_from_json(const std::string& json_text)
{
    SushiDSP::RigGraph scratch(info_.max_block, 64);
    SushiDSP::deserialize_rig_from_json(scratch, json_text, info_);
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
        slots_[id] = std::move(slot);
        slot_order_.push_back(id);
        handle_to_slot[handle] = id;
    }
    for (SushiDSP::NodeHandle handle : scratch.nodes())
    {
        for (SushiDSP::NodeHandle upstream_handle : scratch.upstream_of(handle))
        {
            edges_.push_back({ handle_to_slot.at(upstream_handle), handle_to_slot.at(handle) });
        }
    }

    rebuild_and_publish();
}

void RigController::rebuild_and_publish()
{
    auto graph = std::make_unique<SushiDSP::RigGraph>(info_.max_block, static_cast<std::uint32_t>(slot_order_.size() + 1));

    std::vector<SlotId> surviving_order;
    surviving_order.reserve(slot_order_.size());
    std::unordered_map<SlotId, SushiDSP::NodeHandle> fresh_handles;
    for (SlotId id : slot_order_)
    {
        auto node = SushiDSP::make_node(slots_.at(id).descriptor_id.c_str(), info_);
        if (node == nullptr)
        {
            slots_.erase(id);
            continue;
        }
        fresh_handles[id] = graph->add_node(std::move(node));
        surviving_order.push_back(id);
    }
    slot_order_ = std::move(surviving_order);
    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [&fresh_handles](const Edge& edge)
            { return fresh_handles.find(edge.from) == fresh_handles.end()
                  || fresh_handles.find(edge.to) == fresh_handles.end(); }),
        edges_.end());

    for (const Edge& edge : edges_)
    {
        graph->connect(fresh_handles.at(edge.from), fresh_handles.at(edge.to));
    }

    graph->compile(info_);

    for (SlotId id : slot_order_)
    {
        SushiDSP::IAudioNode* node = graph->node(fresh_handles.at(id));
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
