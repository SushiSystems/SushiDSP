/**************************************************************************/
/* Divider.hpp                                                            */
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
 * @file Divider.hpp
 * @brief Declares the grab strip and the seam drawn between panes.
 * @author Mustafa Garip
 */

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

    /// @brief Draws a pane's grab strip and applies its drag; see apps/host_gui/ui/README.md.
    /// @param id The strip's ImGui id, scoped by PushID at the call site when it repeats.
    /// @param axis Vertical for a strip between side-by-side panes, Horizontal for stacked panes.
    /// @param size In/out: the pane size the drag adjusts.
    /// @param available The space this pane and the other one share along the resize axis.
    /// @param limits This pane's minimum and the other pane's minimum.
    /// @param invert True when dragging toward the pane should grow it rather than shrink it.
    /// @return True when the drag changed size this frame.
    bool DrawDivider(const char* id, DividerAxis axis, float& size, float available,
                     DividerLimits limits, bool invert);

    /// @brief Draws the boundary between two panes that do not resize: a hairline down the
    /// middle of a strip, with no grab handle, so the seam does not invite a drag it will not
    /// answer. Advances the cursor like a normal item.
    /// @param axis Vertical for a strip between side-by-side panes, Horizontal for stacked panes.
    /// @param thickness The strip's thickness across its cross axis, in pixels.
    void DrawPaneSeparator(DividerAxis axis, float thickness);
}
