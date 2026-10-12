/**************************************************************************/
/* meter_bar.hpp                                                          */
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
 * @file meter_bar.hpp
 * @brief Declares the input and output level meters.
 * @author Mustafa Garip
 */

#pragma once

#include "ui/level_probe.hpp"

namespace ui
{
    /// @brief Draws the input and output level bars inline, right-aligned in the current window,
    /// reading and resetting both probes. Owns the ballistics: the displayed level jumps to a
    /// higher peak at once and falls by 15 percent per frame toward a lower one.
    /// @param input The probe fed from the device's input block.
    /// @param output The probe fed from the rig's output block.
    void DrawInlineMeters(LevelProbe& input, LevelProbe& output);
} // namespace ui
