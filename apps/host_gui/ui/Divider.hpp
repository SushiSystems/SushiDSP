/**************************************************************************/
/* Divider.hpp                                                            */
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

#include <cstdint>

#include "ui/DividerMath.hpp"

namespace ui
{
    /// @brief Which cross axis a divider's grab strip spans.
    enum class DividerAxis : std::uint8_t
    {
        Vertical,
        Horizontal
    };

    /// @brief Draws a resizable pane's grab strip: a 14 px-thick invisible button spanning the
    /// region's cross axis, with a centered 3x36 px rounded handle. Advances the cursor like a
    /// normal item.
    /// @param id The strip's ImGui id, scoped by PushID at the call site when it repeats.
    /// @param axis Vertical for a strip between side-by-side panes, Horizontal for stacked panes.
    /// @param size In/out: the pane size the drag adjusts.
    /// @param available The total space split between this pane and the other one along the
    /// resize axis; the strip's own drawn length follows the current content region instead.
    /// @param limits This pane's minimum and the other pane's minimum.
    /// @param invert True when dragging toward the pane should grow it rather than shrink it.
    /// @return True when the drag changed size this frame.
    bool DrawDivider(const char* id, DividerAxis axis, float& size, float available,
                     DividerLimits limits, bool invert);
}
