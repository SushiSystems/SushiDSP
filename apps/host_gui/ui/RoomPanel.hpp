/****************************************************************/
/* RoomPanel.hpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file RoomPanel.hpp
 * @brief The room slot's front panel: a top-down plan, the surfaces, the microphone and the mix.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <string>

#include "RigController.hpp"

namespace ui
{
    /// @brief Draws a room slot's shell, finishes, furnishings and mix controls.
    /// @param rig The rig that owns the slot.
    /// @param slot The room slot to draw.
    /// @param room_error In/out: the last refused scene's reason, shown until an edit lands.
    /// @see apps/host_gui/ui/README.md
    void DrawRoomPanel(RigController& rig, RigController::SlotId slot, std::string& room_error);
}
