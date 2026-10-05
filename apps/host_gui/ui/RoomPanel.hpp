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
