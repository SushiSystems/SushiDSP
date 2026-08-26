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

#include <cstddef>
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

    /// @brief Sets a parameter on the chain slot at `index`. Does NOT republish (no structural
    /// change) and never touches the live node: the value is stored in this controller's own
    /// slot state and queued on the active RigGraph's lock-free parameter ring, where the audio
    /// thread applies it at the top of its next process() call. If the ring is full the change
    /// is dropped and dropped_parameter_changes() is incremented; the call never blocks.
    /// @param index Chain position.
    /// @param param_id The parameter's NodeDescriptor id.
    /// @param value The new value.
    void set_parameter(std::size_t index, const char* param_id, float value);

    /// @brief Reads this controller's stored value for a parameter at chain position `index`.
    float get_parameter(std::size_t index, const char* param_id) const;

    /// @brief Installs a cabinet impulse response on the JCM800 at chain position `slot`.
    /// Loads the file at the controller's prepared sample rate, hands it to the live node, and
    /// remembers the path so a later rebuild (or a preset save) can reproduce it.
    /// @param slot Chain position.
    /// @param path Filesystem path to the impulse response WAV file.
    /// @return True if the slot is a JCM800 and the impulse response was installed.
    /// @throws std::runtime_error if the file cannot be opened or parsed.
    bool set_cabinet(std::size_t slot, const std::string& path);

    /// @brief Reclaims retired graphs and every live JCM800's retired cabinet. Control thread
    /// only; call once per GUI frame.
    void reclaim();

    /// @brief Counts parameter changes dropped because the audio thread's ring was full.
    /// @return The running total since construction.
    std::size_t dropped_parameter_changes() const;

    /// @brief The chain's NodeDescriptor ids, in order.
    std::vector<std::string> chain_ids() const;

    /// @brief Each slot's cabinet impulse response path, in chain order; empty where none.
    std::vector<std::string> chain_cabinet_paths() const;

    /// @brief The currently active, published RigGraph (for the RT thread to acquire from).
    SushiDSP::RigGraph* active_graph();

    /// @brief The RtGraphSlot the audio callback should acquire_for_audio_thread() from.
    SushiDSP::RtGraphSlot& graph_slot();

    /// @brief Replaces the entire chain (used by preset load) and republishes.
    /// @param ids Descriptor ids in chain order.
    /// @param params Per-slot parameter id/value maps, same length and order as ids.
    /// @param cabinet_paths Per-slot cabinet impulse response paths, same length and order as
    /// ids; an empty string means the slot has no cabinet. A shorter vector leaves the
    /// remaining slots without one.
    void load_chain(const std::vector<std::string>& ids,
                    const std::vector<std::map<std::string, float>>& params,
                    const std::vector<std::string>& cabinet_paths = {});

private:
    struct Slot
    {
        std::string descriptor_id;
        std::map<std::string, float> parameters;
        std::string cabinet_path;
    };

    void rebuild_and_publish();

    /// @brief Re-applies `slot.cabinet_path` to the freshly-built node, if both exist.
    /// @param index Chain position of the slot.
    void apply_cabinet_to_live_node(std::size_t index);

    /// @brief Seeds `slot.parameters` with every declared parameter's default_value from
    /// node_registry() for slot.descriptor_id, overwriting any value already present -
    /// callers that need incoming values to win over defaults must call this first and then
    /// overlay their own values on top.
    void seed_defaults(Slot& slot) const;

    SushiDSP::PrepareInfo   info_;
    std::vector<Slot>       slots_;
    SushiDSP::RtGraphSlot   graph_slot_;
    SushiDSP::RigGraph*     active_graph_ = nullptr;
    std::size_t             dropped_parameter_changes_ = 0;
};
