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
#include <cstdio>
#include <exception>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

#include "SushiDSP/log/log.hpp"
#include "SushiDSP/room/microphone_model.hpp"

#include "cabinet_section_wiring.hpp"
#include "host_gui_log_category.hpp"

namespace
{
    using SlotId = RigController::SlotId;

    /// @brief Names the descriptor of the hidden slot that fans a split out.
    constexpr const char* K_SPLITTER_ID = "sushidsp.splitter";

    /// @brief Names the descriptor of the hidden slot that joins a split's branches.
    constexpr const char* K_MIXER_ID = "sushidsp.mixer";

    /// @brief Names the descriptor of every hidden bus: the Parallel join, the microphones' bus,
    /// the Room sum, the amp bus and the master bus.
    constexpr const char* K_BUS_ID = "sushidsp.bus";

    /// @brief Names the descriptor of a mono strip: a Parallel path's, a microphone's, the DI's.
    constexpr const char* K_CHANNEL_STRIP_ID = "sushidsp.channel_strip";

    /// @brief Names the descriptor of a stereo strip: the listener's, the Room's, the master's.
    constexpr const char* K_STEREO_STRIP_ID = "sushidsp.stereo_strip";

    /// @brief Names the descriptor of the room.
    constexpr const char* K_ROOM_ID = "sushidsp.room";

    /// @brief Names the descriptor of the microphone rig element.
    constexpr const char* K_MICROPHONE_ID = "sushidsp.microphone";

    /// @brief Names the descriptor of the cabinet, the room's source.
    constexpr const char* K_CABINET_ID = "sushidsp.cabinet";

    /// @brief Holds the number of inputs a bus carries mute and solo flags for.
    constexpr std::size_t K_BUS_INPUTS = 8;

    /// @brief Holds how far in front of a cabinet a new microphone stands, in metres.
    constexpr double K_MICROPHONE_DISTANCE_M = 0.3;

    /// @brief Holds how far apart new microphones stand across the cabinet, in metres.
    constexpr double K_MICROPHONE_SPACING_M = 0.15;

    /// @brief Holds the margin a default pose keeps from every wall, in metres.
    constexpr double K_WALL_MARGIN_M = 0.2;

    /// @brief Holds half a turn, the yaw of a microphone that faces a cabinet facing +x.
    constexpr double K_HALF_TURN_RAD = 3.14159265358979323846;

    /// @brief Reports whether a list holds a slot.
    /// @return True when `slot` appears in `slots`.
    bool holds(const std::vector<SlotId>& slots, SlotId slot)
    {
        return std::find(slots.begin(), slots.end(), slot) != slots.end();
    }

    /// @brief Reports whether a slot is live in the rig and carries the given descriptor.
    /// @return True when the rig holds `slot` with `descriptor_id`.
    bool is_live_as(const RigController& rig, SlotId slot, const char* descriptor_id)
    {
        return rig.descriptor_id(slot) == descriptor_id;
    }

    /// @brief Adds the DI path to the edges of a rig with no cabinet section: the chain's single
    /// sink and the DI strip both feed the master bus. The sink is the one slot no edge leaves; a
    /// one-slot path has no edges, so it is the path's only slot.
    /// @return The edges with the master bus wired in.
    std::vector<std::pair<SlotId, SlotId>>
    with_di(std::vector<std::pair<SlotId, SlotId>> edges, const std::vector<SlotId>& active,
            SlotId di_strip, SlotId master_bus)
    {
        std::set<SlotId> sources;
        for (const auto& edge : edges)
        {
            sources.insert(edge.first);
        }
        std::optional<SlotId> sink;
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
    SlotId renamed(const std::map<SlotId, SlotId>& renames, SlotId slot)
    {
        const auto it = renames.find(slot);
        return it == renames.end() ? slot : it->second;
    }

    /// @brief Keeps a coordinate inside a room's span, a margin away from both walls.
    /// @return The clamped coordinate.
    double inside(double value, double span)
    {
        return std::max(K_WALL_MARGIN_M, std::min(value, span - K_WALL_MARGIN_M));
    }

    /// @brief Returns where the cabinet of one path stands until it is moved: a quarter of the
    /// way along the room, facing +x. The main path's stands on the centre line, where the starter
    /// room puts its cabinet, and the paths beside it stand a quarter of the width to either side.
    SushiDSP::Room::Pose default_source_pose(const SushiDSP::room::RoomScene& shell,
                                             std::size_t path)
    {
        constexpr double K_ACROSS[3] = {0.5, 0.25, 0.75};
        const SushiDSP::room::Vec3 size = shell.dimensions;
        const double across = K_ACROSS[std::min<std::size_t>(path, 2)];
        return SushiDSP::Room::Pose{
            {inside(0.25 * size.x, size.x), inside(across * size.y, size.y), inside(0.5, size.z)},
            0.0};
    }

    /// @brief Returns where a new microphone stands: in front of a cabinet, facing it, each
    /// further microphone a step across.
    SushiDSP::Room::Pose default_microphone_pose(const SushiDSP::room::RoomScene& shell,
                                                 const SushiDSP::Room::Pose& cabinet,
                                                 std::size_t index)
    {
        const SushiDSP::room::Vec3 size = shell.dimensions;
        const double step = K_MICROPHONE_SPACING_M * static_cast<double>(index);
        return SushiDSP::Room::Pose{{inside(cabinet.position.x + K_MICROPHONE_DISTANCE_M, size.x),
                                     inside(cabinet.position.y + step, size.y),
                                     inside(cabinet.position.z, size.z)},
                                    K_HALF_TURN_RAD};
    }

    /// @brief Reads a microphone slot's gear data into an arrangement entry.
    void read_gear(const RigController& rig, SlotId slot,
                   SushiDSP::Room::MicrophonePlacement& placement)
    {
        const int pattern = static_cast<int>(rig.get_parameter(slot, "pattern") + 0.5f);
        placement.pattern = static_cast<SushiDSP::room::PolarPattern>(std::clamp(pattern, 0, 3));
        placement.proximity = rig.get_parameter(slot, "proximity") >= 0.5f;
    }

    /// @brief Returns the first cabinet a path holds.
    /// @return The cabinet's slot, or nullopt when the path carries none.
    std::optional<SlotId> cabinet_of(const RigController& rig, const std::vector<SlotId>& path)
    {
        for (const SlotId slot : path)
        {
            if (rig.hosts_cabinet(slot) || is_live_as(rig, slot, K_CABINET_ID))
            {
                return slot;
            }
        }
        return std::nullopt;
    }

    /// @brief Returns the name of one bus flag.
    std::string flag_name(const char* kind, std::size_t input)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "%s_%zu", kind, input);
        return name;
    }
} // namespace

void TopologyApplier::apply(RigTopology& topology, RigController& rig)
{
    apply_with(topology, rig, MicrophoneEdit{});
}

void TopologyApplier::apply_with(RigTopology& topology, RigController& rig,
                                 const MicrophoneEdit& edit)
{
    const RigTopology topology_before = topology;
    const State state_before = state_;
    Journal journal;
    journal.edges = rig.edge_records();
    if (state_.room.has_value() && is_live_as(rig, *state_.room, K_ROOM_ID))
    {
        journal.arrangement = {*state_.room, rig.room_arrangement(*state_.room)};
    }

    try
    {
        place_routing(topology, rig, journal);
        place_section(topology, rig, journal, edit);
        place_di(topology, rig, journal);
        remove_strays(topology, rig, journal);
        reconcile_room(topology, rig, journal);
        connect(topology, rig, journal);
        bind_inputs(topology, rig);
        rig.publish();
    }
    catch (...)
    {
        roll_back(journal, topology_before, state_before, topology, rig);
        throw;
    }

    RigShape shape;
    shape.chain_type = topology.chain_type();
    const std::size_t paths = RigTopology::branch_count(topology.chain_type());
    for (std::size_t path = 0; path < paths; ++path)
    {
        shape.branches.push_back(topology.stage(StageAddress{StageKind::Branch, path}));
    }
    shape.post = topology.stage(StageAddress{StageKind::Post, 0});
    shape.di = topology.di();
    shape.di_position = topology.di_position();
    shape.hidden = state_;
    rig.set_shape(shape);
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

void TopologyApplier::set_di(RigTopology& topology, RigController& rig, bool enabled)
{
    if (topology.di() == enabled)
    {
        return;
    }
    topology.set_di(enabled);
    apply(topology, rig);
}

void TopologyApplier::set_di_position(RigTopology& topology, RigController& rig,
                                      DiPosition position)
{
    if (topology.di_position() == position)
    {
        return;
    }
    topology.set_di_position(position);
    apply(topology, rig);
}

TopologyApplier::SlotId TopologyApplier::add_microphone(RigTopology& topology, RigController& rig)
{
    if (topology.active_slots().empty())
    {
        throw std::runtime_error("Place gear in the rig before adding a microphone.");
    }
    if (state_.microphones.size() >= SushiDSP::Room::Arrangement::K_MAX_MICROPHONES)
    {
        throw std::runtime_error(
            "A room takes at most " +
            std::to_string(SushiDSP::Room::Arrangement::K_MAX_MICROPHONES) + " microphones.");
    }
    MicrophoneEdit edit;
    edit.add = true;
    apply_with(topology, rig, edit);
    return state_.microphones.back().microphone;
}

void TopologyApplier::remove_microphone(RigTopology& topology, RigController& rig,
                                        SlotId microphone)
{
    const bool held = std::any_of(state_.microphones.begin(), state_.microphones.end(),
                                  [microphone](const MicrophoneSlots& slots)
                                  {
                                      return slots.microphone == microphone;
                                  });
    if (!held)
    {
        throw std::runtime_error("That slot is not one of the room's microphones.");
    }
    MicrophoneEdit edit;
    edit.remove = microphone;
    apply_with(topology, rig, edit);
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
        if (state_.mixer.has_value() && is_live_as(rig, *state_.mixer, K_MIXER_ID))
        {
            remove_journaled(*state_.mixer, rig, journal);
            state_.mixer.reset();
        }
        state_.left_strip = ensure_slot(state_.left_strip, K_CHANNEL_STRIP_ID, rig, journal);
        state_.right_strip = ensure_slot(state_.right_strip, K_CHANNEL_STRIP_ID, rig, journal);
        state_.mixer = ensure_slot(state_.mixer, K_BUS_ID, rig, journal);
        rig.set_parameter(*state_.left_strip, "pan", -1.0f);
        rig.set_parameter(*state_.right_strip, "pan", 1.0f);
        return;
    }

    if (state_.left_strip.has_value() && is_live_as(rig, *state_.left_strip, K_CHANNEL_STRIP_ID))
    {
        remove_journaled(*state_.left_strip, rig, journal);
    }
    if (state_.right_strip.has_value() && is_live_as(rig, *state_.right_strip, K_CHANNEL_STRIP_ID))
    {
        remove_journaled(*state_.right_strip, rig, journal);
    }
    state_.left_strip.reset();
    state_.right_strip.reset();
    if (state_.mixer.has_value() && is_live_as(rig, *state_.mixer, K_BUS_ID))
    {
        remove_journaled(*state_.mixer, rig, journal);
        state_.mixer.reset();
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
    state_.splitter.reset();
    state_.mixer.reset();
}

void TopologyApplier::place_section(const RigTopology& topology, RigController& rig,
                                    Journal& journal, const MicrophoneEdit& edit)
{
    if (topology.active_slots().empty())
    {
        // A rig with no gear passes its input through; it carries no section to pass it past.
        release_slot(state_.room, rig, journal);
        release_slot(state_.listener_strip, rig, journal);
        release_slot(state_.bus, rig, journal);
        release_slot(state_.room_sum, rig, journal);
        release_slot(state_.room_strip, rig, journal);
        release_slot(state_.amp_bus, rig, journal);
        release_slot(state_.master_strip, rig, journal);
        for (const MicrophoneSlots& slots : state_.microphones)
        {
            remove_journaled(slots.microphone, rig, journal);
            remove_journaled(slots.strip, rig, journal);
        }
        state_.microphones.clear();
        return;
    }

    state_.room = ensure_slot(state_.room, K_ROOM_ID, rig, journal);
    state_.listener_strip = ensure_slot(state_.listener_strip, K_STEREO_STRIP_ID, rig, journal);
    state_.amp_bus = ensure_slot(state_.amp_bus, K_BUS_ID, rig, journal);
    state_.master_strip = ensure_slot(state_.master_strip, K_STEREO_STRIP_ID, rig, journal);

    // A microphone the rig lost, with a preset or a rollback, leaves the list with its strip.
    std::vector<MicrophoneSlots> kept;
    for (const MicrophoneSlots& slots : state_.microphones)
    {
        const bool live = is_live_as(rig, slots.microphone, K_MICROPHONE_ID) &&
                          is_live_as(rig, slots.strip, K_CHANNEL_STRIP_ID);
        if (live && edit.remove != slots.microphone)
        {
            kept.push_back(slots);
            continue;
        }
        remove_journaled(slots.microphone, rig, journal);
        remove_journaled(slots.strip, rig, journal);
    }
    state_.microphones = std::move(kept);
    if (edit.add)
    {
        MicrophoneSlots slots;
        slots.microphone = ensure_slot(std::nullopt, K_MICROPHONE_ID, rig, journal);
        slots.strip = ensure_slot(std::nullopt, K_CHANNEL_STRIP_ID, rig, journal);
        state_.microphones.push_back(slots);
    }

    std::vector<MicrophoneWire> wires;
    for (const MicrophoneSlots& slots : state_.microphones)
    {
        wires.push_back(MicrophoneWire{slots.microphone, slots.strip,
                                       rig.get_parameter(slots.microphone, "room") >= 0.5f});
    }
    const CabinetSectionNeeds needs =
        CabinetSectionNeeds::of(wires, topology.di(), topology.di_position());
    if (needs.bus)
    {
        state_.bus = ensure_slot(state_.bus, K_BUS_ID, rig, journal);
    }
    else
    {
        release_slot(state_.bus, rig, journal);
    }
    if (needs.room_strip)
    {
        state_.room_sum = ensure_slot(state_.room_sum, K_BUS_ID, rig, journal);
        state_.room_strip = ensure_slot(state_.room_strip, K_STEREO_STRIP_ID, rig, journal);
    }
    else
    {
        release_slot(state_.room_sum, rig, journal);
        release_slot(state_.room_strip, rig, journal);
    }
}

void TopologyApplier::place_di(const RigTopology& topology, RigController& rig, Journal& journal)
{
    if (!topology.di())
    {
        release_slot(state_.di_strip, rig, journal);
        release_slot(state_.master_bus, rig, journal);
        return;
    }

    state_.di_strip = ensure_slot(state_.di_strip, K_CHANNEL_STRIP_ID, rig, journal);
    // With no section the DI joins the chain in a bus of its own, wherever the setting points.
    const bool section = state_.room.has_value();
    if (!section || topology.di_position() == DiPosition::AfterEffects)
    {
        state_.master_bus = ensure_slot(state_.master_bus, K_BUS_ID, rig, journal);
    }
    else
    {
        release_slot(state_.master_bus, rig, journal);
    }
}

void TopologyApplier::reconcile_room(const RigTopology& topology, RigController& rig,
                                     Journal&) const
{
    if (!state_.room.has_value())
    {
        return;
    }
    const SushiDSP::Room::Arrangement before = rig.room_arrangement(*state_.room);
    SushiDSP::Room::Arrangement after;
    after.set_shell(before.shell());
    after.set_listener(before.listener());
    std::string reason;

    // A cabinet the room has not met takes the pose of the source that stood in its slot.
    const std::size_t paths = RigTopology::branch_count(topology.chain_type());
    for (std::size_t path = 0; path < paths; ++path)
    {
        const std::optional<SlotId> cabinet =
            cabinet_of(rig, topology.stage(StageAddress{StageKind::Branch, path}));
        if (!cabinet.has_value())
        {
            continue;
        }
        const std::size_t index = after.sources().size();
        SushiDSP::Room::SourcePlacement source;
        source.id = *cabinet;
        if (const SushiDSP::Room::SourcePlacement* known = before.find_source(*cabinet))
        {
            source.pose = known->pose;
        }
        else if (index < before.sources().size())
        {
            source.pose = before.sources()[index].pose;
        }
        else
        {
            source.pose = default_source_pose(after.shell(), path);
        }
        if (!after.add_source(source, reason))
        {
            throw std::runtime_error(reason);
        }
    }

    const SushiDSP::Room::Pose anchor = after.sources().empty()
                                            ? default_source_pose(after.shell(), 0)
                                            : after.sources().front().pose;
    for (std::size_t index = 0; index < state_.microphones.size(); ++index)
    {
        SushiDSP::Room::MicrophonePlacement placement;
        placement.id = state_.microphones[index].microphone;
        if (const SushiDSP::Room::MicrophonePlacement* known =
                before.find_microphone(placement.id))
        {
            placement.pose = known->pose;
        }
        else if (index < before.microphones().size() &&
                 !std::any_of(state_.microphones.begin(), state_.microphones.end(),
                              [&](const MicrophoneSlots& slots)
                              {
                                  return slots.microphone == before.microphones()[index].id;
                              }))
        {
            placement.pose = before.microphones()[index].pose;
        }
        else
        {
            placement.pose = default_microphone_pose(after.shell(), anchor, index);
        }
        read_gear(rig, placement.id, placement);
        if (!after.add_microphone(placement, reason))
        {
            throw std::runtime_error(reason);
        }
    }

    if (!rig.set_room_arrangement(*state_.room, after, reason))
    {
        throw std::runtime_error(reason);
    }
}

bool TopologyApplier::refresh_room(RigController& rig, std::string& error_out) const
{
    error_out.clear();
    if (!state_.room.has_value())
    {
        return true;
    }
    SushiDSP::Room::Arrangement arrangement = rig.room_arrangement(*state_.room);
    for (const MicrophoneSlots& slots : state_.microphones)
    {
        const SushiDSP::Room::MicrophonePlacement* known =
            arrangement.find_microphone(slots.microphone);
        if (known == nullptr)
        {
            continue;
        }
        SushiDSP::Room::MicrophonePlacement placement = *known;
        read_gear(rig, slots.microphone, placement);
        arrangement.update_microphone(placement);
    }
    return rig.set_room_arrangement(*state_.room, arrangement, error_out);
}

void TopologyApplier::remove_strays(const RigTopology& topology, RigController& rig,
                                    Journal& journal)
{
    const std::vector<SlotId> active = topology.active_slots();
    std::set<SlotId> routing;
    State walked = state_;
    walked.join.reset();
    walked.bus_feeders.clear();
    walked.room_sum_feeders.clear();
    walked.amp_bus_feeders.clear();
    walked.master_bus_feeders.clear();
    walked.for_each_id(
        [&routing](SlotId& id)
        {
            routing.insert(id);
        });
    for (SlotId slot : rig.slot_order())
    {
        if (routing.count(slot) == 0 && !holds(active, slot))
        {
            remove_journaled(slot, rig, journal);
        }
    }
}

void TopologyApplier::connect(const RigTopology& topology, RigController& rig, Journal& journal)
{
    rig.clear_edges(false);
    auto chain =
        topology.chain_type() == ChainType::Parallel
            ? (state_.left_strip && state_.right_strip && state_.mixer
                   ? topology.parallel_edges(*state_.left_strip, *state_.right_strip, *state_.mixer)
                   : std::vector<std::pair<SlotId, SlotId>>{})
            : topology.edges(state_.splitter, state_.mixer);

    if (!state_.room.has_value())
    {
        // No gear, so no section: the DI path alone, when it is on.
        if (topology.di() && state_.di_strip.has_value() && state_.master_bus.has_value())
        {
            chain = with_di(std::move(chain), topology.active_slots(), *state_.di_strip,
                            *state_.master_bus);
        }
        for (const auto& [from, to] : chain)
        {
            rig.connect(from, to, false);
        }
        state_.join.reset();
        state_.bus_feeders.clear();
        state_.room_sum_feeders.clear();
        state_.amp_bus_feeders.clear();
        state_.master_bus_feeders.clear();
        return;
    }

    CabinetSectionRequest request;
    request.post = topology.active_post();
    // The section places the post stage itself, after the amp bus.
    for (const auto& edge : chain)
    {
        if (!holds(request.post, edge.first) && !holds(request.post, edge.second))
        {
            request.chain_edges.push_back(edge);
        }
    }
    const std::size_t paths = RigTopology::branch_count(topology.chain_type());
    const std::vector<SlotId>& main_path = topology.stage(StageAddress{StageKind::Branch, 0});
    request.join = paths > 1 && state_.mixer.has_value() ? *state_.mixer : main_path.back();
    for (std::size_t path = 0; path < paths; ++path)
    {
        const std::vector<SlotId>& slots = topology.stage(StageAddress{StageKind::Branch, path});
        if (cabinet_of(rig, slots).has_value())
        {
            request.sources.push_back(slots.back());
        }
    }
    for (const MicrophoneSlots& slots : state_.microphones)
    {
        request.microphones.push_back(MicrophoneWire{
            slots.microphone, slots.strip, rig.get_parameter(slots.microphone, "room") >= 0.5f});
    }
    request.di_position = topology.di_position();
    request.room = *state_.room;
    request.listener_strip = *state_.listener_strip;
    request.amp_bus = *state_.amp_bus;
    request.master_strip = *state_.master_strip;
    request.bus = state_.bus;
    request.room_sum = state_.room_sum;
    request.room_strip = state_.room_strip;
    request.di_strip = topology.di() ? state_.di_strip : std::nullopt;
    request.master_bus = state_.master_bus;

    const CabinetSectionWiring wiring = wire_cabinet_section(request);
    for (const CabinetSectionEdge& edge : wiring.edges)
    {
        rig.connect(edge.from, edge.to, false, edge.from_port);
    }

    // The listener starts muted: a rig sounds as its cabinets do until the room is brought in.
    const bool listener_is_new = !holds(state_.amp_bus_feeders, *state_.listener_strip);
    remap_flags(rig, *state_.amp_bus, state_.amp_bus_feeders, wiring.amp_bus_feeders,
                listener_is_new ? state_.listener_strip : std::nullopt, journal);
    if (state_.bus.has_value())
    {
        remap_flags(rig, *state_.bus, state_.bus_feeders, wiring.bus_feeders, std::nullopt,
                    journal);
    }
    if (state_.room_sum.has_value())
    {
        remap_flags(rig, *state_.room_sum, state_.room_sum_feeders, wiring.room_sum_feeders,
                    std::nullopt, journal);
    }
    if (state_.master_bus.has_value())
    {
        remap_flags(rig, *state_.master_bus, state_.master_bus_feeders, wiring.master_bus_feeders,
                    std::nullopt, journal);
    }
    state_.join = request.join;
    state_.bus_feeders = state_.bus.has_value() ? wiring.bus_feeders : std::vector<SlotId>{};
    state_.room_sum_feeders =
        state_.room_sum.has_value() ? wiring.room_sum_feeders : std::vector<SlotId>{};
    state_.amp_bus_feeders = wiring.amp_bus_feeders;
    state_.master_bus_feeders =
        state_.master_bus.has_value() ? wiring.master_bus_feeders : std::vector<SlotId>{};
}

void TopologyApplier::remap_flags(RigController& rig, SlotId bus,
                                  const std::vector<SlotId>& before,
                                  const std::vector<SlotId>& after,
                                  std::optional<SlotId> muted_at_first, Journal& journal)
{
    for (const char* kind : {"mute", "solo"})
    {
        std::vector<float> held(K_BUS_INPUTS, 0.0f);
        for (std::size_t input = 0; input < K_BUS_INPUTS; ++input)
        {
            held[input] = rig.get_parameter(bus, flag_name(kind, input).c_str());
        }
        for (std::size_t input = 0; input < K_BUS_INPUTS; ++input)
        {
            float value = 0.0f;
            if (input < after.size())
            {
                const auto was = std::find(before.begin(), before.end(), after[input]);
                if (was != before.end())
                {
                    const std::size_t index = static_cast<std::size_t>(was - before.begin());
                    value = index < K_BUS_INPUTS ? held[index] : 0.0f;
                }
                else if (muted_at_first == after[input] && std::string(kind) == "mute")
                {
                    value = 1.0f;
                }
            }
            if (value != held[input])
            {
                const std::string name = flag_name(kind, input);
                journal.flags.emplace_back(bus, name, held[input]);
                rig.set_parameter(bus, name.c_str(), value);
            }
        }
    }
}

void TopologyApplier::bind_inputs(const RigTopology& topology, RigController& rig) const
{
    // Every slot reads channel 0 unless it is a Parallel path's head.
    for (SlotId slot : topology.active_slots())
    {
        rig.set_input_channel(slot, 0u, false);
    }
    for (const std::optional<SlotId>& strip : {state_.left_strip, state_.right_strip,
                                               state_.di_strip})
    {
        if (strip.has_value())
        {
            rig.set_input_channel(*strip, 0u, false);
        }
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

void TopologyApplier::release_slot(std::optional<SlotId>& slot, RigController& rig,
                                   Journal& journal)
{
    if (slot.has_value())
    {
        remove_journaled(*slot, rig, journal);
        slot.reset();
    }
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

    for (auto it = journal.flags.rbegin(); it != journal.flags.rend(); ++it)
    {
        const SlotId bus = renamed(renames, std::get<0>(*it));
        if (rig.snapshot(bus).has_value())
        {
            rig.set_parameter(bus, std::get<1>(*it).c_str(), std::get<2>(*it));
        }
    }

    state_before.for_each_id(
        [&renames](SlotId& id)
        {
            id = renamed(renames, id);
        });

    if (journal.arrangement.has_value())
    {
        SushiDSP::Room::Arrangement arrangement = journal.arrangement->second;
        for (const auto& [from, to] : renames)
        {
            arrangement.rename(from, to);
        }
        std::string reason;
        const SlotId room = renamed(renames, journal.arrangement->first);
        if (!rig.set_room_arrangement(room, arrangement, reason))
        {
            SD_LOG_WARN(K_HOST_GUI_LOG_CATEGORY,
                        "putting the room's arrangement back after rolling back a topology edit "
                        "failed: {}; the room keeps the arrangement of the refused edit",
                        reason);
        }
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

TopologyApplier::MixerSlots TopologyApplier::mixer_slots() const
{
    return state_;
}

void TopologyApplier::adopt(const HiddenSlots& slots, const RigController& rig)
{
    const auto live = [&rig](const std::optional<SlotId>& slot,
                             const char* descriptor_id) -> std::optional<SlotId>
    {
        return slot.has_value() && is_live_as(rig, *slot, descriptor_id) ? slot : std::nullopt;
    };
    state_ = slots;
    state_.splitter = live(slots.splitter, K_SPLITTER_ID);
    state_.mixer = live(slots.mixer, K_MIXER_ID);
    if (!state_.mixer.has_value())
    {
        state_.mixer = live(slots.mixer, K_BUS_ID);
    }
    state_.left_strip = live(slots.left_strip, K_CHANNEL_STRIP_ID);
    state_.right_strip = live(slots.right_strip, K_CHANNEL_STRIP_ID);
    state_.room = live(slots.room, K_ROOM_ID);
    state_.listener_strip = live(slots.listener_strip, K_STEREO_STRIP_ID);
    state_.bus = live(slots.bus, K_BUS_ID);
    state_.room_sum = live(slots.room_sum, K_BUS_ID);
    state_.room_strip = live(slots.room_strip, K_STEREO_STRIP_ID);
    state_.amp_bus = live(slots.amp_bus, K_BUS_ID);
    state_.di_strip = live(slots.di_strip, K_CHANNEL_STRIP_ID);
    state_.master_bus = live(slots.master_bus, K_BUS_ID);
    state_.master_strip = live(slots.master_strip, K_STEREO_STRIP_ID);
    state_.microphones.clear();
    for (const MicrophoneSlots& microphone : slots.microphones)
    {
        if (is_live_as(rig, microphone.microphone, K_MICROPHONE_ID) &&
            is_live_as(rig, microphone.strip, K_CHANNEL_STRIP_ID))
        {
            state_.microphones.push_back(microphone);
        }
    }
}

void TopologyApplier::migrate_legacy_room(RigTopology& topology, RigController& rig,
                                          const LegacyRoom& legacy)
{
    if (!state_.room.has_value())
    {
        SD_LOG_WARN(K_HOST_GUI_LOG_CATEGORY,
                    "the preset's room was dropped: the rig holds no gear to stand in it");
        return;
    }
    if (!legacy.ears)
    {
        add_microphone(topology, rig);
    }

    SushiDSP::Room::Arrangement arrangement = rig.room_arrangement(*state_.room);
    arrangement.set_shell(legacy.scene);
    arrangement.set_listener({legacy.scene.listener_position, legacy.scene.listener_yaw_rad});
    const bool has_cabinet = !arrangement.sources().empty();
    if (has_cabinet)
    {
        arrangement.move_source(arrangement.sources().front().id,
                                {legacy.scene.source_position, legacy.scene.source_yaw_rad});
    }
    if (!legacy.ears && !state_.microphones.empty())
    {
        const SlotId microphone = state_.microphones.back().microphone;
        rig.set_parameter(microphone, "pattern",
                          static_cast<float>(static_cast<int>(legacy.scene.microphone_pattern)));
        SushiDSP::Room::MicrophonePlacement placement;
        placement.id = microphone;
        placement.pose = {legacy.scene.microphone_position, legacy.scene.microphone_yaw_rad};
        placement.pattern = legacy.scene.microphone_pattern;
        placement.proximity = legacy.scene.microphone_proximity;
        arrangement.update_microphone(placement);
        // A centred strip is 3 dB down a side; the old room's microphone was at full level on both.
        rig.set_parameter(state_.microphones.back().strip, "level", legacy.level_db + 3.0103f);
    }
    std::string reason;
    if (!rig.set_room_arrangement(*state_.room, arrangement, reason))
    {
        throw std::runtime_error(reason);
    }
    if (legacy.ears && state_.listener_strip.has_value())
    {
        rig.set_parameter(*state_.listener_strip, "level", legacy.level_db);
    }

    // The old chain ended in the room, so the room was all it sounded like.
    if (has_cabinet && state_.amp_bus.has_value())
    {
        for (std::size_t input = 0; input < state_.amp_bus_feeders.size(); ++input)
        {
            const SlotId feeder = state_.amp_bus_feeders[input];
            const bool heard = legacy.ears ? feeder == state_.listener_strip
                                           : feeder == state_.bus;
            rig.set_parameter(*state_.amp_bus, flag_name("mute", input).c_str(),
                              heard ? 0.0f : 1.0f);
        }
    }

    SD_LOG_INFO(K_HOST_GUI_LOG_CATEGORY,
                "migrated the preset's room into the cabinet section: {} as its receiver, level "
                "{} dB on that strip, Cabinet {}",
                legacy.ears ? "the listener" : "one microphone", legacy.level_db,
                has_cabinet ? "muted" : "left on, because the rig holds no cabinet for the room");
    if (legacy.mix < 1.0f)
    {
        SD_LOG_WARN(K_HOST_GUI_LOG_CATEGORY,
                    "the preset's room mix of {} was not carried over: the room has no dry path, "
                    "and the Cabinet input now holds the direct signal",
                    legacy.mix);
    }
}
