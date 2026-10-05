/****************************************************************/
/* SlotSelection.hpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include "RigController.hpp"

/// @brief Whether the rack list's "currently selected slot" still names a live slot, kept out of
/// main.cpp's ImGui frame so it can be unit tested. A SlotId (unlike the old chain-index scheme)
/// stays valid across insert/move/remove, so the only remaining job after an edit is dropping a
/// selection that no longer exists — no index-remapping arithmetic is needed anymore.
namespace slot_selection
{
    /// @brief Whether `selected` still names a slot present in `order`.
    /// @param selected The current selection, or std::nullopt for none.
    /// @param order The rig's current slot_order().
    inline bool still_valid(std::optional<RigController::SlotId> selected, const std::vector<RigController::SlotId>& order)
    {
        if (!selected.has_value())
        {
            return false;
        }
        return std::find(order.begin(), order.end(), *selected) != order.end();
    }
}
