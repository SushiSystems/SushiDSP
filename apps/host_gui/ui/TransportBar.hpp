/**************************************************************************/
/* TransportBar.hpp                                                       */
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
 * @file TransportBar.hpp
 * @brief Declares the bottom transport bar.
 * @author Mustafa Garip
 */

#pragma once

#include <string>

#include "RigController.hpp"
#include "ui/LevelProbe.hpp"

namespace ui
{
    /// @brief Draws the bottom transport bar, its second row and the level meters.
    /// @param rig The rig whose transport, clock, metronome, recording bus and input it drives.
    /// @param input The probe fed from the device's input block.
    /// @param output The probe fed from the rig's output block.
    /// @param mini_daw_active In/out: whether the workspace shows the mini-DAW.
    /// @param transport_error In/out: the last refusal, shown until an action succeeds.
    /// @see apps/host_gui/ui/README.md
    void DrawTransportBar(RigController& rig, LevelProbe& input, LevelProbe& output,
                          bool& mini_daw_active, std::string& transport_error);
}
