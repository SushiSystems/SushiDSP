/**************************************************************************/
/* DeviceSettings.cpp                                                     */
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
 * @file DeviceSettings.cpp
 * @brief Implements the device and channel settings window.
 * @author Mustafa Garip
 */

#include "ui/DeviceSettings.hpp"

#include <cstddef>

#include <imgui.h>

#include "ui/Theme.hpp"

namespace ui
{
    bool DrawDeviceSettings(const std::vector<std::string>& asio_drivers, DeviceChoice& choice,
                            bool already_running, const std::string& error, bool& open)
    {
        if (!open)
        {
            return false;
        }

        bool connect_clicked = false;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 center(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                            viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
        if (ImGui::Begin("Audio device", &open, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::TextUnformatted("Device");
            if (ImGui::RadioButton("SDL2 (no input)", choice.device_index == 0))
            {
                choice.device_index = 0;
            }
            for (std::size_t i = 0; i < asio_drivers.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::RadioButton(asio_drivers[i].c_str(),
                                       choice.device_index == static_cast<int>(i) + 1))
                {
                    choice.device_index = static_cast<int>(i) + 1;
                }
                ImGui::PopID();
            }

            if (choice.device_index > 0)
            {
                ImGui::Separator();
                ImGui::TextUnformatted("Input");
                if (ImGui::RadioButton("L##input", choice.input == SushiDSP::ChannelSelection::Left))
                {
                    choice.input = SushiDSP::ChannelSelection::Left;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("R##input", choice.input == SushiDSP::ChannelSelection::Right))
                {
                    choice.input = SushiDSP::ChannelSelection::Right;
                }

                ImGui::TextUnformatted("Output");
                if (ImGui::RadioButton("L##output", choice.output == SushiDSP::ChannelSelection::Left))
                {
                    choice.output = SushiDSP::ChannelSelection::Left;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("R##output", choice.output == SushiDSP::ChannelSelection::Right))
                {
                    choice.output = SushiDSP::ChannelSelection::Right;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("stereo##output", choice.output == SushiDSP::ChannelSelection::Stereo))
                {
                    choice.output = SushiDSP::ChannelSelection::Stereo;
                }
            }

            ImGui::Separator();
            if (already_running)
            {
                ImGui::TextColored(to_imvec4(rgba(214, 168, 74)),
                    "Connecting replaces the current rig with an empty one.");
            }
            if (!error.empty())
            {
                ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "%s", error.c_str());
            }

            if (ImGui::Button("Connect"))
            {
                connect_clicked = true;
            }
            if (already_running)
            {
                ImGui::SameLine();
                if (ImGui::Button("Close"))
                {
                    open = false;
                }
            }
        }
        ImGui::End();

        return connect_clicked;
    }
}
