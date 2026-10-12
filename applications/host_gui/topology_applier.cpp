/**************************************************************************/
/* topology_applier.cpp                                                   */
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

/** @file topology_applier.cpp
 *  @brief Implements the brick that compiles a RigTopology into a RigController's slots and edges.
 *  @author Mustafa Garip
 */

#include "topology_applier.hpp"

#include <algorithm>
#include <exception>
#include <map>
#include <set>
#include <string>

#include "SushiDSP/log/log.hpp"
#include "SushiDSP/room/room_environment_node.hpp"

#include "host_gui_log_category.hpp"

namespace
{
    /// @brief Names the descriptor of the hidden slot that fans a split out.
    constexpr const char* K_SPLITTER_ID = "sushidsp.splitter";

    /// @brief Names the descriptor of the hidden slot that joins a split's branches.
    constexpr const char* K_MIXER_ID = "sushidsp.mixer";

    /// @brief Names the descriptor of the hidden bus that joins the Parallel chain's two paths.
    constexpr const char* K_BUS_ID = "sushidsp.bus";

    /// @brief Names the descriptor of the hidden strip that pans a Parallel path to its side.
    constexpr const char* K_CHANNEL_STRIP_ID = "sushidsp.channel_strip";

    /// @brief Names the descriptor of the room environment that places the cabinets and microphones.
    constexpr const char* K_ROOM_ENVIRONMENT_ID = "sushidsp.room_environment";

    /// @brief Names the descriptor of the microphone rig element.
    constexpr const char* K_MICROPHONE_ID = "sushidsp.microphone";

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

    /// @brief Adds the DI path to a chain's edges: the chain's single sink and the DI strip both feed
    /// the master bus. The sink is the one slot no edge leaves; a one-slot path has no edges, so it is
    /// the path's only slot.
    /// @return The edges with the master bus wired in.
    std::vector<std::pair<ui::SlotId, ui::SlotId>>
    with_di(std::vector<std::pair<ui::SlotId, ui::SlotId>> edges,
            const std::vector<ui::SlotId>& active, ui::SlotId di_strip, ui::SlotId master_bus)
    {
        std::set<ui::SlotId> sources;
        for (const auto& edge : edges)
        {
            sources.insert(edge.first);
        }
        std::optional<ui::SlotId> sink;
        for (const auto& edge : edges)
        {
            if (sources.count(edge.second) == 0)
            {
                sink = edge.second;
            }
        }
        if (!sink.has_value() && edges.empty() && active.size() == 1)
        {
            sink = active.front();
        }
        if (sink.has_value())
        {
            edges.push_back({*sink, master_bus});
        }
        edges.push_back({di_strip, master_bus});
        return edges;
    }

    /// @brief Maps an id through a rename table, keeping ids the table does not name.
    /// @return The renamed id, or `slot` itself.
    ui::SlotId renamed(const std::map<ui::SlotId, ui::SlotId>& renames, ui::SlotId slot)
    {
        const auto it = renames.find(slot);
        return it == renames.end() ? slot : it->second;
    }
} // namespace

void TopologyApplier::apply(RigTopology& topology, RigController& rig)
{
    const RigTopology topology_before = topology;
    const State state_before = state_;
    Journal journal;
    journal.edges = rig.edge_records();

    try
    {
        if (topology.room())
        {
            place_room(topology, rig, journal);
        }
        else
        {
            remove_room(rig, journal);
            place_routing(topology, rig, journal);
        }
        place_di(topology, rig, journal);
        remove_strays(topology, rig, journal);
        connect(topology, rig);
        bind_inputs(topology, rig);
        rig.publish();
    }
    catch (...)
    {
        roll_back(journal, topology_before, state_before, topology, rig);
        throw;
    }
}

void TopologyApplier::switch_chain_type(RigTopology& topology, RigController& rig, ChainType type)
{
    const ChainTypeChange change = topology.set_chain_type(type);

    // The main path is copied into each path that opened; see applications/host_gui/README.md.
    const std::vector<SlotId> main_path = topology.stage(StageAddress{StageKind::Branch, 0});
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
            topology.insert(StageAddress{StageKind::Branch, branch}, index, copy);
            ++index;
        }
    }

    apply(topology, rig);
}

void TopologyApplier::place_routing(const RigTopology& topology, RigController& rig,
                                    Journal& journal)
{
    if (topology.chain_type() == ChainType::Parallel)
    {
        if (state_.splitter.has_value() && is_live_as(rig, *state_.splitter, K_SPLITTER_ID))
        {
            remove_journaled(*state_.splitter, rig, journal);
        }
        state_.splitter.reset();
        state_.left_strip = ensure_slot(state_.left_strip, K_CHANNEL_STRIP_ID, rig, journal);
        state_.right_strip = ensure_slot(state_.right_strip, K_CHANNEL_STRIP_ID, rig, journal);
        state_.mixer = ensure_slot(state_.mixer, K_BUS_ID, rig, journal);
        rig.set_parameter(*state_.left_strip, "pan", -1.0f);
        rig.set_parameter(*state_.right_strip, "pan", 1.0f);
        return;
    }

    if (RigTopology::branch_count(topology.chain_type()) > 1)
    {
        state_.splitter = ensure_slot(state_.splitter, K_SPLITTER_ID, rig, journal);
        state_.mixer = ensure_slot(state_.mixer, K_MIXER_ID, rig, journal);
        return;
    }

    if (state_.splitter.has_value() && is_live_as(rig, *state_.splitter, K_SPLITTER_ID))
    {
        remove_journaled(*state_.splitter, rig, journal);
    }
    if (state_.mixer.has_value() && is_live_as(rig, *state_.mixer, K_MIXER_ID))
    {
        remove_journaled(*state_.mixer, rig, journal);
    }
    if (state_.left_strip.has_value() && is_live_as(rig, *state_.left_strip, K_CHANNEL_STRIP_ID))
    {
        remove_journaled(*state_.left_strip, rig, journal);
    }
    if (state_.right_strip.has_value() && is_live_as(rig, *state_.right_strip, K_CHANNEL_STRIP_ID))
    {
        remove_journaled(*state_.right_strip, rig, journal);
    }
    state_.splitter.reset();
    state_.mixer.reset();
    state_.left_strip.reset();
    state_.right_strip.reset();
}

void TopologyApplier::place_room(const RigTopology& topology, RigController& rig, Journal& journal)
{
    const auto release = [&rig, &journal](std::optional<SlotId>& slot)
    {
        if (slot.has_value())
        {
            remove_journaled(*slot, rig, journal);
            slot.reset();
        }
    };

    // The chain's own joins are not used while the room is open: every path ends in the room.
    release(state_.splitter);
    release(state_.mixer);
    release(state_.left_strip);
    release(state_.right_strip);

    const std::size_t paths = RigTopology::branch_count(topology.chain_type());
    for (std::size_t path = 0; path < paths; ++path)
    {
        if (topology.stage(StageAddress{StageKind::Branch, path}).empty())
        {
            throw std::runtime_error("The room needs a slot in every path; path " +
                                     std::to_string(path) + " is empty.");
        }
    }

    const RoomLayout& layout = rig.room_layout();
    if (layout.cabinets().size() < paths)
    {
        throw std::runtime_error("Place the cabinet of path " +
                                 std::to_string(layout.cabinets().size()) + " in the room first.");
    }

    state_.environment = ensure_slot(state_.environment, K_ROOM_ENVIRONMENT_ID, rig, journal);
    state_.room_left = ensure_slot(state_.room_left, K_CHANNEL_STRIP_ID, rig, journal);
    state_.room_right = ensure_slot(state_.room_right, K_CHANNEL_STRIP_ID, rig, journal);
    state_.room_bus = ensure_slot(state_.room_bus, K_BUS_ID, rig, journal);
    rig.set_parameter(*state_.room_left, "pan", -1.0f);
    rig.set_parameter(*state_.room_right, "pan", 1.0f);

    // One microphone slot per layout microphone, keyed by its id so its settings follow it.
    std::map<std::uint32_t, SlotId> kept;
    for (const MicrophoneSpot& spot : layout.microphones())
    {
        std::optional<SlotId> current;
        const auto held = state_.microphones.find(spot.id);
        if (held != state_.microphones.end())
        {
            current = held->second;
        }
        kept[spot.id] = ensure_slot(current, K_MICROPHONE_ID, rig, journal);
    }
    for (const auto& [id, slot] : state_.microphones)
    {
        if (kept.count(id) == 0)
        {
            remove_journaled(slot, rig, journal);
        }
    }
    state_.microphones = kept;
}

void TopologyApplier::remove_room(RigController& rig, Journal& journal)
{
    const auto release = [&rig, &journal](std::optional<SlotId>& slot)
    {
        if (slot.has_value())
        {
            remove_journaled(*slot, rig, journal);
            slot.reset();
        }
    };
    release(state_.environment);
    release(state_.room_left);
    release(state_.room_right);
    release(state_.room_bus);
    for (const auto& [id, slot] : state_.microphones)
    {
        remove_journaled(slot, rig, journal);
    }
    state_.microphones.clear();
}

void TopologyApplier::connect_room(const RigTopology& topology, RigController& rig) const
{
    const std::size_t paths = RigTopology::branch_count(topology.chain_type());
    const SlotId environment = *state_.environment;
    const SlotId bus = *state_.room_bus;

    // Each path runs to its last slot, which feeds the room's cabinet input for that path.
    for (std::size_t path = 0; path < paths; ++path)
    {
        const std::vector<SlotId> slots = topology.stage(StageAddress{StageKind::Branch, path});
        for (std::size_t index = 1; index < slots.size(); ++index)
        {
            rig.connect(slots[index - 1], slots[index], false);
        }
    }
    for (std::size_t path = 0; path < paths; ++path)
    {
        const std::vector<SlotId> slots = topology.stage(StageAddress{StageKind::Branch, path});
        rig.connect(slots.back(), environment, false, 0);
    }

    // Output ports: 0 and 1 are the listener's ears, 2 + m is microphone m.
    rig.connect(environment, *state_.room_left, false, 0);
    rig.connect(environment, *state_.room_right, false, 1);
    rig.connect(*state_.room_left, bus, false, 0);
    rig.connect(*state_.room_right, bus, false, 0);

    const RoomLayout& layout = rig.room_layout();
    for (std::size_t index = 0; index < layout.microphones().size(); ++index)
    {
        const SlotId microphone = state_.microphones.at(layout.microphones()[index].id);
        rig.connect(environment, microphone, false,
                    static_cast<std::uint32_t>(SushiDSP::Room::RoomEnvironmentNode::K_LISTENER_PORTS + index));
        rig.connect(microphone, bus, false, 0);
    }

    SlotId sink = bus;
    const std::vector<SlotId> post = topology.stage(StageAddress{StageKind::Post, 0});
    if (!post.empty())
    {
        rig.connect(sink, post.front(), false);
        for (std::size_t index = 1; index < post.size(); ++index)
        {
            rig.connect(post[index - 1], post[index], false);
        }
        sink = post.back();
    }

    if (topology.di() && state_.di_strip.has_value() && state_.master_bus.has_value())
    {
        rig.connect(sink, *state_.master_bus, false);
        rig.connect(*state_.di_strip, *state_.master_bus, false);
    }
}

void TopologyApplier::set_room(RigTopology& topology, RigController& rig, bool enabled)
{
    if (topology.room() == enabled)
    {
        return;
    }
    topology.set_room(enabled);
    apply(topology, rig);
}

void TopologyApplier::place_di(const RigTopology& topology, RigController& rig, Journal& journal)
{
    if (!topology.di())
    {
        if (state_.di_strip.has_value() && is_live_as(rig, *state_.di_strip, K_CHANNEL_STRIP_ID))
        {
            remove_journaled(*state_.di_strip, rig, journal);
        }
        if (state_.master_bus.has_value() && is_live_as(rig, *state_.master_bus, K_BUS_ID))
        {
            remove_journaled(*state_.master_bus, rig, journal);
        }
        state_.di_strip.reset();
        state_.master_bus.reset();
        return;
    }

    state_.di_strip = ensure_slot(state_.di_strip, K_CHANNEL_STRIP_ID, rig, journal);
    state_.master_bus = ensure_slot(state_.master_bus, K_BUS_ID, rig, journal);
}

void TopologyApplier::remove_strays(const RigTopology& topology, RigController& rig,
                                    Journal& journal)
{
    const std::vector<SlotId> active = topology.active_slots();
    for (SlotId slot : rig.slot_order())
    {
        bool routing = slot == state_.splitter || slot == state_.mixer ||
                       slot == state_.left_strip || slot == state_.right_strip ||
                       slot == state_.di_strip || slot == state_.master_bus ||
                       slot == state_.environment || slot == state_.room_left ||
                       slot == state_.room_right || slot == state_.room_bus;
        for (const auto& [id, mic] : state_.microphones)
        {
            routing = routing || slot == mic;
        }
        if (!routing && !holds(active, slot))
        {
            remove_journaled(slot, rig, journal);
        }
    }
}

void TopologyApplier::connect(const RigTopology& topology, RigController& rig) const
{
    rig.clear_edges(false);
    if (topology.room())
    {
        connect_room(topology, rig);
        return;
    }
    auto edges =
        topology.chain_type() == ChainType::Parallel
            ? (state_.left_strip && state_.right_strip && state_.mixer
                   ? topology.parallel_edges(*state_.left_strip, *state_.right_strip, *state_.mixer)
                   : std::vector<std::pair<SlotId, SlotId>>{})
            : topology.edges(state_.splitter, state_.mixer);
    if (topology.di() && state_.di_strip.has_value() && state_.master_bus.has_value())
    {
        edges = with_di(std::move(edges), topology.active_slots(), *state_.di_strip,
                        *state_.master_bus);
    }
    for (const auto& [from, to] : edges)
    {
        rig.connect(from, to, false);
    }
}

void TopologyApplier::bind_inputs(const RigTopology& topology, RigController& rig) const
{
    // Every slot reads channel 0 unless it is a Parallel path's head, so a path left behind by a
    // chain-type switch does not keep a channel it no longer needs.
    for (SlotId slot : topology.active_slots())
    {
        rig.set_input_channel(slot, 0u, false);
    }
    if (state_.left_strip.has_value())
    {
        rig.set_input_channel(*state_.left_strip, 0u, false);
    }
    if (state_.right_strip.has_value())
    {
        rig.set_input_channel(*state_.right_strip, 0u, false);
    }

    if (topology.chain_type() != ChainType::Parallel)
    {
        return;
    }
    // Each path's head is a root; an empty path's head is its strip, when the chain has one.
    const std::vector<SlotId> left = topology.stage(StageAddress{StageKind::Branch, 0});
    const std::vector<SlotId> right = topology.stage(StageAddress{StageKind::Branch, 1});
    if (!left.empty())
    {
        rig.set_input_channel(left.front(), 0u, false);
    }
    else if (state_.left_strip.has_value())
    {
        rig.set_input_channel(*state_.left_strip, 0u, false);
    }
    if (!right.empty())
    {
        rig.set_input_channel(right.front(), 1u, false);
    }
    else if (state_.right_strip.has_value())
    {
        rig.set_input_channel(*state_.right_strip, 1u, false);
    }
}

TopologyApplier::SlotId TopologyApplier::ensure_slot(std::optional<SlotId> slot,
                                                     const char* descriptor_id, RigController& rig,
                                                     Journal& journal)
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
    for (const SlotGraph::EdgeRecord& edge : journal.edges)
    {
        rig.connect(renamed(renames, edge.from), renamed(renames, edge.to), false, edge.from_port);
    }

    for (std::optional<SlotId>* id :
         {&state_before.splitter, &state_before.mixer, &state_before.left_strip,
          &state_before.right_strip, &state_before.di_strip, &state_before.master_bus,
          &state_before.environment, &state_before.room_left, &state_before.room_right,
          &state_before.room_bus})
    {
        if (id->has_value())
        {
            *id = renamed(renames, **id);
        }
    }
    for (auto& [microphone, slot] : state_before.microphones)
    {
        slot = renamed(renames, slot);
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
    catch (const std::exception& error)
    {
        SD_LOG_WARN(K_HOST_GUI_LOG_CATEGORY,
                    "publishing the rig after rolling back a topology edit with {} removed slots "
                    "failed: {}; the graph published before the edit keeps playing",
                    journal.removed.size(), error.what());
    }
    catch (...)
    {
        SD_LOG_WARN(K_HOST_GUI_LOG_CATEGORY,
                    "publishing the rig after rolling back a topology edit with {} removed slots "
                    "failed: an exception of unknown type; the graph published before the edit "
                    "keeps playing",
                    journal.removed.size());
    }
}

void TopologyApplier::set_di(RigTopology& topology, RigController& rig, bool enabled)
{
    if (topology.di() == enabled)
    {
        return;
    }
    topology.set_di(enabled);
    apply(topology, rig);
}

TopologyApplier::MixerSlots TopologyApplier::mixer_slots() const
{
    MixerSlots slots;
    slots.room_left = state_.room_left;
    slots.room_right = state_.room_right;
    slots.room_bus = state_.room_bus;
    slots.di_strip = state_.di_strip;
    slots.master_bus = state_.master_bus;
    slots.microphones = state_.microphones;
    return slots;
}
