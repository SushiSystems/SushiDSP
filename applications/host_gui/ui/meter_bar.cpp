/**************************************************************************/
/* meter_bar.cpp                                                          */
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
 * @file meter_bar.cpp
 * @brief Implements the input and output level meters.
 * @author Mustafa Garip
 */

#include "ui/meter_bar.hpp"

#include <algorithm>

#include <imgui.h>

#include "ui/level_text.hpp"
#include "ui/theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Fraction of the displayed level kept each frame while the signal is falling.
        constexpr float K_FALL_PER_FRAME = 0.85f;

        /// @brief Width of one meter's fill track, in pixels.
        constexpr float K_TRACK_WIDTH = 70.0f;

        /// @brief Height of one meter's fill track, in pixels.
        constexpr float K_TRACK_HEIGHT = 10.0f;

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
            draw->AddRectFilled(origin, ImVec2(origin.x + K_TRACK_WIDTH, origin.y + K_TRACK_HEIGHT),
                                to_imu32(rgba(24, 24, 28)), 3.0f);
            draw->AddRectFilled(
                origin, ImVec2(origin.x + K_TRACK_WIDTH * fill, origin.y + K_TRACK_HEIGHT),
                to_imu32(displayed_peak >= 1.0f ? colors.meter_clipping : colors.meter_normal),
                3.0f);
            ImGui::Dummy(ImVec2(K_TRACK_WIDTH, K_TRACK_HEIGHT));
            ImGui::SameLine();
            ImGui::TextUnformatted(decibel_label(decibels).c_str());
        }
    } // namespace

    void DrawInlineMeters(LevelProbe& input, LevelProbe& output)
    {
        static float displayed_input = 0.0f;
        static float displayed_output = 0.0f;

        const float input_peak = input.read_peak();
        const float output_peak = output.read_peak();
        displayed_input = std::max(input_peak, displayed_input * K_FALL_PER_FRAME);
        displayed_output = std::max(output_peak, displayed_output * K_FALL_PER_FRAME);

        draw_bar("In", displayed_input);
        ImGui::SameLine();
        draw_bar("Out", displayed_output);
    }
} // namespace ui
