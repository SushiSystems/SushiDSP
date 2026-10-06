/**************************************************************************/
/* TopologyApplier.cpp                                                    */
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

/** @file TopologyApplier.cpp
 *  @brief Implements the brick that compiles a RigTopology into a RigController's slots and edges.
 *  @author Mustafa Garip
 */

#include "TopologyApplier.hpp"

#include <algorithm>
#include <map>
#include <string>

namespace
{
    /// @brief Names the descriptor of the hidden slot that fans a split out.
    constexpr const char* kSplitterId = "sushidsp.splitter";

    /// @brief Names the descriptor of the hidden slot that joins a split's branches.
    constexpr const char* kMixerId = "sushidsp.mixer";

    /// @brief Reports whether a list holds a slot.
    /// @return True when `slot` appears in `slots`.
    bool holds(const std::vector<ui::SlotId>& slots, ui::SlotId slot)
    {
        return std::find(slots.begin(), slots.end(), slot) != slots.end();
    }

    /// @brief Reports whether a slot is live in the rig and carries the given descriptor.
    /// @return True when the rig holds `slot` with `descriptor_id`.
    bool is_live_as(const RigController& rig, ui::SlotId slot, const char* descriptor_id)
    {
        return rig.descriptor_id(slot) == descriptor_id;
    }

    /// @brief Maps an id through a rename table, keeping ids the table does not name.
    /// @return The renamed id, or `slot` itself.
    ui::SlotId renamed(const std::map<ui::SlotId, ui::SlotId>& renames, ui::SlotId slot)
    {
        const auto it = renames.find(slot);
        return it == renames.end() ? slot : it->second;
    }
}

void TopologyApplier::apply(RigTopology& topology, RigController& rig)
{
    const RigTopology topology_before = topology;
    const State       state_before = state_;
    Journal           journal;
    journal.edges = rig.edges();

    try
    {
        place_routing(topology, rig, journal);
        remove_strays(topology, rig, journal);
        connect(topology, rig);
        rig.publish();
    }
    catch (...)
    {
        roll_back(journal, topology_before, state_before, topology, rig);
        throw;
    }
}

void TopologyApplier::switch_chain_type(RigTopology& topology, RigController& rig,
                                        ChainType type)
{
    const ChainTypeChange change = topology.set_chain_type(type);

    // The main path is copied into each path that opened; see apps/host_gui/README.md.
    const std::vector<SlotId> main_path = topology.stage(StageAddress{ StageKind::Branch, 0 });
    for (std::size_t branch : change.opened)
    {
        std::size_t index = 0;
        for (SlotId source : main_path)
        {
            const std::optional<RigController::SlotSnapshot> saved = rig.snapshot(source);
            if (!saved.has_value())
            {
                continue;
            }
            const SlotId copy = rig.restore(*saved, false);
            topology.insert(StageAddress{ StageKind::Branch, branch }, index, copy);
            ++index;
        }
    }

    apply(topology, rig);
}

void TopologyApplier::place_routing(const RigTopology& topology, RigController& rig,
                                    Journal& journal)
{
    if (RigTopology::branch_count(topology.chain_type()) > 1)
    {
        state_.splitter = ensure_slot(state_.splitter, kSplitterId, rig, journal);
        state_.mixer = ensure_slot(state_.mixer, kMixerId, rig, journal);
        return;
    }

    if (state_.splitter.has_value() && is_live_as(rig, *state_.splitter, kSplitterId))
    {
        remove_journaled(*state_.splitter, rig, journal);
    }
    if (state_.mixer.has_value() && is_live_as(rig, *state_.mixer, kMixerId))
    {
        remove_journaled(*state_.mixer, rig, journal);
    }
    state_.splitter.reset();
    state_.mixer.reset();
}

void TopologyApplier::remove_strays(const RigTopology& topology, RigController& rig,
                                    Journal& journal)
{
    const std::vector<SlotId> active = topology.active_slots();
    for (SlotId slot : rig.slot_order())
    {
        const bool routing = slot == state_.splitter || slot == state_.mixer;
        if (!routing && !holds(active, slot))
        {
            remove_journaled(slot, rig, journal);
        }
    }
}



void TopologyApplier::connect(const RigTopology& topology, RigController& rig) const
{
    rig.clear_edges(false);
    for (const auto& [from, to] : topology.edges(state_.splitter, state_.mixer))
    {
        rig.connect(from, to, false);
    }
}

TopologyApplier::SlotId TopologyApplier::ensure_slot(std::optional<SlotId> slot,
                                                     const char* descriptor_id,
                                                     RigController& rig, Journal& journal)
{
    if (slot.has_value() && is_live_as(rig, *slot, descriptor_id))
    {
        return *slot;
    }
    const SlotId created = rig.add_node(descriptor_id, false);
    journal.added.push_back(created);
    return created;
}

void TopologyApplier::remove_journaled(SlotId slot, RigController& rig, Journal& journal)
{
    std::optional<RigController::SlotSnapshot> saved = rig.snapshot(slot);
    if (!saved.has_value())
    {
        return;
    }
    journal.removed.emplace_back(slot, std::move(*saved));
    rig.remove(slot, false);
}

void TopologyApplier::roll_back(const Journal& journal, RigTopology topology_before,
                                State state_before, RigTopology& topology, RigController& rig)
{
    for (auto it = journal.added.rbegin(); it != journal.added.rend(); ++it)
    {
        if (rig.snapshot(*it).has_value())
        {
            rig.remove(*it, false);
        }
    }

    std::map<SlotId, SlotId> renames;
    for (const auto& [slot, saved] : journal.removed)
    {
        const SlotId restored = rig.restore(saved, false);
        renames[slot] = restored;
        topology_before.replace(slot, restored);
    }

    rig.clear_edges(false);
    for (const auto& [from, to] : journal.edges)
    {
        rig.connect(renamed(renames, from), renamed(renames, to), false);
    }

    if (state_before.splitter.has_value())
    {
        state_before.splitter = renamed(renames, *state_before.splitter);
    }
    if (state_before.mixer.has_value())
    {
        state_before.mixer = renamed(renames, *state_before.mixer);
    }
    topology = std::move(topology_before);
    state_ = std::move(state_before);

    if (journal.removed.empty())
    {
        return;
    }
    try
    {
        rig.publish();
    }
    catch (...)
    {
        // The pre-apply model compiled once, so a failure here leaves the old graph playing.
    }
}
