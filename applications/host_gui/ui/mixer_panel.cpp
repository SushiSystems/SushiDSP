/**************************************************************************/
/* mixer_panel.cpp                                                        */
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
 * @file mixer_panel.cpp
 * @brief Implements the mixer panel's strips, faders and switches.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/mixer_panel.hpp"

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "ui/mixer_model.hpp"
#include "ui/theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Holds one strip's width, in pixels.
        constexpr float K_STRIP_WIDTH = 92.0f;

        /// @brief Holds the height of a strip's level fader, in pixels.
        constexpr float K_FADER_HEIGHT = 150.0f;

        /// @brief Holds the width of a strip's level fader, in pixels.
        constexpr float K_FADER_WIDTH = 26.0f;

        /// @brief Holds the lowest level a fader reaches, in decibels.
        constexpr float K_MIN_LEVEL_DB = -60.0f;

        /// @brief Holds the highest level a fader reaches, in decibels.
        constexpr float K_MAX_LEVEL_DB = 12.0f;

        /// @brief Draws one continuous parameter as a slider across the strip and writes it on
        /// change.
        void draw_slider(RigController& rig, SlotId slot, const char* param_id, const char* label,
                         float min, float max, const char* format)
        {
            float value = rig.get_parameter(slot, param_id);
            ImGui::SetNextItemWidth(K_STRIP_WIDTH - 8.0f);
            if (ImGui::SliderFloat(label, &value, min, max, format))
            {
                rig.set_parameter(slot, param_id, value);
            }
        }

        /// @brief Draws one switch as a button that is lit while the switch is on, and writes it
        /// on a press.
        /// @return True on the frame the switch changed.
        bool draw_switch(RigController& rig, SlotId slot, const char* param_id, const char* label,
                         Color lit)
        {
            const bool on = rig.get_parameter(slot, param_id) >= 0.5f;
            if (on)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, to_imvec4(lit));
            }
            const bool pressed = ImGui::Button(label, ImVec2(K_STRIP_WIDTH * 0.5f - 6.0f, 0.0f));
            if (on)
            {
                ImGui::PopStyleColor();
            }
            if (pressed)
            {
                rig.set_parameter(slot, param_id, on ? 0.0f : 1.0f);
            }
            return pressed;
        }

        /// @brief Draws the mute and solo buttons of the bus input a strip feeds.
        void draw_flags(RigController& rig, const MixerFlags& flags)
        {
            char mute_id[16];
            char solo_id[16];
            std::snprintf(mute_id, sizeof(mute_id), "mute_%zu", flags.input);
            std::snprintf(solo_id, sizeof(solo_id), "solo_%zu", flags.input);
            ImGui::PushID(static_cast<int>(flags.bus));
            draw_switch(rig, flags.bus, mute_id, "M", rgba(200, 72, 60));
            ImGui::SameLine(0.0f, 4.0f);
            draw_switch(rig, flags.bus, solo_id, "S", rgba(214, 168, 74));
            ImGui::PopID();
        }

        /// @brief Draws a strip's level fader with its reading under it.
        void draw_fader(RigController& rig, SlotId slot)
        {
            float level = rig.get_parameter(slot, "level");
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (K_STRIP_WIDTH - K_FADER_WIDTH) * 0.5f -
                                 4.0f);
            if (ImGui::VSliderFloat("##level", ImVec2(K_FADER_WIDTH, K_FADER_HEIGHT), &level,
                                    K_MIN_LEVEL_DB, K_MAX_LEVEL_DB, ""))
            {
                rig.set_parameter(slot, "level", level);
            }
            ImGui::TextColored(to_imvec4(palette().muted_text), "%+.1f dB", level);
        }

        /// @brief Draws one strip's column.
        void draw_strip(RigController& rig, const MixerStrip& strip, std::size_t index,
                        MixerEdit& edit)
        {
            ImGui::PushID(static_cast<int>(index));
            ImGui::BeginChild("##strip", ImVec2(K_STRIP_WIDTH, 0.0f), true);
            ImGui::TextUnformatted(strip.label.c_str());

            if (strip.strip.has_value())
            {
                ImGui::PushID(static_cast<int>(*strip.strip));
                draw_fader(rig, *strip.strip);
                if (strip.stereo)
                {
                    draw_slider(rig, *strip.strip, "balance", "##balance", -1.0f, 1.0f, "bal %.2f");
                    draw_slider(rig, *strip.strip, "width", "##width", 0.0f, 1.0f, "wid %.2f");
                }
                else
                {
                    draw_slider(rig, *strip.strip, "pan", "##pan", -1.0f, 1.0f, "pan %.2f");
                    draw_slider(rig, *strip.strip, "delay", "##delay", 0.0f, 50.0f, "%.1f ms");
                }
                ImGui::PopID();
            }
            else
            {
                ImGui::Dummy(ImVec2(K_FADER_WIDTH, K_FADER_HEIGHT));
                ImGui::TextColored(to_imvec4(palette().muted_text), "no fader");
            }

            if (strip.flags.has_value())
            {
                draw_flags(rig, *strip.flags);
            }

            if (strip.microphone.has_value())
            {
                ImGui::PushID(static_cast<int>(*strip.microphone));
                draw_switch(rig, *strip.microphone, "polarity", "Inv", rgba(96, 160, 220));
                ImGui::SameLine(0.0f, 4.0f);
                edit.rewire =
                    draw_switch(rig, *strip.microphone, "room", "Room", rgba(120, 200, 120)) ||
                    edit.rewire;
                if (ImGui::Button("Remove", ImVec2(K_STRIP_WIDTH - 8.0f, 0.0f)))
                {
                    edit.remove_microphone = *strip.microphone;
                }
                ImGui::PopID();
            }
            else if (strip.strip.has_value() && !strip.stereo)
            {
                ImGui::PushID(static_cast<int>(*strip.strip));
                draw_switch(rig, *strip.strip, "polarity", "Inv", rgba(96, 160, 220));
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::PopID();
        }
    } // namespace

    MixerEdit draw_mixer_panel(const char* id, RigController& rig, const HiddenSlots& slots,
                               JoinContent join)
    {
        MixerEdit edit;
        const std::vector<MixerStrip> strips = mixer_strips(slots, join);
        ImGui::PushID(id);
        if (strips.empty())
        {
            ImGui::TextColored(to_imvec4(palette().muted_text),
                               "Place gear in the rig to open the mixer.");
        }
        for (std::size_t index = 0; index < strips.size(); ++index)
        {
            if (index > 0)
            {
                ImGui::SameLine(0.0f, 4.0f);
            }
            draw_strip(rig, strips[index], index, edit);
        }
        ImGui::PopID();
        return edit;
    }
} // namespace ui
