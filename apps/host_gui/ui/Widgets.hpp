/****************************************************************/
/* Widgets.hpp                                                  */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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

    /// @brief The fader's width as a fraction of its travel.
    inline constexpr float kSliderWidthRatio = 0.34f;

    /// @brief Draws a vertical fader for one parameter and applies pointer edits to *value. A
    /// vertical drag moves the cap, Shift makes the drag four times finer, and a double click
    /// restores the parameter's default.
    /// @param id The ImGui id, unique within the window.
    /// @param param The parameter, whose range and kind set the taper and the tooltip's unit.
    /// @param value In/out: the current value.
    /// @param travel The cap's travel in pixels, from the slot's bottom to its top.
    /// @return True on the frames where *value changed.
    bool Slider(const char* id, const SushiDSP::ParameterDescriptor& param, float* value, float travel);

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
