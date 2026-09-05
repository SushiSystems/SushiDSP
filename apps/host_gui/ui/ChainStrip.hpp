/**************************************************************************/
/* ChainStrip.hpp                                                         */
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

#include "RigController.hpp"

namespace ui
{
    /// @brief Draws the chain strip into the current window and handles selection, drag-reorder,
    /// gear drops, bypass toggles and removal.
    /// @param rig The rig to draw and edit.
    /// @param selected In/out: the selected slot, cleared when that slot stops existing.
    /// @param created_slot Out: the slot a gear drop just created, or std::nullopt when this frame
    /// created none. The caller uses it to run its own post-insert step.
    void DrawChainStrip(RigController& rig, std::optional<RigController::SlotId>& selected,
                        std::optional<RigController::SlotId>& created_slot);
}
