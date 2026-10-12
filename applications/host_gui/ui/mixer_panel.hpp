/**************************************************************************/
/* mixer_panel.hpp                                                        */
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
 * @file mixer_panel.hpp
 * @brief Declares the cabinet section's mixer: the room strips, the microphones, the room bus,
 * the DI strip and the master bus.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "rig_controller.hpp"
#include "topology_applier.hpp"

namespace ui
{
    /// @brief Draws the mixer's rows for every built slot and writes edits through the rig. The
    /// panel does not publish.
    /// @param id The ImGui id of the panel.
    /// @param rig The rig that owns the slots.
    /// @param slots The hidden slots to show; a missing slot is not drawn.
    /// @return True when any parameter changed.
    bool draw_mixer_panel(const char* id, RigController& rig,
                          const TopologyApplier::MixerSlots& slots);
} // namespace ui
