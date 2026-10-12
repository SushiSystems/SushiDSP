/**************************************************************************/
/* knob_math.hpp                                                          */
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
 * @file knob_math.hpp
 * @brief Declares the conversions between a parameter value, a knob position and an angle.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/core/node_descriptor.hpp"

namespace ui
{
    /// @brief The knob's sweep, from the 7 o'clock position clockwise to 5 o'clock.
    inline constexpr float K_KNOB_SWEEP_RADIANS = 4.71238898f;

    /// @brief Where the sweep begins, measured clockwise from the positive x axis.
    inline constexpr float K_KNOB_START_RADIANS = 2.35619449f;

    /// @brief Pointer travel, in pixels, that sweeps a knob end to end.
    inline constexpr float K_KNOB_DRAG_PIXELS = 200.0f;

    /// @brief Pointer travel, in pixels, that sweeps a knob end to end while Shift is held.
    inline constexpr float K_KNOB_FINE_DRAG_PIXELS = 800.0f;

    /// @brief Converts a parameter value to its position along the knob's sweep.
    /// @param param The parameter, whose kind selects the taper.
    /// @param value The value; clamped into the parameter's range first.
    /// @return The position, in [0, 1]; 0 for a range with no width, whose sweep means nothing.
    float value_to_normalized(const SushiDSP::ParameterDescriptor& param, float value);

    /// @brief Converts a position along the sweep back to a parameter value.
    /// @param param The parameter, whose kind selects the taper.
    /// @param normalized The position; clamped into [0, 1] first.
    /// @return The value; param.min for a range with no width, which is what
    /// value_to_normalized() maps that range's every value to.
    float normalized_to_value(const SushiDSP::ParameterDescriptor& param, float normalized);

    /// @brief Converts a sweep position to the pointer's angle.
    /// @param normalized The position; clamped into [0, 1] first.
    /// @return The angle in radians, clockwise from the positive x axis.
    float normalized_to_angle(float normalized);

    /// @brief Applies one frame of pointer travel to a sweep position.
    /// @param normalized The current position.
    /// @param pixels_dy The frame's vertical pointer travel, positive downward.
    /// @param fine True while Shift is held.
    /// @return The new position, clamped into [0, 1].
    float apply_drag(float normalized, float pixels_dy, bool fine);
} // namespace ui
