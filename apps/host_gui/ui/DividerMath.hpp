/****************************************************************/
/* DividerMath.hpp                                              */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
}
