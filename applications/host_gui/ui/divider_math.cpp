/**************************************************************************/
/* divider_math.cpp                                                       */
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
 * @file divider_math.cpp
 * @brief Implements the clamping a pane size goes through when a divider moves.
 * @author Mustafa Garip
 */

#include "ui/divider_math.hpp"

#include <algorithm>

namespace ui
{
    namespace
    {
        /// @brief Clamps size to [limits.minimum, available - limits.other_minimum].
        /// @param size The candidate pane size.
        /// @param available The total space split between this pane and the other one.
        /// @param limits This pane's minimum and the other pane's minimum.
        /// @return The clamped size; this pane's minimum when available cannot hold both.
        float clamp_pane_size(float size, float available, DividerLimits limits) noexcept
        {
            const float maximum = available - limits.other_minimum;
            if (maximum < limits.minimum)
            {
                return limits.minimum;
            }
            return std::clamp(size, limits.minimum, maximum);
        }
    } // namespace

    float drag_pane_size(float current, float delta, float available, DividerLimits limits) noexcept
    {
        return clamp_pane_size(current + delta, available, limits);
    }

    float fit_pane_size(float current, float available, DividerLimits limits) noexcept
    {
        return clamp_pane_size(current, available, limits);
    }
} // namespace ui
