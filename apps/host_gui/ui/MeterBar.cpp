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

#include <imgui.h>

#include "ui/LevelText.hpp"
#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Fraction of the displayed level kept each frame while the signal is falling.
        constexpr float kFallPerFrame = 0.85f;

        /// @brief Width of one meter's fill track, in pixels.
        constexpr float kTrackWidth = 70.0f;

        /// @brief Height of one meter's fill track, in pixels.
        constexpr float kTrackHeight = 10.0f;

        /// @brief Draws one labelled bar for a level already converted to a display peak.
        void draw_bar(const char* label, float displayed_peak)
        {
            const Palette& colors = palette();
            const float decibels = peak_to_decibels(displayed_peak);
            const float fill = decibels_to_fill(decibels);

            ImGui::TextUnformatted(label);
            ImGui::SameLine();

            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin, ImVec2(origin.x + kTrackWidth, origin.y + kTrackHeight),
                                to_imu32(rgba(24, 24, 28)), 3.0f);
            draw->AddRectFilled(
                origin, ImVec2(origin.x + kTrackWidth * fill, origin.y + kTrackHeight),
                to_imu32(displayed_peak >= 1.0f ? colors.meter_clipping : colors.meter_normal),
                3.0f);
            ImGui::Dummy(ImVec2(kTrackWidth, kTrackHeight));
            ImGui::SameLine();
            ImGui::TextUnformatted(decibel_label(decibels).c_str());
        }
    }

    void DrawInlineMeters(LevelProbe& input, LevelProbe& output)
    {
        static float displayed_input = 0.0f;
        static float displayed_output = 0.0f;

        const float input_peak = input.read_peak();
        const float output_peak = output.read_peak();
        displayed_input = std::max(input_peak, displayed_input * kFallPerFrame);
        displayed_output = std::max(output_peak, displayed_output * kFallPerFrame);

        draw_bar("In", displayed_input);
        ImGui::SameLine();
        draw_bar("Out", displayed_output);
    }
}
