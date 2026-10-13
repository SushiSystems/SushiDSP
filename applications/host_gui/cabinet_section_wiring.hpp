/**************************************************************************/
/* cabinet_section_wiring.hpp                                             */
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
 * @file cabinet_section_wiring.hpp
 * @brief Declares the cabinet section's wiring as data: the hidden slots it needs and the edges between them.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "rig_topology.hpp"
#include "ui/geometry.hpp"

/// @brief One microphone as the cabinet section wires it.
struct MicrophoneWire
{
        ui::SlotId microphone = 0; ///< Holds the microphone rig element's slot.
        ui::SlotId strip = 0;      ///< Holds the microphone's channel strip.
        bool room = false;         ///< True when the microphone is summed on the Room strip.
};

/// @brief Says which hidden slots a cabinet section needs for what it holds.
struct CabinetSectionNeeds
{
        bool bus = false;        ///< The bus the microphone strips and the Room strip join.
        bool room_strip = false; ///< The Room sum and its strip, for the microphones marked room.
        bool di_strip = false;   ///< The DI strip, while the DI path is on.
        bool master_bus = false; ///< The bus that joins the DI path after the post effects.

        /// @brief Works out the needs from the microphones and the DI choice.
        /// @param microphones The section's microphones.
        /// @param di True while the DI path is on.
        /// @param di_position Where the DI path joins.
        /// @return The needs.
        static CabinetSectionNeeds of(const std::vector<MicrophoneWire>& microphones, bool di,
                                      DiPosition di_position);
};

/// @brief What the cabinet section's wiring is built from. Pure data; no rig is touched.
struct CabinetSectionRequest
{
        /// @brief Holds the chain's own edges up to and including its join, with the post stage's
        /// edges left out.
        std::vector<std::pair<ui::SlotId, ui::SlotId>> chain_edges;

        ui::SlotId join = 0; ///< Holds the chain's join: the single path's end, the mixer or the bus.

        /// @brief Holds the path ends that feed the room, in input-slot order.
        std::vector<ui::SlotId> sources;

        /// @brief Holds the microphones in the room's port order.
        std::vector<MicrophoneWire> microphones;

        std::vector<ui::SlotId> post; ///< Holds the post stage's slots, in stage order.

        DiPosition di_position = DiPosition::AfterEffects; ///< Holds where the DI path joins.

        ui::SlotId room = 0;           ///< Holds the room's slot.
        ui::SlotId listener_strip = 0; ///< Holds the stereo strip of the listener's port.
        ui::SlotId amp_bus = 0;        ///< Holds the bus that joins Cabinet, Bus and Listener.
        ui::SlotId master_strip = 0;   ///< Holds the stereo strip the rig leaves through.

        std::optional<ui::SlotId> bus;        ///< Holds the microphones' bus; see CabinetSectionNeeds.
        std::optional<ui::SlotId> room_sum;   ///< Holds the bus that sums the room microphones.
        std::optional<ui::SlotId> room_strip; ///< Holds the stereo strip of the Room sum.
        std::optional<ui::SlotId> di_strip;   ///< Holds the DI strip.
        std::optional<ui::SlotId> master_bus; ///< Holds the bus that joins the DI after the effects.
};

/// @brief One edge of the section, with the upstream port it reads.
struct CabinetSectionEdge
{
        ui::SlotId from = 0;         ///< Holds the upstream slot.
        ui::SlotId to = 0;           ///< Holds the downstream slot.
        std::uint32_t from_port = 0; ///< Holds the upstream output port.
};

/// @brief The section's wiring as data: every edge, in the order a rig must connect them, and
/// what feeds each bus input, in input order.
struct CabinetSectionWiring
{
        std::vector<CabinetSectionEdge> edges; ///< Holds every edge of the rig, chain included.

        std::vector<ui::SlotId> bus_feeders;        ///< Holds what feeds each input of the bus.
        std::vector<ui::SlotId> room_sum_feeders;   ///< Holds what feeds each input of the Room sum.
        std::vector<ui::SlotId> amp_bus_feeders;    ///< Holds what feeds each input of the amp bus.
        std::vector<ui::SlotId> master_bus_feeders; ///< Holds what feeds each input of the master bus.
};

/// @brief Builds the cabinet section's wiring: each source into the room, port 0 into the listener
/// strip, port 1 + m through microphone m into the bus or the Room sum, then the amp bus, the
/// post stage, the master bus and the master strip.
/// @param request The slots and choices to wire.
/// @return The wiring; applications/host_gui/README.md draws it.
CabinetSectionWiring wire_cabinet_section(const CabinetSectionRequest& request);
