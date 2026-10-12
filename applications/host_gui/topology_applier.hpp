/**************************************************************************/
/* topology_applier.hpp                                                   */
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

/** @file topology_applier.hpp
 *  @brief Declares the brick that compiles a RigTopology into a RigController's slots and edges.
 *  @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "rig_controller.hpp"
#include "rig_topology.hpp"

/// @brief Rewires a RigController to match a RigTopology and owns the hidden routing slots.
/// GUI thread only.
class TopologyApplier
{
    public:
        /// @brief Rewires the rig to match the topology and publishes once.
        /// @param topology The model to apply.
        /// @param rig The controller to rewire; a slot the topology does not hold is removed.
        /// @throws std::runtime_error when the rewired graph does not compile; the rig, the topology
        /// and this applier are rolled back first.
        void apply(RigTopology& topology, RigController& rig);

        /// @brief Switches the chain type and rewires the rig to match, in one step.
        /// @param topology The model to switch.
        /// @param rig The controller to rewire.
        /// @param type The chain type to switch to.
        /// @throws std::runtime_error when the rewired graph does not compile; everything is rolled
        /// back first.
        /// @see applications/host_gui/README.md
        void switch_chain_type(RigTopology& topology, RigController& rig, ChainType type);

        /// @brief Opens or closes the DI path and rewires the rig to match, in one step.
        /// @param topology The model to change.
        /// @param rig The controller to rewire.
        /// @param enabled True to open the DI path.
        /// @throws std::runtime_error when the rewired graph does not compile; everything is rolled
        /// back first.
        void set_di(RigTopology& topology, RigController& rig, bool enabled);

        /// @brief The hidden slots the mixer shows: the room's strips and bus, the DI strip and the master
        /// bus, and one microphone slot per layout microphone. Empty members mean the slot is not built.
        struct MixerSlots
        {
                std::optional<ui::SlotId> room_left;   ///< The listener's left-ear strip.
                std::optional<ui::SlotId> room_right;  ///< The listener's right-ear strip.
                std::optional<ui::SlotId> room_bus;    ///< The bus that joins the room's outputs.
                std::optional<ui::SlotId> di_strip;    ///< The DI strip, while DI is open.
                std::optional<ui::SlotId> master_bus;  ///< The master bus, while DI is open.
                std::vector<std::pair<std::uint32_t, ui::SlotId>>
                    microphones; ///< Microphone slots in layout order, with their layout ids.
        };

        /// @brief Reads the hidden slots the mixer shows.
        /// @return The slots as the last apply left them.
        MixerSlots mixer_slots() const;

        /// @brief Opens or closes the cabinet section and rewires the rig to match, in one step.
        /// @param topology The model to change.
        /// @param rig The controller to rewire; its room layout is read when the room is built.
        /// @param enabled True to open the room.
        /// @throws std::runtime_error when the room cannot be built, or the rewired graph does not
        /// compile; everything is rolled back first.
        void set_room(RigTopology& topology, RigController& rig, bool enabled);

    private:
        using SlotId = RigController::SlotId;

        /// @brief Records every slot edit one apply made, so a failed apply can undo them.
        struct Journal
        {
                std::vector<SlotGraph::EdgeRecord>
                    edges;                 ///< Holds the rig's edges, with their ports, before the apply.
                std::vector<SlotId> added; ///< Holds the slots the apply created.
                std::vector<std::pair<SlotId, RigController::SlotSnapshot>>
                    removed; ///< Holds the slots the apply removed.
        };

        /// @brief Holds this applier's own state, so a failed apply can put it back.
        struct State
        {
                std::optional<SlotId> splitter; ///< Holds the hidden splitter slot.
                std::optional<SlotId> mixer;    ///< Holds the hidden mixer slot.
                std::optional<SlotId> left_strip;  ///< Holds the Parallel left path's strip.
                std::optional<SlotId> right_strip; ///< Holds the Parallel right path's strip.
                std::optional<SlotId> di_strip;    ///< Holds the DI path's strip, while DI is open.
                std::optional<SlotId> master_bus;  ///< Holds the bus that sums the chain and DI.
                std::optional<SlotId> environment; ///< Holds the room environment, while the room is open.
                std::optional<SlotId> room_left;   ///< Holds the strip that pans the listener's left ear.
                std::optional<SlotId> room_right;  ///< Holds the strip that pans the listener's right ear.
                std::optional<SlotId> room_bus;    ///< Holds the bus that joins the room's outputs.
                std::map<std::uint32_t, SlotId> microphones; ///< Holds one microphone slot per layout id.
                std::vector<std::uint32_t> microphone_order; ///< Holds the layout ids in layout order.
        };

        /// @brief Creates the splitter and mixer while a split is open, or removes them for one path.
        void place_routing(const RigTopology& topology, RigController& rig, Journal& journal);

        /// @brief Creates the room environment, its strips, its bus and one microphone slot per layout
        /// microphone, and removes the chain's own joins.
        void place_room(const RigTopology& topology, RigController& rig, Journal& journal);

        /// @brief Removes the room's slots, when the room is closed or being rebuilt.
        void remove_room(RigController& rig, Journal& journal);

        /// @brief Wires the room: every path ends in the environment, its outputs reach the room bus.
        void connect_room(const RigTopology& topology, RigController& rig) const;

        /// @brief Creates the DI strip and the master bus while DI is open, or removes them.
        void place_di(const RigTopology& topology, RigController& rig, Journal& journal);

        /// @brief Removes every rig slot the topology does not route and that is not a routing slot.
        void remove_strays(const RigTopology& topology, RigController& rig, Journal& journal);

        /// @brief Replaces every rig edge with the topology's edges.
        void connect(const RigTopology& topology, RigController& rig) const;

        /// @brief Binds each root slot to the host input channel its path reads: the Parallel
        /// chain's two paths read channels 0 and 1, every other root reads channel 0.
        void bind_inputs(const RigTopology& topology, RigController& rig) const;

        /// @brief Returns a live routing slot of the given descriptor, creating one when needed.
        /// @return The slot's id.
        static SlotId ensure_slot(std::optional<SlotId> slot, const char* descriptor_id,
                                  RigController& rig, Journal& journal);

        /// @brief Snapshots a live slot into the journal and removes it without publishing.
        static void remove_journaled(SlotId slot, RigController& rig, Journal& journal);

        /// @brief Undoes a journal's edits and puts the topology and this applier back.
        void roll_back(const Journal& journal, RigTopology topology_before, State state_before,
                       RigTopology& topology, RigController& rig);

        State state_;
};
