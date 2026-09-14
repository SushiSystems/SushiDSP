/**************************************************************************/
/* TopologyApplier.hpp                                                    */
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

/** @file TopologyApplier.hpp
 *  @brief Declares the brick that compiles a RigTopology into a RigController's slots and edges.
 *  @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "RigController.hpp"
#include "RigTopology.hpp"

/// @brief Rewires a RigController to match a RigTopology, owning the hidden routing slots and
/// the snapshots of parked series slots. GUI thread only.
class TopologyApplier
{
public:
    /// @brief Rewires the rig to match the topology and publishes once.
    /// @param topology The model to apply; parked slots that come back are renamed in it.
    /// @param rig The controller to rewire; a slot the topology does not hold is removed.
    /// @throws std::runtime_error when the rewired graph does not compile; the rig, the topology
    /// and this applier are rolled back first.
    void apply(RigTopology& topology, RigController& rig);

    /// @brief Counts the parked series slots held as snapshots.
    /// @return The number of snapshots waiting for Series to come back.
    std::size_t parked_count() const noexcept;

private:
    using SlotId = RigController::SlotId;

    /// @brief Records every slot edit one apply made, so a failed apply can undo them.
    struct Journal
    {
        std::vector<std::pair<SlotId, SlotId>>                     edges;    ///< Holds the rig's edges before the apply.
        std::vector<SlotId>                                        added;    ///< Holds the slots the apply created.
        std::vector<std::pair<SlotId, RigController::SlotSnapshot>> removed;  ///< Holds the slots the apply removed.
    };

    /// @brief Holds this applier's own state, so a failed apply can put it back.
    struct State
    {
        std::optional<SlotId>                                       splitter; ///< Holds the hidden splitter slot.
        std::optional<SlotId>                                       mixer;    ///< Holds the hidden mixer slot.
        std::vector<std::pair<SlotId, RigController::SlotSnapshot>> parked;   ///< Holds parked snapshots by pre-park id.
    };

    /// @brief Snapshots and removes every parked slot still live in the rig.
    void park(const RigTopology& topology, RigController& rig, Journal& journal);

    /// @brief Restores every active slot held as a snapshot and renames it in the topology.
    void unpark(RigTopology& topology, RigController& rig, Journal& journal);

    /// @brief Creates the splitter and mixer for a split type, or removes them for Series.
    void place_routing(const RigTopology& topology, RigController& rig, Journal& journal);

    /// @brief Removes every rig slot the topology does not route and that is not a routing slot.
    void remove_strays(const RigTopology& topology, RigController& rig, Journal& journal);

    /// @brief Drops parked snapshots the topology no longer holds.
    void prune_parked(const RigTopology& topology);

    /// @brief Replaces every rig edge with the topology's edges.
    void connect(const RigTopology& topology, RigController& rig) const;

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
