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
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <SushiDSP/nodes/room/room_arrangement.hpp>

#include "cabinet_section_wiring.hpp"
#include "rig_controller.hpp"
#include "rig_shape.hpp"
#include "rig_topology.hpp"

/// @brief Rewires a RigController to match a RigTopology and owns the hidden routing slots: the
/// chain's own joins, and the cabinet section with its room, microphones, strips and buses.
///
/// The cabinet section exists while the topology routes at least one slot. Its room holds one
/// source per path that carries a cabinet, keyed by the cabinet's slot, and one microphone per
/// microphone slot this applier owns. GUI thread only.
class TopologyApplier
{
    public:
        using SlotId = RigController::SlotId;

        /// @brief Names one microphone's two slots.
        using MicrophoneSlots = ::MicrophoneSlots;

        /// @brief The hidden slots the mixer and the scene show.
        using MixerSlots = HiddenSlots;

        /// @brief Rewires the rig to match the topology and publishes once.
        /// @param topology The model to apply.
        /// @param rig The controller to rewire; a slot the topology does not hold is removed.
        /// @throws std::runtime_error when the rewired graph does not compile or the room refuses
        /// its arrangement; the rig, the topology and this applier are rolled back first.
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

        /// @brief Moves the DI path before or after the post effects and rewires, in one step.
        /// @param topology The model to change.
        /// @param rig The controller to rewire.
        /// @param position Where the DI path joins.
        /// @throws std::runtime_error when the rewired graph does not compile; everything is rolled
        /// back first.
        void set_di_position(RigTopology& topology, RigController& rig, DiPosition position);

        /// @brief Adds a microphone to the room, in front of the first cabinet, and rewires.
        /// @param topology The model the rig follows.
        /// @param rig The controller to rewire.
        /// @return The new microphone's slot.
        /// @throws std::runtime_error when the room is full, when the rig holds no gear, or when
        /// the rewired graph does not compile; everything is rolled back first.
        SlotId add_microphone(RigTopology& topology, RigController& rig);

        /// @brief Removes a microphone and its strip from the room and rewires.
        /// @param topology The model the rig follows.
        /// @param rig The controller to rewire.
        /// @param microphone The microphone's slot.
        /// @throws std::runtime_error when the slot is not one of the room's microphones or the
        /// rewired graph does not compile; everything is rolled back first.
        void remove_microphone(RigTopology& topology, RigController& rig, SlotId microphone);

        /// @brief Copies every microphone's pattern and proximity switch into the room's
        /// arrangement, without rewiring. Called after one of those parameters changes.
        /// @param rig The controller that holds the room.
        /// @param error_out Receives the reason when the room refuses the arrangement.
        /// @return False when the room refused; true otherwise, or when no room is built.
        bool refresh_room(RigController& rig, std::string& error_out) const;

        /// @brief Says what the chain's join carries: the cabinets' signal, a line signal when no
        /// path holds a cabinet, or both already summed.
        /// @param topology The model the rig follows.
        /// @param rig The controller that holds the paths.
        /// @return The join's content.
        JoinContent join_content(const RigTopology& topology, const RigController& rig) const;

        /// @brief Reads the hidden slots the mixer and the scene show.
        /// @return The slots as the last apply left them.
        MixerSlots mixer_slots() const;

        /// @brief Takes over hidden slots a loaded rig already holds, so the next apply keeps them
        /// and their settings. A slot the rig does not hold under the expected descriptor is
        /// dropped from the list.
        /// @param slots The slots to adopt, as a preset named them.
        /// @param rig The controller that holds them.
        void adopt(const HiddenSlots& slots, const RigController& rig);

        /// @brief Turns the room of a preset written before the cabinet section into the section's
        /// own terms: the room's shell and poses, one receiver, its level on that receiver's
        /// strip, and Cabinet muted. Logs what it carried over and what it could not.
        /// @param topology The model the rig follows.
        /// @param rig The controller to change.
        /// @param legacy The old room.
        /// @throws std::runtime_error when the room refuses the arrangement or the rewired graph
        /// does not compile; everything is rolled back first.
        void migrate_legacy_room(RigTopology& topology, RigController& rig,
                                 const LegacyRoom& legacy);

    private:
        /// @brief Records every edit one apply made, so a failed apply can undo them.
        struct Journal
        {
                std::vector<SlotGraph::EdgeRecord>
                    edges;                 ///< Holds the rig's edges, with their ports, before the apply.
                std::vector<SlotId> added; ///< Holds the slots the apply created.
                std::vector<std::pair<SlotId, RigController::SlotSnapshot>>
                    removed; ///< Holds the slots the apply removed.

                /// @brief Holds each bus flag the apply wrote, with the value it replaced.
                std::vector<std::tuple<SlotId, std::string, float>> flags;

                /// @brief Holds the room's slot and arrangement before the apply, when it had one.
                std::optional<std::pair<SlotId, SushiDSP::Room::Arrangement>> arrangement;
        };

        /// @brief Names an edit of the room's microphones one apply carries out.
        struct MicrophoneEdit
        {
                bool add = false;             ///< True to add one microphone.
                std::optional<SlotId> remove; ///< Holds the microphone to remove.
        };

        /// @brief Holds this applier's own state, so a failed apply can put it back.
        using State = HiddenSlots;

        /// @brief Runs one apply with a microphone edit folded in.
        void apply_with(RigTopology& topology, RigController& rig, const MicrophoneEdit& edit);

        /// @brief Creates the splitter and mixer while a split is open, or removes them for one path.
        void place_routing(const RigTopology& topology, RigController& rig, Journal& journal);

        /// @brief Creates the cabinet section's slots while the topology routes a slot, carrying out
        /// the microphone edit, or removes every one of them.
        void place_section(const RigTopology& topology, RigController& rig, Journal& journal,
                           const MicrophoneEdit& edit);

        /// @brief Creates the DI strip and, where the DI joins a bus of its own, that bus; or
        /// removes them.
        void place_di(const RigTopology& topology, RigController& rig, Journal& journal);

        /// @brief Brings the room's arrangement in line with the topology: one source per path
        /// that carries a cabinet, one microphone per microphone slot, each new one at a default
        /// pose.
        /// @throws std::runtime_error when the room refuses the arrangement.
        void reconcile_room(const RigTopology& topology, RigController& rig, Journal& journal) const;

        /// @brief Removes every rig slot the topology does not route and that is not a routing slot.
        void remove_strays(const RigTopology& topology, RigController& rig, Journal& journal);

        /// @brief Replaces every rig edge with the topology's edges and the section's, and moves
        /// each bus's mute and solo flags to the input its feeder now holds.
        void connect(const RigTopology& topology, RigController& rig, Journal& journal);

        /// @brief Binds each root slot to the host input channel its path reads: the Parallel
        /// chain's two paths read channels 0 and 1, every other root reads channel 0.
        void bind_inputs(const RigTopology& topology, RigController& rig) const;

        /// @brief Returns a live routing slot of the given descriptor, creating one when needed.
        /// @return The slot's id.
        static SlotId ensure_slot(std::optional<SlotId> slot, const char* descriptor_id,
                                  RigController& rig, Journal& journal);

        /// @brief Removes a routing slot when it is live, and forgets it.
        static void release_slot(std::optional<SlotId>& slot, RigController& rig, Journal& journal);

        /// @brief Snapshots a live slot into the journal and removes it without publishing.
        static void remove_journaled(SlotId slot, RigController& rig, Journal& journal);

        /// @brief Moves one bus's mute and solo flags from the inputs its feeders held to the
        /// inputs they hold now. A feeder the bus did not have starts unmuted, or muted when it is
        /// the one named.
        static void remap_flags(RigController& rig, SlotId bus, const std::vector<SlotId>& before,
                                const std::vector<SlotId>& after,
                                std::optional<SlotId> muted_at_first, Journal& journal);

        /// @brief Undoes a journal's edits and puts the topology and this applier back.
        void roll_back(const Journal& journal, RigTopology topology_before, State state_before,
                       RigTopology& topology, RigController& rig);

        State state_;
};
