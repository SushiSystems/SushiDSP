/**************************************************************************/
/* room_panel.hpp                                                         */
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
 * @file room_panel.hpp
 * @brief The room slot's front panel: a top-down plan, the surfaces, the microphone and the mix.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <string>

#include "rig_controller.hpp"

namespace ui
{
    /// @brief Draws a room's shell: its dimensions, openings, finishes and furnishings, and the
    /// direct-delay choice. Where the cabinets, the listener and the microphones stand is the
    /// scene view's to edit; this panel keeps them inside the shell when it shrinks.
    /// @param rig The rig that owns the slot.
    /// @param slot The room slot to draw.
    /// @param room_error In/out: the last refused room's reason, shown until an edit lands.
    /// @see applications/host_gui/ui/README.md
    void DrawRoomPanel(RigController& rig, RigController::SlotId slot, std::string& room_error);
} // namespace ui
