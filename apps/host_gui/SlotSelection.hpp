/**************************************************************************/
/* SlotSelection.hpp                                                      */
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


#pragma once

#include <cstddef>

/// @brief Pure index arithmetic for the rack list's "currently selected slot", kept out of
/// main.cpp's ImGui frame so it can be unit tested. A selection is an int because -1 means
/// "nothing selected"; every function returns the new selection and never touches the rig.
namespace slot_selection
{
    /// @brief Remaps a selection after a slot was inserted at `inserted_at`.
    /// @param selected The current selection, or -1 for none.
    /// @param inserted_at Chain position the new slot now occupies.
    /// @return The selection, shifted up by one if the insert pushed it along.
    inline int remap_after_insert(int selected, std::size_t inserted_at)
    {
        return selected >= static_cast<int>(inserted_at) ? selected + 1 : selected;
    }

    /// @brief Remaps a selection after the slot at `from` was moved to `to`.
    /// @param selected The current selection, or -1 for none.
    /// @param from The moved slot's old chain position.
    /// @param to The moved slot's new chain position.
    /// @return The selection, following the move the same way RigController::move() reorders
    /// the chain (erase at `from`, then insert at `to`).
    inline int remap_after_move(int selected, std::size_t from, std::size_t to)
    {
        const int from_index = static_cast<int>(from);
        const int to_index = static_cast<int>(to);
        if (selected == from_index)
        {
            return to_index;
        }
        if (from_index < selected && selected <= to_index)
        {
            return selected - 1;
        }
        if (to_index <= selected && selected < from_index)
        {
            return selected + 1;
        }
        return selected;
    }

    /// @brief Remaps a selection after the slot at `removed_at` was removed.
    /// @param selected The current selection, or -1 for none.
    /// @param removed_at The removed slot's chain position.
    /// @return -1 if the selected slot was the one removed, otherwise the shifted selection.
    inline int remap_after_remove(int selected, std::size_t removed_at)
    {
        const int removed_index = static_cast<int>(removed_at);
        if (selected == removed_index)
        {
            return -1;
        }
        return selected > removed_index ? selected - 1 : selected;
    }

    /// @brief Drops a selection that no longer addresses a live slot.
    /// @param selected The current selection, or -1 for none.
    /// @param chain_size The chain's current length.
    /// @return The selection, or -1 if it is out of range. An edit that silently fails (an
    /// unknown descriptor id never constructs a node) leaves the chain shorter than the
    /// remap functions assumed, so this is the authoritative last word each frame.
    inline int clamp(int selected, std::size_t chain_size)
    {
        return selected >= 0 && selected < static_cast<int>(chain_size) ? selected : -1;
    }
}
