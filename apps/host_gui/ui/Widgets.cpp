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
}
