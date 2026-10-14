/**************************************************************************/
/* rig_shape.hpp                                                          */
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
 * @file rig_shape.hpp
 * @brief The shape of a rig beyond its slots: the stages, the DI choice and the hidden slots, as plain data.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <optional>
#include <vector>

#include <SushiDSP/nodes/room/room_scene.hpp>

#include "rig_topology.hpp"
#include "ui/geometry.hpp"

/// @brief Names one microphone's two slots.
struct MicrophoneSlots
{
        ui::SlotId microphone = 0; ///< Holds the microphone rig element.
        ui::SlotId strip = 0;      ///< Holds the microphone's channel strip.
};

/// @brief Names every slot a rig holds that no stage of the chain shows: the chain's own joins and
/// the cabinet section. An empty member means the slot is not built. Pure data.
struct HiddenSlots
{
        std::optional<ui::SlotId> splitter;    ///< The slot that fans a split chain out.
        std::optional<ui::SlotId> mixer;       ///< The chain's join: a mixer, or the Parallel bus.
        std::optional<ui::SlotId> left_strip;  ///< The Parallel left path's strip.
        std::optional<ui::SlotId> right_strip; ///< The Parallel right path's strip.
        std::optional<ui::SlotId> di_strip;    ///< The DI strip, while the DI path is on.
        std::optional<ui::SlotId> master_bus;  ///< The bus that joins the DI path.
        std::optional<ui::SlotId> room;        ///< The room.
        std::optional<ui::SlotId> listener_strip; ///< The listener's stereo strip.
        std::optional<ui::SlotId> bus;            ///< The bus the microphone strips join.
        std::optional<ui::SlotId> room_sum;       ///< The bus that sums the room microphones.
        std::optional<ui::SlotId> room_strip;     ///< The Room strip.
        std::optional<ui::SlotId> amp_bus;        ///< The bus of Cabinet, Bus and Listener.
        std::optional<ui::SlotId> master_strip;   ///< The strip the rig leaves through.
        std::optional<ui::SlotId> join;           ///< The chain's join as last wired, the Cabinet input.
        std::vector<MicrophoneSlots> microphones; ///< The microphones in the room's port order.
        std::vector<ui::SlotId> bus_feeders;        ///< What feeds each input of the bus.
        std::vector<ui::SlotId> room_sum_feeders;   ///< What feeds each input of the Room sum.
        std::vector<ui::SlotId> amp_bus_feeders;    ///< What feeds each input of the amp bus.
        std::vector<ui::SlotId> master_bus_feeders; ///< What feeds each input of the master bus.

        /// @brief Calls a function on every slot id this record names, so a caller can rename
        /// them all in one pass.
        /// @param rename Takes each id by reference.
        template <typename Function> void for_each_id(Function&& rename)
        {
            for (std::optional<ui::SlotId>* slot :
                 {&splitter, &mixer, &left_strip, &right_strip, &di_strip, &master_bus, &room,
                  &listener_strip, &bus, &room_sum, &room_strip, &amp_bus, &master_strip, &join})
            {
                if (slot->has_value())
                {
                    rename(**slot);
                }
            }
            for (MicrophoneSlots& slots : microphones)
            {
                rename(slots.microphone);
                rename(slots.strip);
            }
            for (std::vector<ui::SlotId>* feeders :
                 {&bus_feeders, &room_sum_feeders, &amp_bus_feeders, &master_bus_feeders})
            {
                for (ui::SlotId& feeder : *feeders)
                {
                    rename(feeder);
                }
            }
        }
};

/// @brief The whole shape of a rig beyond its slots: which stage holds which slot, the DI choice
/// and the hidden slots. What a preset carries so a loaded rig is the rig that was saved.
struct RigShape
{
        ChainType chain_type = ChainType::Single; ///< Holds the chain type.

        /// @brief Holds each path's slots, in path order; one list per path the type opens.
        std::vector<std::vector<ui::SlotId>> branches;

        std::vector<ui::SlotId> post; ///< Holds the post stage's slots.
        bool di = false;              ///< True while the DI path is on.
        DiPosition di_position = DiPosition::AfterEffects; ///< Holds where the DI path joins.
        HiddenSlots hidden;           ///< Holds the hidden slots.

        /// @brief Calls a function on every slot id the shape names.
        /// @param rename Takes each id by reference.
        template <typename Function> void for_each_id(Function&& rename)
        {
            for (std::vector<ui::SlotId>& branch : branches)
            {
                for (ui::SlotId& slot : branch)
                {
                    rename(slot);
                }
            }
            for (ui::SlotId& slot : post)
            {
                rename(slot);
            }
            hidden.for_each_id(rename);
        }
};

/// @brief What a preset written before the cabinet section says about its room: one cabinet heard
/// by one receiver, with a level of its own. A host turns it into an arrangement and strip
/// settings.
struct LegacyRoom
{
        SushiDSP::room::RoomScene scene; ///< Holds the room, its cabinet, listener and microphone.
        bool ears = true;                ///< True when the room was heard by the listener.
        float level_db = 0.0f;           ///< Holds the room's own output level.
        float mix = 1.0f;                ///< Holds the room's dry-wet mix, which nothing carries on.
};
