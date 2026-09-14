/**************************************************************************/
/* MeterBar.hpp                                                           */
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
