/**************************************************************************/
/* DividerMath.cpp                                                        */
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
