/**************************************************************************/
/* divider_math.hpp                                                       */
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
 * @file divider_math.hpp
 * @brief Declares the clamping a pane size goes through when a divider moves.
 * @author Mustafa Garip
 */

#pragma once

namespace ui
{
    /// @brief The two minimum sizes a divider drag must respect.
    struct DividerLimits
    {
            float minimum;
            float other_minimum;
    };

    /// @brief Returns the pane size after a drag, clamped so both panes keep their minimum.
    /// @param current The pane's size before the drag.
    /// @param delta The drag distance; positive grows the pane.
    /// @param available The total space split between this pane and the other one.
    /// @param limits This pane's minimum and the other pane's minimum.
    /// @return The clamped size; when available cannot hold both minimums, this pane's minimum.
    float drag_pane_size(float current, float delta, float available,
                         DividerLimits limits) noexcept;

    /// @brief Re-clamps a stored pane size after the window it lives in changes size.
    /// @param current The pane's stored size.
    /// @param available The total space split between this pane and the other one.
    /// @param limits This pane's minimum and the other pane's minimum.
    /// @return The clamped size; when available cannot hold both minimums, this pane's minimum.
    float fit_pane_size(float current, float available, DividerLimits limits) noexcept;
} // namespace ui
