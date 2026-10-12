/**************************************************************************/
/* slot_selection.hpp                                                     */
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
 * @file slot_selection.hpp
 * @brief Defines the check that the rack's selected slot still names a live slot.
 * @author Mustafa Garip
 */

#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include "rig_controller.hpp"

/// @brief Whether the rack list's "currently selected slot" still names a live slot, kept out of
/// main.cpp's ImGui frame so it can be unit tested. A SlotId (unlike the old chain-index scheme)
/// stays valid across insert/move/remove, so the only remaining job after an edit is dropping a
/// selection that no longer exists — no index-remapping arithmetic is needed anymore.
namespace slot_selection
{
    /// @brief Whether `selected` still names a slot present in `order`.
    /// @param selected The current selection, or std::nullopt for none.
    /// @param order The rig's current slot_order().
    inline bool still_valid(std::optional<RigController::SlotId> selected,
                            const std::vector<RigController::SlotId>& order)
    {
        if (!selected.has_value())
        {
            return false;
        }
        return std::find(order.begin(), order.end(), *selected) != order.end();
    }
} // namespace slot_selection
