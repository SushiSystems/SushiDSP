/**************************************************************************/
/* cabinet_section_wiring.cpp                                             */
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

/**
 * @file cabinet_section_wiring.cpp
 * @brief Implements the cabinet section's wiring.
 * @author Mustafa Garip & Sushi Systems
 */

#include "cabinet_section_wiring.hpp"

#include <algorithm>

#include <SushiDSP/nodes/room/room_node.hpp>

namespace
{
    /// @brief Reports whether any microphone has a given room flag.
    bool any_with(const std::vector<MicrophoneWire>& microphones, bool room)
    {
        return std::any_of(microphones.begin(), microphones.end(),
                           [room](const MicrophoneWire& microphone)
                           {
                               return microphone.room == room;
                           });
    }
} // namespace

JoinContent join_content_of(std::size_t cabinet_paths, std::size_t paths)
{
    if (cabinet_paths == 0)
    {
        return JoinContent::Line;
    }
    return cabinet_paths >= paths ? JoinContent::Cabinets : JoinContent::Mixed;
}

CabinetSectionNeeds CabinetSectionNeeds::of(const std::vector<MicrophoneWire>& microphones,
                                            bool di, DiPosition di_position)
{
    CabinetSectionNeeds needs;
    needs.bus = !microphones.empty();
    needs.room_strip = any_with(microphones, true);
    needs.di_strip = di;
    needs.master_bus = di && di_position == DiPosition::AfterEffects;
    return needs;
}

CabinetSectionWiring wire_cabinet_section(const CabinetSectionRequest& request)
{
    using SushiDSP::room::RoomNode;

    CabinetSectionWiring wiring;
    const auto connect = [&wiring](ui::SlotId from, ui::SlotId to, std::uint32_t port = 0)
    {
        wiring.edges.push_back(CabinetSectionEdge{from, to, port});
    };

    for (const auto& [from, to] : request.chain_edges)
    {
        connect(from, to);
    }

    // The room's input slots are filled in connect order, so the sources go in as listed.
    for (const ui::SlotId source : request.sources)
    {
        connect(source, request.room);
    }
    connect(request.room, request.listener_strip, RoomNode::K_LISTENER_PORT);

    for (std::size_t index = 0; index < request.microphones.size(); ++index)
    {
        const MicrophoneWire& microphone = request.microphones[index];
        connect(request.room, microphone.microphone,
                RoomNode::microphone_port(static_cast<std::uint32_t>(index)));
        connect(microphone.microphone, microphone.strip);
    }
    for (const MicrophoneWire& microphone : request.microphones)
    {
        if (!microphone.room && request.bus.has_value())
        {
            connect(microphone.strip, *request.bus);
            wiring.bus_feeders.push_back(microphone.strip);
        }
        if (microphone.room && request.room_sum.has_value())
        {
            connect(microphone.strip, *request.room_sum);
            wiring.room_sum_feeders.push_back(microphone.strip);
        }
    }
    if (request.room_sum.has_value() && request.room_strip.has_value() && request.bus.has_value())
    {
        connect(*request.room_sum, *request.room_strip);
        connect(*request.room_strip, *request.bus);
        wiring.bus_feeders.push_back(*request.room_strip);
    }

    connect(request.join, request.amp_bus);
    wiring.amp_bus_feeders.push_back(request.join);
    if (request.bus.has_value())
    {
        connect(*request.bus, request.amp_bus);
        wiring.amp_bus_feeders.push_back(*request.bus);
    }
    connect(request.listener_strip, request.amp_bus);
    wiring.amp_bus_feeders.push_back(request.listener_strip);
    const bool di_before =
        request.di_strip.has_value() && request.di_position == DiPosition::BeforeEffects;
    if (di_before)
    {
        connect(*request.di_strip, request.amp_bus);
        wiring.amp_bus_feeders.push_back(*request.di_strip);
    }

    ui::SlotId last = request.amp_bus;
    for (const ui::SlotId slot : request.post)
    {
        connect(last, slot);
        last = slot;
    }

    if (request.di_strip.has_value() && !di_before && request.master_bus.has_value())
    {
        connect(last, *request.master_bus);
        connect(*request.di_strip, *request.master_bus);
        wiring.master_bus_feeders = {last, *request.di_strip};
        last = *request.master_bus;
    }
    connect(last, request.master_strip);
    return wiring;
}
