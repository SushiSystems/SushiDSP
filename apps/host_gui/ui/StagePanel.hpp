/**************************************************************************/
/* StagePanel.hpp                                                         */
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
