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
 * @brief Draws the cabinet section's mixer and writes its edits through the rig.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/mixer_panel.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <utility>
#include <vector>

#include <imgui.h>

namespace ui
{
    namespace
    {
        using SlotId = RigController::SlotId;

        /// @brief Holds the width of a strip's continuous slider, in pixels.
        constexpr float K_SLIDER_WIDTH = 160.0f;

        /// @brief Holds the bus's input count, the size of its mute and solo banks.
        constexpr std::size_t K_BUS_INPUTS = 8;

        /// @brief Holds the room bus's inputs before the first microphone: the two ears.
        constexpr std::size_t K_ROOM_BUS_EARS = 2;

        /// @brief Draws one continuous parameter as a slider and writes it on change.
        /// @return True on the frame the value changed.
        bool draw_slider(RigController& rig, SlotId slot, const char* param_id, const char* label,
                         float min, float max, const char* format)
        {
            float value = rig.get_parameter(slot, param_id);
            ImGui::SetNextItemWidth(K_SLIDER_WIDTH);
            if (!ImGui::SliderFloat(label, &value, min, max, format))
            {
                return false;
            }
            rig.set_parameter(slot, param_id, value);
            return true;
        }

        /// @brief Draws one 0-or-1 parameter as a checkbox and writes it on change.
        /// @return True on the frame the value changed.
        bool draw_toggle(RigController& rig, SlotId slot, const char* param_id, const char* label)
        {
            bool on = rig.get_parameter(slot, param_id) >= 0.5f;
            if (!ImGui::Checkbox(label, &on))
            {
                return false;
            }
            rig.set_parameter(slot, param_id, on ? 1.0f : 0.0f);
            return true;
        }

        /// @brief Draws a channel strip's row: level, pan, polarity and delay.
        /// @return True on the frame any of them changed.
        bool draw_strip_row(RigController& rig, SlotId slot, const char* name)
        {
            ImGui::PushID(static_cast<int>(slot));
            ImGui::TextUnformatted(name);
            bool changed = draw_slider(rig, slot, "level", "level", -60.0f, 12.0f, "%.1f dB");
            ImGui::SameLine();
            changed = draw_slider(rig, slot, "pan", "pan", -1.0f, 1.0f, "%.2f") || changed;
            ImGui::SameLine();
            changed = draw_toggle(rig, slot, "polarity", "invert") || changed;
            ImGui::SameLine();
            changed = draw_slider(rig, slot, "delay", "delay", 0.0f, 50.0f, "%.2f ms") || changed;
            ImGui::PopID();
            return changed;
        }

        /// @brief Draws a microphone's row: its id, polarity and room flag.
        /// @return True on the frame either changed.
        bool draw_microphone_row(RigController& rig, std::uint32_t microphone_id, SlotId slot)
        {
            ImGui::PushID(static_cast<int>(slot));
            ImGui::Text("Microphone %u", static_cast<unsigned>(microphone_id));
            ImGui::SameLine();
            bool changed = draw_toggle(rig, slot, "polarity", "invert");
            ImGui::SameLine();
            changed = draw_toggle(rig, slot, "room", "room") || changed;
            ImGui::PopID();
            return changed;
        }

        /// @brief Draws one bus input's mute and solo boxes under the given label.
        /// @return True on the frame either changed.
        bool draw_bus_input_row(RigController& rig, SlotId slot, std::size_t input,
                                const char* label)
        {
            char mute_id[16];
            char solo_id[16];
            std::snprintf(mute_id, sizeof(mute_id), "mute_%zu", input);
            std::snprintf(solo_id, sizeof(solo_id), "solo_%zu", input);
            ImGui::PushID(static_cast<int>(input));
            ImGui::TextUnformatted(label);
            ImGui::SameLine(K_SLIDER_WIDTH);
            bool changed = draw_toggle(rig, slot, mute_id, "mute");
            ImGui::SameLine();
            changed = draw_toggle(rig, slot, solo_id, "solo") || changed;
            ImGui::PopID();
            return changed;
        }

        /// @brief Draws the room bus: the two ears, then one row per microphone in layout order.
        /// @return True on the frame any input changed.
        bool draw_room_bus(RigController& rig, SlotId slot,
                           const std::vector<std::pair<std::uint32_t, SlotId>>& microphones)
        {
            ImGui::PushID(static_cast<int>(slot));
            ImGui::TextUnformatted("Room bus");
            bool changed = draw_bus_input_row(rig, slot, 0, "Listener left ear");
            changed = draw_bus_input_row(rig, slot, 1, "Listener right ear") || changed;
            std::size_t input = K_ROOM_BUS_EARS;
            for (const auto& [microphone_id, microphone_slot] : microphones)
            {
                if (input >= K_BUS_INPUTS)
                {
                    break;
                }
                char label[32];
                std::snprintf(label, sizeof(label), "Microphone %u",
                              static_cast<unsigned>(microphone_id));
                changed = draw_bus_input_row(rig, slot, input, label) || changed;
                ++input;
            }
            ImGui::PopID();
            return changed;
        }

        /// @brief Draws the master bus: the chain and room output, then the DI strip.
        /// @return True on the frame either input changed.
        bool draw_master_bus(RigController& rig, SlotId slot)
        {
            ImGui::PushID(static_cast<int>(slot));
            ImGui::TextUnformatted("Master bus");
            bool changed = draw_bus_input_row(rig, slot, 0, "Chain and room");
            changed = draw_bus_input_row(rig, slot, 1, "DI") || changed;
            ImGui::PopID();
            return changed;
        }

        /// @brief Draws a strip row when the slot is built.
        /// @return True on the frame the strip changed.
        bool draw_optional_strip(RigController& rig, const std::optional<SlotId>& slot,
                                 const char* name)
        {
            return slot.has_value() && draw_strip_row(rig, *slot, name);
        }
    } // namespace

    bool draw_mixer_panel(const char* id, RigController& rig,
                          const TopologyApplier::MixerSlots& slots)
    {
        ImGui::PushID(id);
        bool changed = draw_optional_strip(rig, slots.room_left, "Room left");
        changed = draw_optional_strip(rig, slots.room_right, "Room right") || changed;
        for (const auto& [microphone_id, slot] : slots.microphones)
        {
            changed = draw_microphone_row(rig, microphone_id, slot) || changed;
        }
        if (slots.room_bus.has_value())
        {
            ImGui::Separator();
            changed = draw_room_bus(rig, *slots.room_bus, slots.microphones) || changed;
        }
        if (slots.di_strip.has_value() || slots.master_bus.has_value())
        {
            ImGui::Separator();
        }
        changed = draw_optional_strip(rig, slots.di_strip, "DI") || changed;
        if (slots.master_bus.has_value())
        {
            changed = draw_master_bus(rig, *slots.master_bus) || changed;
        }
        ImGui::PopID();
        return changed;
    }
} // namespace ui
