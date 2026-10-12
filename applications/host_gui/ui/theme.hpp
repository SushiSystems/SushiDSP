/**************************************************************************/
/* theme.hpp                                                              */
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
 * @file theme.hpp
 * @brief Declares the GUI's palette and the theme written into ImGui's style.
 * @author Mustafa Garip
 */

#pragma once

#include <imgui.h>

#include "ui/geometry.hpp"

namespace ui
{
    /// @brief Every color the GUI draws with.
    struct Palette
    {
            Color window_background;
            Color panel_background;
            Color accent;
            Color text;
            Color muted_text;
            Color meter_normal;
            Color meter_clipping;
    };

    /// @brief The palette, for drawing code that reaches ImDrawList directly.
    /// @return The palette.
    const Palette& palette();

    /// @brief Writes the palette, corner radii and spacing into ImGui's active style. Call once,
    /// after ImGui::CreateContext().
    void apply_theme();

    /// @brief Converts a Geometry Color to the packed form ImDrawList takes.
    /// @param color The 0xRRGGBBAA color.
    /// @return The ImU32 equivalent.
    ImU32 to_imu32(Color color);

    /// @brief Converts a Geometry Color to an ImVec4, for ImGui style slots.
    /// @param color The 0xRRGGBBAA color.
    /// @return The ImVec4 equivalent, each channel in [0, 1].
    ImVec4 to_imvec4(Color color);
} // namespace ui
