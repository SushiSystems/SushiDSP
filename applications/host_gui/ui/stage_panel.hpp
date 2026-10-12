/**************************************************************************/
/* stage_panel.hpp                                                        */
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
 * @file stage_panel.hpp
 * @brief Declares the drawing of the selected slot's front panel.
 * @author Mustafa Garip
 */

#pragma once

#include <optional>
#include <string>

#include "rig_controller.hpp"

namespace ui
{
    /// @brief Draws the selected slot's front panel into the current window and writes knob edits
    /// back through the controller. Draws a hint and nothing else when no slot is selected.
    /// @param rig The rig that owns the slot.
    /// @param selected The slot to draw.
    /// @param default_ir_directory Where the cabinet browser opens when the rig remembers no
    /// impulse-response folder. The caller derives it, so the executable's own location is read
    /// in one place.
    void DrawStagePanel(RigController& rig, std::optional<RigController::SlotId> selected,
                        const std::string& default_ir_directory);
} // namespace ui
