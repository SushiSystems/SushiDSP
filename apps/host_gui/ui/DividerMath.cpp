/****************************************************************/
/* DividerMath.cpp                                              */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file DividerMath.cpp
 * @brief Implements the clamping a pane size goes through when a divider moves.
 * @author Mustafa Garip
 */

#include "ui/DividerMath.hpp"

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
    }

    float drag_pane_size(float current, float delta, float available,
                         DividerLimits limits) noexcept
    {
        return clamp_pane_size(current + delta, available, limits);
    }

    float fit_pane_size(float current, float available, DividerLimits limits) noexcept
    {
        return clamp_pane_size(current, available, limits);
    }
}
