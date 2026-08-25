/**************************************************************************/
/* RigController.hpp                                                      */
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

#pragma once

#include <map>
#include <string>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/core/RtGraphSlot.hpp"

/// @brief Owns the authoritative, ordered rig chain (a list of NodeDescriptor ids plus each
/// slot's current parameter values) and rebuilds + publishes a fresh SushiDSP::RigGraph
/// through an SushiDSP::RtGraphSlot on every structural edit. Not real-time safe — every
/// method here runs on the GUI/main thread only. Not part of the SushiDSP:: namespace: this
/// is app-local glue, not a library product.
class RigController
{
public:
    explicit RigController(const SushiDSP::PrepareInfo& info);

    /// @brief Appends a new node (by NodeDescriptor::id) to the end of the chain and republishes.
    void append(const std::string& descriptor_id);

    /// @brief Inserts a new node at chain position `index` and republishes.
    void insert(std::size_t index, const std::string& descriptor_id);

    /// @brief Removes the chain slot at `index` and republishes.
    void remove(std::size_t index);

    /// @brief Moves the chain slot at `from` to just after `to` (or to the front if to == 0 and
    /// from > 0 moving backward) and republishes. Semantics match std::vector element move:
    /// the element at `from` is removed, then reinserted at index `to` in the resulting vector.
    void move(std::size_t from, std::size_t to);

    /// @brief Sets a parameter on the live node at chain position `index`. Does NOT republish
    /// (no structural change) - writes directly to the live IAudioNode instance. This writes a
    /// plain float from the GUI thread while the audio thread may concurrently read the same
    /// field in process() - an accepted, documented approximation for this project's current
    /// scope (not something to fix now), and it is not a data-structure race (RigGraph's and
    /// RtGraphSlot's structural safety is unaffected). If audible artifacts from a torn read
    /// are ever observed, the proper fix is a lock-free parameter queue.
    void set_parameter(std::size_t index, const char* param_id, float value);

    /// @brief Reads a parameter from the live node at chain position `index`.
    float get_parameter(std::size_t index, const char* param_id) const;

    /// @brief The chain's NodeDescriptor ids, in order.
    std::vector<std::string> chain_ids() const;

    /// @brief The currently active, published RigGraph (for the RT thread to acquire from).
    SushiDSP::RigGraph* active_graph();

    /// @brief The RtGraphSlot the audio callback should acquire_for_audio_thread() from.
    SushiDSP::RtGraphSlot& graph_slot();

    /// @brief Replaces the entire chain (used by preset load) and republishes.
    /// @param ids Descriptor ids in chain order.
    /// @param params Per-slot parameter id/value maps, same length and order as ids.
    void load_chain(const std::vector<std::string>& ids, const std::vector<std::map<std::string, float>>& params);

private:
    struct Slot
    {
        std::string descriptor_id;
        std::map<std::string, float> parameters;
    };

    void rebuild_and_publish();

    /// @brief Seeds `slot.parameters` with every declared parameter's default_value from
    /// node_registry() for slot.descriptor_id, overwriting any value already present -
    /// callers that need incoming values to win over defaults must call this first and then
    /// overlay their own values on top.
    void seed_defaults(Slot& slot) const;

    SushiDSP::PrepareInfo   info_;
    std::vector<Slot>       slots_;
    SushiDSP::RtGraphSlot   graph_slot_;
    SushiDSP::RigGraph*     active_graph_ = nullptr;
};
