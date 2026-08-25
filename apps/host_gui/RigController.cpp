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

#include "SushiDSP/node_registry/NodeRegistry.hpp"

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
    slots_.insert(slots_.begin() + static_cast<std::ptrdiff_t>(index), std::move(slot));
    rebuild_and_publish();
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
    if (active_graph_ != nullptr)
    {
        SushiDSP::IAudioNode* node = active_graph_->node(static_cast<SushiDSP::NodeHandle>(index));
        if (node != nullptr)
        {
            node->set_parameter(param_id, value);
        }
    }
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

void RigController::load_chain(const std::vector<std::string>& ids, const std::vector<std::map<std::string, float>>& params)
{
    slots_.clear();
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        Slot slot;
        slot.descriptor_id = ids[i];
        slot.parameters = i < params.size() ? params[i] : std::map<std::string, float>{};
        slots_.push_back(std::move(slot));
    }
    rebuild_and_publish();
}

void RigController::rebuild_and_publish()
{
    auto graph = std::make_unique<SushiDSP::RigGraph>(info_.max_block, static_cast<std::uint32_t>(slots_.size() + 1));

    std::vector<SushiDSP::NodeHandle> handles;
    handles.reserve(slots_.size());
    for (const Slot& slot : slots_)
    {
        auto node = SushiDSP::make_node(slot.descriptor_id.c_str(), info_);
        const SushiDSP::NodeHandle handle = graph->add_node(std::move(node));
        handles.push_back(handle);
    }
    for (std::size_t i = 0; i + 1 < handles.size(); ++i)
    {
        graph->connect(handles[i], handles[i + 1]);
    }

    graph->compile(info_);

    for (std::size_t i = 0; i < slots_.size(); ++i)
    {
        SushiDSP::IAudioNode* node = graph->node(handles[i]);
        for (const auto& [param_id, value] : slots_[i].parameters)
        {
            node->set_parameter(param_id.c_str(), value);
        }
    }

    active_graph_ = graph.get();
    graph_slot_.publish(std::move(graph));
}
