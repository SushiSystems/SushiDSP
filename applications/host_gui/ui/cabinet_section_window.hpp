/**************************************************************************/
/* cabinet_section_window.hpp                                             */
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
 * @file cabinet_section_window.hpp
 * @brief Declares the cabinet section window: the room and DI switches, the cabinet placements, the
 * plan view and the mixer.
 * @author Mustafa Garip
 */

#pragma once

#include <string>

#include "rig_controller.hpp"
#include "rig_topology.hpp"
#include "topology_applier.hpp"

namespace ui
{
    /// @brief Draws the cabinet section window. Structural edits go through the applier, which
    /// rewires and publishes the rig; parameter edits go through the mixer panel and are not published.
    /// @param rig The rig to edit.
    /// @param topology The chain's topology, changed by the room and DI switches.
    /// @param applier The applier that rewires the rig.
    /// @param error In/out: holds why the last structural edit was refused, or "".
    void DrawCabinetSectionWindow(RigController& rig, RigTopology& topology,
                                  TopologyApplier& applier, std::string& error);
} // namespace ui
