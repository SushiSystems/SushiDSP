/**************************************************************************/
/* RigTopology.hpp                                                        */
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

/** @file RigTopology.hpp
 *  @brief Declares the host's chain-type model and the stage lists it routes.
 *  @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "ui/Geometry.hpp"

/// @brief The rig's chain shape: one linear run, or a split with 2 or 3 parallel branches.
enum class ChainType : std::uint8_t
{
    Series,
    TwoWay,
    ThreeWay,
    TwoWayAlt
};

/// @brief Which stage list a slot address names.
enum class StageKind : std::uint8_t
{
    Series,
    Pre,
    Branch,
    Post
};

/// @brief Names one stage: a fixed list, or a branch picked out by index.
struct StageAddress
{
    StageKind   kind = StageKind::Series;
    std::size_t branch = 0;   ///< Holds the branch index; read only when kind is Branch.
};

/// @brief Owns the chain type and the slot lists it routes: series, pre, branches, post.
class RigTopology
{
public:
    using SlotId = ui::SlotId;

    /// @brief Reports how many parallel branches a chain type opens.
    /// @return 0 for Series, 2 for TwoWay and TwoWayAlt, 3 for ThreeWay.
    static std::size_t branch_count(ChainType type) noexcept;

    /// @brief Reads the active chain type.
    /// @return The current ChainType.
    ChainType chain_type() const noexcept;

    /// @brief Switches the active chain type, discarding branches the new type does not keep.
    /// @param type The chain type to switch to.
    /// @return The slots the switch discards; series, pre and post lists survive every switch.
    std::vector<SlotId> set_chain_type(ChainType type);

    /// @brief Reads one stage's slot list.
    /// @param address The stage to read.
    /// @return The stage's slots, in stage order.
    const std::vector<SlotId>& stage(const StageAddress& address) const;

    /// @brief Reports whether a stage is reachable under the active chain type.
    /// @param address The stage to check.
    /// @return True for Series only while Series is active; true for Pre, Branch and Post only
    /// while a split type is active, and only for a branch index the type opens.
    bool is_visible(const StageAddress& address) const;

    /// @brief Inserts a slot into a stage at a clamped index.
    /// @param address The stage to insert into.
    /// @param index Where to insert; clamped to the stage's size.
    /// @param slot The slot to insert.
    /// @return False when the stage is not visible; true otherwise.
    bool insert(const StageAddress& address, std::size_t index, SlotId slot);

    /// @brief Moves a held slot to a new stage and index.
    /// @param slot The slot to move.
    /// @param address The destination stage.
    /// @param index Where to insert in the post-removal list; same-stage moves adjust the index.
    /// @return False when the slot is not held or the destination is not visible.
    bool move(SlotId slot, const StageAddress& address, std::size_t index);

    /// @brief Removes a slot from whichever visible stage holds it.
    /// @param slot The slot to remove.
    /// @return False when no visible stage holds the slot.
    bool remove(SlotId slot);

    /// @brief Renames a held slot in place, keeping its stage and position.
    /// @param from The slot's current id.
    /// @param to The slot's new id.
    /// @return False when no stage holds `from`.
    bool replace(SlotId from, SlotId to);

    /// @brief Finds which stage holds a slot.
    /// @param slot The slot to find.
    /// @return The slot's stage address, or nullopt when no stage holds it.
    std::optional<StageAddress> find(SlotId slot) const;

    /// @brief Lists every slot the active chain type routes, in signal order.
    /// @return Series' slots when Series is active; otherwise pre, then each branch, then post.
    std::vector<SlotId> active_slots() const;

    /// @brief Lists every slot held but not routed by the active chain type.
    /// @return The series list's slots while a split type is active; empty while Series is active.
    std::vector<SlotId> parked_slots() const;

    /// @brief Builds the edge list a split or series chain compiles to.
    /// @param splitter The splitter slot; required when a split type is active.
    /// @param mixer The mixer slot; required when a split type is active.
    /// @return The edges, first occurrence kept when a slot repeats an edge.
    std::vector<std::pair<SlotId, SlotId>> edges(std::optional<SlotId> splitter,
                                                  std::optional<SlotId> mixer) const;

    /// @brief Builds a Series topology from a linear slot order, for loading a preset.
    /// @param order The slots, in signal order.
    /// @return A RigTopology with `order` as its series list.
    static RigTopology from_series(std::vector<SlotId> order);

private:
    ChainType                          type_ = ChainType::Series;
    std::vector<SlotId>                series_;
    std::vector<SlotId>                pre_;
    std::vector<SlotId>                post_;
    std::vector<std::vector<SlotId>>   branches_;
};
