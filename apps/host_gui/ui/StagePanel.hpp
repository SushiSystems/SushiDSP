/****************************************************************/
/* StagePanel.hpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file StagePanel.hpp
 * @brief Declares the drawing of the selected slot's front panel.
 * @author Mustafa Garip
 */

#pragma once

#include <optional>
#include <string>

#include "RigController.hpp"

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
}
