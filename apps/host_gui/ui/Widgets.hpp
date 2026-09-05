/**************************************************************************/
/* Widgets.hpp                                                            */
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

#include <imgui.h>

#include "SushiDSP/core/NodeDescriptor.hpp"

namespace ui
{
    /// @brief Draws a turnable knob for one parameter and applies pointer edits to *value. A
    /// vertical drag turns it, Shift makes the drag four times finer, and a double click restores
    /// the parameter's default.
    /// @param id The ImGui id, unique within the window.
    /// @param param The parameter, whose range and kind set the taper and the tooltip's unit.
    /// @param value In/out: the current value.
    /// @param diameter The knob's diameter in pixels.
    /// @return True on the frames where *value changed.
    bool Knob(const char* id, const SushiDSP::ParameterDescriptor& param, float* value, float diameter);

    /// @brief Draws a two-state rocker switch.
    /// @param id The ImGui id, unique within the window.
    /// @param on In/out: the current state.
    /// @param width The switch's width in pixels.
    /// @param height The switch's height in pixels.
    /// @return True on the frames where *on changed.
    bool ToggleSwitch(const char* id, bool* on, float width, float height);

    /// @brief Draws one parameter's current value as text the pointer cannot change.
    /// @param id The ImGui id, unique within the window.
    /// @param param The parameter, whose label and unit fill the tooltip.
    /// @param value The value to show.
    /// @param width The box's width in pixels.
    /// @param height The box's height in pixels.
    void ValueReadout(const char* id, const SushiDSP::ParameterDescriptor& param, float value,
                      float width, float height);
}
