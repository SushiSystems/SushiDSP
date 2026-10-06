/**************************************************************************/
/* Theme.cpp                                                              */
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
 * @file Theme.cpp
 * @brief Implements the GUI's palette and the theme written into ImGui's style.
 * @author Mustafa Garip
 */

#include "ui/Theme.hpp"

namespace ui
{
    const Palette& palette()
    {
        static const Palette colors{
            rgba(18, 18, 20),
            rgba(28, 28, 32),
            rgba(214, 168, 74),
            rgba(228, 228, 232),
            rgba(140, 140, 148),
            rgba(96, 200, 112),
            rgba(220, 72, 60),
        };
        return colors;
    }

    ImU32 to_imu32(Color color)
    {
        const ImU32 r = (color >> 24) & 0xFFu;
        const ImU32 g = (color >> 16) & 0xFFu;
        const ImU32 b = (color >> 8) & 0xFFu;
        const ImU32 a = color & 0xFFu;
        return IM_COL32(r, g, b, a);
    }

    ImVec4 to_imvec4(Color color)
    {
        return ImVec4(
            static_cast<float>((color >> 24) & 0xFFu) / 255.0f,
            static_cast<float>((color >> 16) & 0xFFu) / 255.0f,
            static_cast<float>((color >> 8) & 0xFFu) / 255.0f,
            static_cast<float>(color & 0xFFu) / 255.0f);
    }

    void apply_theme()
    {
        const Palette& colors = palette();
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding = 0.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 1.0f;
        style.WindowPadding = ImVec2(12.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 5.0f);
        style.ItemSpacing = ImVec2(8.0f, 7.0f);
        style.ScrollbarSize = 12.0f;

        style.Colors[ImGuiCol_WindowBg] = to_imvec4(colors.window_background);
        style.Colors[ImGuiCol_ChildBg] = to_imvec4(colors.panel_background);
        style.Colors[ImGuiCol_PopupBg] = to_imvec4(colors.panel_background);
        style.Colors[ImGuiCol_Text] = to_imvec4(colors.text);
        style.Colors[ImGuiCol_TextDisabled] = to_imvec4(colors.muted_text);
        style.Colors[ImGuiCol_FrameBg] = to_imvec4(colors.panel_background);
        style.Colors[ImGuiCol_FrameBgHovered] = to_imvec4(rgba(44, 44, 50));
        style.Colors[ImGuiCol_FrameBgActive] = to_imvec4(rgba(56, 56, 62));
        style.Colors[ImGuiCol_Border] = to_imvec4(rgba(58, 58, 64));
        style.Colors[ImGuiCol_Button] = to_imvec4(rgba(44, 44, 50));
        style.Colors[ImGuiCol_ButtonHovered] = to_imvec4(rgba(60, 60, 68));
        style.Colors[ImGuiCol_ButtonActive] = to_imvec4(colors.accent);
        style.Colors[ImGuiCol_Header] = to_imvec4(rgba(44, 44, 50));
        style.Colors[ImGuiCol_HeaderHovered] = to_imvec4(rgba(60, 60, 68));
        style.Colors[ImGuiCol_HeaderActive] = to_imvec4(colors.accent);
        style.Colors[ImGuiCol_Separator] = to_imvec4(rgba(48, 48, 54));
        style.Colors[ImGuiCol_CheckMark] = to_imvec4(colors.accent);
        style.Colors[ImGuiCol_TitleBg] = to_imvec4(colors.panel_background);
        style.Colors[ImGuiCol_TitleBgActive] = to_imvec4(rgba(44, 44, 50));
    }
}
