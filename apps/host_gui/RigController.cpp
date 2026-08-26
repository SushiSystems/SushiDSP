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

#include <cstring>
#include <exception>

#include "SushiDSP/io/IrLoader.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"

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

void RigController::append(const std::string& descriptor_id)
{
    insert(slots_.size(), descriptor_id);
}

void RigController::insert(std::size_t index, const std::string& descriptor_id)
{
    Slot slot;
    slot.descriptor_id = descriptor_id;
    seed_defaults(slot);

    slots_.insert(slots_.begin() + static_cast<std::ptrdiff_t>(index), std::move(slot));
    rebuild_and_publish();
}

void RigController::seed_defaults(Slot& slot) const
{
    // Seed the parameter map with every declared parameter's default value so that
    // get_parameter() mirrors the live node's state from the moment it is added, even
    // for parameters nobody has touched yet (e.g. before the first set_parameter() call,
    // or when saving a preset without ever having moved a slider).
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

void RigController::remove(std::size_t index)
{
    if (index < slots_.size())
    {
        slots_.erase(slots_.begin() + static_cast<std::ptrdiff_t>(index));
        rebuild_and_publish();
    }
}

void RigController::move(std::size_t from, std::size_t to)
{
    if (from >= slots_.size() || to >= slots_.size() || from == to)
    {
        return;
    }
    Slot moved = std::move(slots_[from]);
    slots_.erase(slots_.begin() + static_cast<std::ptrdiff_t>(from));
    slots_.insert(slots_.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
    rebuild_and_publish();
}

void RigController::set_parameter(std::size_t index, const char* param_id, float value)
{
    if (index >= slots_.size())
    {
        return;
    }
    slots_[index].parameters[param_id] = value;

    std::uint16_t parameter_index = 0;
    if (!find_parameter_index(slots_[index].descriptor_id, param_id, parameter_index))
    {
        return;
    }

    // rebuild_and_publish() adds every surviving slot to the fresh graph in chain order and
    // never removes one, so RigGraph's handles are handed out as 0, 1, 2, ... and a slot's
    // chain position IS its NodeHandle. Any future change that removes nodes from a live
    // graph, or adds them out of chain order, must keep a per-slot handle instead.
    if (active_graph_ != nullptr)
    {
        const SushiDSP::ParameterChange change{
            static_cast<SushiDSP::NodeHandle>(index), parameter_index, value };
        if (!active_graph_->push_parameter_change(change))
        {
            ++dropped_parameter_changes_;
        }
    }
}

bool RigController::set_cabinet(std::size_t slot, const std::string& path)
{
    if (slot >= slots_.size() || active_graph_ == nullptr)
    {
        return false;
    }
    auto* amp = dynamic_cast<SushiDSP::Jcm800*>(
        active_graph_->node(static_cast<SushiDSP::NodeHandle>(slot)));
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

void RigController::apply_cabinet_to_live_node(std::size_t index)
{
    if (slots_[index].cabinet_path.empty() || active_graph_ == nullptr)
    {
        return;
    }
    auto* amp = dynamic_cast<SushiDSP::Jcm800*>(
        active_graph_->node(static_cast<SushiDSP::NodeHandle>(index)));
    if (amp == nullptr)
    {
        slots_[index].cabinet_path.clear();
        return;
    }

    // A rebuild must not be able to throw its way out of a structural edit just because the
    // impulse response file has since been deleted or corrupted: drop the cabinet instead, so
    // the chain still comes back with a working (dry) amp.
    try
    {
        if (!amp->set_cabinet(SushiDSP::IrLoader::load(slots_[index].cabinet_path, info_.sample_rate)))
        {
            slots_[index].cabinet_path.clear();
        }
    }
    catch (const std::exception&)
    {
        slots_[index].cabinet_path.clear();
    }
}

void RigController::reclaim()
{
    graph_slot_.reclaim();
    if (active_graph_ == nullptr)
    {
        return;
    }
    // Handles are chain positions (see set_parameter), so this walks every live node without
    // nodes()' per-call vector - reclaim() runs once per GUI frame.
    for (std::size_t i = 0; i < slots_.size(); ++i)
    {
        if (auto* amp = dynamic_cast<SushiDSP::Jcm800*>(active_graph_->node(static_cast<SushiDSP::NodeHandle>(i))))
        {
            amp->reclaim_cabinets();
        }
    }
}

std::size_t RigController::dropped_parameter_changes() const
{
    return dropped_parameter_changes_;
}

float RigController::get_parameter(std::size_t index, const char* param_id) const
{
    if (index >= slots_.size())
    {
        return 0.0f;
    }
    const auto it = slots_[index].parameters.find(param_id);
    return it != slots_[index].parameters.end() ? it->second : 0.0f;
}

std::vector<std::string> RigController::chain_cabinet_paths() const
{
    std::vector<std::string> paths;
    paths.reserve(slots_.size());
    for (const Slot& slot : slots_)
    {
        paths.push_back(slot.cabinet_path);
    }
    return paths;
}

std::vector<std::string> RigController::chain_ids() const
{
    std::vector<std::string> ids;
    ids.reserve(slots_.size());
    for (const Slot& slot : slots_)
    {
        ids.push_back(slot.descriptor_id);
    }
    return ids;
}

SushiDSP::RigGraph* RigController::active_graph()
{
    return active_graph_;
}

SushiDSP::RtGraphSlot& RigController::graph_slot()
{
    return graph_slot_;
}

void RigController::load_chain(const std::vector<std::string>& ids,
                               const std::vector<std::map<std::string, float>>& params,
                               const std::vector<std::string>& cabinet_paths)
{
    slots_.clear();
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        Slot slot;
        slot.descriptor_id = ids[i];
        seed_defaults(slot);
        if (i < params.size())
        {
            for (const auto& [param_id, value] : params[i])
            {
                slot.parameters[param_id] = value;
            }
        }
        if (i < cabinet_paths.size())
        {
            slot.cabinet_path = cabinet_paths[i];
        }
        slots_.push_back(std::move(slot));
    }
    rebuild_and_publish();
}

void RigController::rebuild_and_publish()
{
    auto graph = std::make_unique<SushiDSP::RigGraph>(info_.max_block, static_cast<std::uint32_t>(slots_.size() + 1));

    // An unknown descriptor id (e.g. from a corrupt/hand-edited preset file - see Task 7's
    // load_chain() caller) makes SushiDSP::make_node() return nullptr. Such a slot never
    // constructed a real node, so it is dropped from the chain entirely: it is not added to
    // the graph, it does not get a handle, and it does not survive into the rebuilt slots_ -
    // chain_ids() reports only slots that actually produced a live node.
    std::vector<Slot> surviving_slots;
    surviving_slots.reserve(slots_.size());
    std::vector<SushiDSP::NodeHandle> handles;
    handles.reserve(slots_.size());
    for (Slot& slot : slots_)
    {
        auto node = SushiDSP::make_node(slot.descriptor_id.c_str(), info_);
        if (node == nullptr)
        {
            continue;
        }
        const SushiDSP::NodeHandle handle = graph->add_node(std::move(node));
        handles.push_back(handle);
        surviving_slots.push_back(std::move(slot));
    }
    slots_ = std::move(surviving_slots);

    for (std::size_t i = 0; i + 1 < handles.size(); ++i)
    {
        graph->connect(handles[i], handles[i + 1]);
    }

    graph->compile(info_);

    for (std::size_t i = 0; i < slots_.size(); ++i)
    {
        SushiDSP::IAudioNode* node = graph->node(handles[i]);
        if (node == nullptr)
        {
            continue;
        }
        for (const auto& [param_id, value] : slots_[i].parameters)
        {
            node->set_parameter(param_id.c_str(), value);
        }
    }

    active_graph_ = graph.get();

    // Cabinets are installed before publication, while nothing is reading this graph yet;
    // Jcm800::set_cabinet()'s own retire/reclaim handshake covers it after that point.
    for (std::size_t i = 0; i < slots_.size(); ++i)
    {
        apply_cabinet_to_live_node(i);
    }

    graph_slot_.publish(std::move(graph));
}
