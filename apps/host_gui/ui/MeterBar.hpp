/****************************************************************/
/* MeterBar.hpp                                                 */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include "ui/LevelProbe.hpp"

namespace ui
{
    /// @brief Draws the input and output level bars inline, right-aligned in the current window,
    /// reading and resetting both probes. Owns the ballistics: the displayed level jumps to a
    /// higher peak at once and falls by 15 percent per frame toward a lower one.
    /// @param input The probe fed from the device's input block.
    /// @param output The probe fed from the rig's output block.
    void DrawInlineMeters(LevelProbe& input, LevelProbe& output);
}
