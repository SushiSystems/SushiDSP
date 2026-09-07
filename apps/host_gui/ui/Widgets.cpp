/**************************************************************************/
/* Widgets.cpp                                                            */
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

#include "ui/Widgets.hpp"

#include <cmath>
#include <cstdio>

#include "ui/KnobMath.hpp"
#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Tick marks drawn around a knob's sweep.
        constexpr int kKnobTickCount = 11;

        /// @brief Tick marks drawn beside a fader's slot.
        constexpr int kSliderTickCount = 5;
    }

    bool Knob(const char* id, const SushiDSP::ParameterDescriptor& param, float* value, float diameter)
    {
        ImGui::PushID(id);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("knob", ImVec2(diameter, diameter));
        const bool active = ImGui::IsItemActive();
        const ImVec2 center(origin.x + diameter * 0.5f, origin.y + diameter * 0.5f);
        const float radius = diameter * 0.5f;

        bool changed = false;
        float normalized = value_to_normalized(param, *value);

        if (active)
        {
            const float delta_y = ImGui::GetIO().MouseDelta.y;
            if (delta_y != 0.0f)
            {
                normalized = apply_drag(normalized, delta_y, ImGui::GetIO().KeyShift);
                *value = normalized_to_value(param, normalized);
                changed = true;
            }
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            *value = param.default_value;
            normalized = value_to_normalized(param, *value);
            changed = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s: %.2f %s", param.label, *value, param.unit);
        }

        const Palette& colors = palette();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        draw->AddCircleFilled(center, radius, to_imu32(rgba(20, 20, 24)), 48);
        draw->AddCircleFilled(center, radius * 0.86f, to_imu32(rgba(52, 52, 58)), 48);
        draw->AddCircle(center, radius * 0.86f, to_imu32(rgba(78, 78, 86)), 48, 1.5f);

        for (int tick = 0; tick < kKnobTickCount; ++tick)
        {
            const float position = static_cast<float>(tick) / static_cast<float>(kKnobTickCount - 1);
            const float angle = normalized_to_angle(position);
            const ImVec2 inner(center.x + std::cos(angle) * radius * 0.95f,
                               center.y + std::sin(angle) * radius * 0.95f);
            const ImVec2 outer(center.x + std::cos(angle) * radius * 1.12f,
                               center.y + std::sin(angle) * radius * 1.12f);
            draw->AddLine(inner, outer, to_imu32(colors.muted_text), 1.0f);
        }

        draw->PathArcTo(center, radius * 1.02f, kKnobStartRadians,
                        normalized_to_angle(normalized), 48);
        draw->PathStroke(to_imu32(colors.accent), 0, 3.0f);

        const float pointer_angle = normalized_to_angle(normalized);
        draw->AddLine(center,
                      ImVec2(center.x + std::cos(pointer_angle) * radius * 0.78f,
                             center.y + std::sin(pointer_angle) * radius * 0.78f),
                      to_imu32(colors.text), 2.5f);

        ImGui::PopID();
        return changed;
    }

    bool Slider(const char* id, const SushiDSP::ParameterDescriptor& param, float* value, float travel)
    {
        const float width = travel * kSliderWidthRatio;

        ImGui::PushID(id);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("slider", ImVec2(width, travel));
        const bool active = ImGui::IsItemActive();

        bool changed = false;
        float normalized = value_to_normalized(param, *value);

        if (active)
        {
            const float delta_y = ImGui::GetIO().MouseDelta.y;
            if (delta_y != 0.0f)
            {
                normalized = apply_drag(normalized, delta_y, ImGui::GetIO().KeyShift);
                *value = normalized_to_value(param, normalized);
                changed = true;
            }
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            *value = param.default_value;
            normalized = value_to_normalized(param, *value);
            changed = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s: %.2f %s", param.label, *value, param.unit);
        }

        const Palette& colors = palette();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        const float center_x = origin.x + width * 0.5f;
        const float slot_half = width * 0.11f;
        const ImVec2 slot_top(center_x - slot_half, origin.y);
        const ImVec2 slot_bottom(center_x + slot_half, origin.y + travel);
        draw->AddRectFilled(slot_top, slot_bottom, to_imu32(rgba(20, 20, 24)), slot_half);
        draw->AddRect(slot_top, slot_bottom, to_imu32(rgba(78, 78, 86)), slot_half, 0, 1.5f);

        const float cap_half_height = width * 0.28f;
        const float top_limit = origin.y + cap_half_height;
        const float bottom_limit = origin.y + travel - cap_half_height;
        const float cap_y = bottom_limit - normalized * (bottom_limit - top_limit);

        for (int tick = 0; tick < kSliderTickCount; ++tick)
        {
            const float position = static_cast<float>(tick) / static_cast<float>(kSliderTickCount - 1);
            const float y = bottom_limit - position * (bottom_limit - top_limit);
            draw->AddLine(ImVec2(center_x + width * 0.24f, y), ImVec2(center_x + width * 0.44f, y),
                          to_imu32(colors.muted_text), 1.0f);
        }

        draw->AddLine(ImVec2(center_x, cap_y), ImVec2(center_x, bottom_limit),
                      to_imu32(colors.accent), 3.0f);

        const ImVec2 cap_top(center_x - width * 0.5f, cap_y - cap_half_height);
        const ImVec2 cap_bottom(center_x + width * 0.5f, cap_y + cap_half_height);
        draw->AddRectFilled(cap_top, cap_bottom, to_imu32(rgba(38, 38, 44)), 2.0f);
        draw->AddRect(cap_top, cap_bottom, to_imu32(colors.accent), 2.0f, 0, 1.5f);
        draw->AddLine(ImVec2(cap_top.x + 2.0f, cap_y), ImVec2(cap_bottom.x - 2.0f, cap_y),
                      to_imu32(colors.text), 2.0f);

        ImGui::PopID();
        return changed;
    }

    bool ToggleSwitch(const char* id, bool* on, float width, float height)
    {
        ImGui::PushID(id);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("switch", ImVec2(width, height));

        bool changed = false;
        if (ImGui::IsItemClicked())
        {
            *on = !*on;
            changed = true;
        }

        const Palette& colors = palette();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float radius = height * 0.5f;
        const ImVec2 bottom_right(origin.x + width, origin.y + height);

        draw->AddRectFilled(origin, bottom_right,
                            to_imu32(*on ? colors.accent : rgba(52, 52, 58)), radius);
        const float knob_x = *on ? bottom_right.x - radius : origin.x + radius;
        draw->AddCircleFilled(ImVec2(knob_x, origin.y + radius), radius * 0.78f,
                              to_imu32(rgba(230, 230, 234)), 24);

        ImGui::PopID();
        return changed;
    }

    void ValueReadout(const char* id, const SushiDSP::ParameterDescriptor& param, float value,
                      float width, float height)
    {
        ImGui::PushID(id);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("readout", ImVec2(width, height));

        const Palette& colors = palette();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 bottom_right(origin.x + width, origin.y + height);

        draw->AddRectFilled(origin, bottom_right, to_imu32(rgba(20, 20, 24)), 3.0f);
        draw->AddRect(origin, bottom_right, to_imu32(rgba(78, 78, 86)), 3.0f, 0, 1.5f);

        char text[32];
        std::snprintf(text, sizeof(text), "%.2f", value);
        const ImVec2 text_size = ImGui::CalcTextSize(text);
        draw->AddText(ImVec2(origin.x + (width - text_size.x) * 0.5f,
                             origin.y + (height - text_size.y) * 0.5f),
                      to_imu32(colors.text), text);

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s: %.2f %s", param.label, value, param.unit);
        }

        ImGui::PopID();
    }
}
