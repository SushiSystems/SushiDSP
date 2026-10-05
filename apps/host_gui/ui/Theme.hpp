/****************************************************************/
/* Theme.hpp                                                    */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include <imgui.h>

#include "ui/Geometry.hpp"

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
}
