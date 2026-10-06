/**************************************************************************/
/* Divider.cpp                                                            */
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
 * @file Divider.cpp
 * @brief Implements the grab strip and the seam drawn between panes.
 * @author Mustafa Garip
 */

#include "ui/Divider.hpp"

#include <imgui.h>

#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief The grab strip's thickness across its cross axis, in pixels.
        constexpr float kStripThickness = 14.0f;

        /// @brief The handle's length along the strip and thickness across it, in pixels.
        constexpr float kHandleLength = 36.0f;
        constexpr float kHandleThickness = 3.0f;
    }

    bool DrawDivider(const char* id, DividerAxis axis, float& size, float available,
                     DividerLimits limits, bool invert)
    {
        ImGui::PushID(id);

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const bool   vertical = axis == DividerAxis::Vertical;
        const float  cross_length =
            vertical ? ImGui::GetContentRegionAvail().y : ImGui::GetContentRegionAvail().x;
        const ImVec2 strip_size = vertical ? ImVec2(kStripThickness, cross_length)
                                           : ImVec2(cross_length, kStripThickness);

        ImGui::InvisibleButton("divider", strip_size);

        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered || active)
        {
            ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        }

        bool changed = false;
        if (active)
        {
            const ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
            float        delta = vertical ? mouse_delta.x : mouse_delta.y;
            if (invert)
            {
                delta = -delta;
            }
            if (delta != 0.0f)
            {
                const float dragged = drag_pane_size(size, delta, available, limits);
                if (dragged != size)
                {
                    size = dragged;
                    changed = true;
                }
            }
        }

        const Palette& colors = palette();
        ImDrawList*    draw = ImGui::GetWindowDrawList();
        const ImU32    handle_color = to_imu32((hovered || active) ? colors.accent
                                                                   : colors.muted_text);

        const ImVec2 center(origin.x + strip_size.x * 0.5f, origin.y + strip_size.y * 0.5f);
        const ImVec2 handle_min = vertical
            ? ImVec2(center.x - kHandleThickness * 0.5f, center.y - kHandleLength * 0.5f)
            : ImVec2(center.x - kHandleLength * 0.5f, center.y - kHandleThickness * 0.5f);
        const ImVec2 handle_max = vertical
            ? ImVec2(center.x + kHandleThickness * 0.5f, center.y + kHandleLength * 0.5f)
            : ImVec2(center.x + kHandleLength * 0.5f, center.y + kHandleThickness * 0.5f);

        draw->AddRectFilled(handle_min, handle_max, handle_color, kHandleThickness * 0.5f);

        ImGui::PopID();
        return changed;
    }

    void DrawPaneSeparator(DividerAxis axis, float thickness)
    {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const bool   vertical = axis == DividerAxis::Vertical;
        const float  cross_length =
            vertical ? ImGui::GetContentRegionAvail().y : ImGui::GetContentRegionAvail().x;
        const ImVec2 strip_size = vertical ? ImVec2(thickness, cross_length)
                                           : ImVec2(cross_length, thickness);

        ImGui::Dummy(strip_size);

        const ImVec2 center(origin.x + strip_size.x * 0.5f, origin.y + strip_size.y * 0.5f);
        const ImVec2 line_min = vertical ? ImVec2(center.x - 0.5f, origin.y)
                                         : ImVec2(origin.x, center.y - 0.5f);
        const ImVec2 line_max = vertical ? ImVec2(center.x + 0.5f, origin.y + strip_size.y)
                                         : ImVec2(origin.x + strip_size.x, center.y + 0.5f);
        ImGui::GetWindowDrawList()->AddRectFilled(line_min, line_max,
                                                  to_imu32(palette().muted_text));
    }
}
