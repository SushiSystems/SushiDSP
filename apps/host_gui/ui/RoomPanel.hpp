/**************************************************************************/
/* RoomPanel.hpp                                                          */
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
    /// @brief Draws a room slot's controls into the current window in three layers and a mix
    /// group. Shell holds the shoebox's dimensions and a top-down plan where the cabinet, the
    /// head and the microphone drag, with each pose's height and facing and the microphone's
    /// pattern. Finishes holds a material and scattering word per surface. Furnishings holds a
    /// list of furniture rows with a quantity each. The mix group holds the node's mix, level,
    /// listener and direct-delay parameters.
    /// Beside them it reads out the Schroeder frequency the node split its last set at, Weyl's
    /// estimate of the modes under it, and where the HRTF came from. Every edit goes through
    /// RigController::set_room_scene, which paces a drag. Spec §9.2 and §14, DE6.
    /// @param rig The rig that owns the slot.
    /// @param slot The room slot to draw.
    /// @param room_error In/out: the last refused scene's reason, shown until an edit lands.
    void DrawRoomPanel(RigController& rig, RigController::SlotId slot, std::string& room_error);
}
