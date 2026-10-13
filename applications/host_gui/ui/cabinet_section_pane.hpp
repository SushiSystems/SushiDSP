/**************************************************************************/
/* cabinet_section_pane.hpp                                               */
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
 * @file cabinet_section_pane.hpp
 * @brief Declares the cabinet section's pane: the room scene, the inspector and the mixer.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <string>

#include "rig_controller.hpp"
#include "rig_topology.hpp"
#include "topology_applier.hpp"
#include "ui/room_scene_view.hpp"

namespace ui
{
    /// @brief Holds what the cabinet section keeps between frames: the scene's camera and
    /// selection, and the last refused edit's reasons.
    struct CabinetSectionState
    {
            RoomSceneState scene;   ///< The room scene's camera, selection and drag.
            std::string error;      ///< Why the last edit of the section was refused, or "".
            std::string room_error; ///< Why the last edit of the room's shell was refused, or "".
    };

    /// @brief Draws the cabinet section over the region it is given: the room scene with its
    /// cabinets, microphones and listener, an inspector for the selected one and for the room's
    /// shell, the DI path's switches, and the mixer. Every edit is applied through the rig or the
    /// applier on the frame it is made.
    /// @param rig The rig to draw and edit.
    /// @param topology The model the rig follows.
    /// @param applier The owner of the section's hidden slots.
    /// @param state In/out: the camera, the selection and the refusal reasons.
    void DrawCabinetSectionPane(RigController& rig, RigTopology& topology,
                                TopologyApplier& applier, CabinetSectionState& state);
} // namespace ui
