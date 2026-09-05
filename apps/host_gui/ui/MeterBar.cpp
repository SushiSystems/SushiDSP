/**************************************************************************/
/* MeterBar.cpp                                                           */
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

#include "ui/MeterBar.hpp"

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Fraction of the displayed level kept each frame while the signal is falling.
        constexpr float kFallPerFrame = 0.85f;

        /// @brief Level, in dBFS, that the left end of a bar represents.
        constexpr float kFloorDecibels = -60.0f;

        /// @brief Converts a magnitude to dBFS, flooring silence at kFloorDecibels.
        float to_decibels(float magnitude)
        {
            return magnitude <= 0.0f ? kFloorDecibels
                                     : std::max(kFloorDecibels, 20.0f * std::log10(magnitude));
        }

        /// @brief Draws one labelled bar and returns the level it displayed.
        void draw_bar(const char* label, float displayed, float width)
        {
            const Palette& colors = palette();
            const float decibels = to_decibels(displayed);
            const float fill = (decibels - kFloorDecibels) / (0.0f - kFloorDecibels);

            ImGui::TextUnformatted(label);
            ImGui::SameLine();

            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float height = 14.0f;
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height),
                                to_imu32(rgba(24, 24, 28)), 3.0f);
            draw->AddRectFilled(origin,
                                ImVec2(origin.x + width * std::clamp(fill, 0.0f, 1.0f), origin.y + height),
                                to_imu32(displayed >= 1.0f ? colors.meter_clipping : colors.meter_normal),
                                3.0f);
            ImGui::Dummy(ImVec2(width, height));
            ImGui::SameLine();
            ImGui::Text("%.1f dB", decibels);
        }
    }

    void DrawMeterBar(LevelProbe& input, LevelProbe& output)
    {
        static float displayed_input = 0.0f;
        static float displayed_output = 0.0f;

        const float input_peak = input.read_peak();
        const float output_peak = output.read_peak();
        displayed_input = std::max(input_peak, displayed_input * kFallPerFrame);
        displayed_output = std::max(output_peak, displayed_output * kFallPerFrame);

        const float bar_width = std::max(80.0f, ImGui::GetContentRegionAvail().x * 0.3f);
        draw_bar("In", displayed_input, bar_width);
        ImGui::SameLine();
        draw_bar("Out", displayed_output, bar_width);
    }
}
