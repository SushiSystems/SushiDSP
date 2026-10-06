/**************************************************************************/
/* RigTopology.hpp                                                        */
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

/// @brief The rig's chain shape: one path, or a split into two or three parallel paths.
///
/// The names follow AmpliTube 5's own, which is the behaviour this model reproduces; the
/// routing rules and what a path change does to the gear already placed are recorded in
/// docs/design/GUI_UX_REDESIGN.md.
enum class ChainType : std::uint8_t
{
    Single,
    TwoWay,
    ThreeWay
};

/// @brief Which stage list a slot address names.
enum class StageKind : std::uint8_t
{
    Branch,
    Post
};

/// @brief Names one stage: the master bus, or a path picked out by index.
struct StageAddress
{
    StageKind   kind = StageKind::Branch;
    std::size_t branch = 0;   ///< Holds the path index; read only when kind is Branch.
};

/// @brief Reports what a chain-type switch did, so the caller can act on the slots it names.
///
/// The model holds identities, not nodes, so it cannot copy a path's gear itself: it says which
/// paths were opened and leaves the duplication to whoever owns the nodes.
struct ChainTypeChange
{
    /// @brief The slots the paths that closed were holding; the caller destroys them.
    std::vector<ui::SlotId> discarded;

    /// @brief The indices of the paths that opened, each to be filled from the main path.
    std::vector<std::size_t> opened;
};

/// @brief Owns the chain type and the slot lists it routes: one path per branch, plus the
/// master bus every path merges into.
///
/// Branch 0 is the main path. It exists under every chain type and keeps its gear across every
/// switch; the paths beside it open as copies of it and are discarded when they close.
class RigTopology
{
public:
    using SlotId = ui::SlotId;

    /// @brief Reports how many parallel paths a chain type opens.
    /// @return 1 for Single, 2 for TwoWay, 3 for ThreeWay.
    static std::size_t branch_count(ChainType type) noexcept;

    /// @brief Reads the active chain type.
    /// @return The current ChainType.
    ChainType chain_type() const noexcept;

    /// @brief Switches the active chain type, keeping the main path and discarding the paths the
    /// new type closes.
    /// @param type The chain type to switch to.
    /// @return What closed and what opened; both lists are empty when the type does not change.
    ChainTypeChange set_chain_type(ChainType type);

    /// @brief Reads one stage's slot list.
    /// @param address The stage to read.
    /// @return The stage's slots, in stage order.
    const std::vector<SlotId>& stage(const StageAddress& address) const;

    /// @brief Reports whether a stage is reachable under the active chain type.
    /// @param address The stage to check.
    /// @return True for Post always; true for a Branch only at an index the type opens.
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
    /// @return Each path's slots in path order, then the master bus's.
    std::vector<SlotId> active_slots() const;

    /// @brief Builds the edge list the active chain type compiles to.
    /// @param splitter The splitter slot; required while more than one path is open.
    /// @param mixer The mixer slot; required while more than one path is open.
    /// @return The edges, first occurrence kept when a slot repeats an edge.
    std::vector<std::pair<SlotId, SlotId>> edges(std::optional<SlotId> splitter,
                                                  std::optional<SlotId> mixer) const;

    /// @brief Builds a single-path topology from a linear slot order, for loading a preset.
    /// @param order The slots, in signal order.
    /// @return A RigTopology whose main path is `order`.
    static RigTopology from_linear_order(std::vector<SlotId> order);

private:
    ChainType                          type_ = ChainType::Single;
    std::vector<SlotId>                post_;
    std::vector<std::vector<SlotId>>   branches_{ 1 };
};
