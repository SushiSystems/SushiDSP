/****************************************************************/
/* TransportBar.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
