/**************************************************************************/
/* TopBar.cpp                                                             */
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
 * @file TopBar.cpp
 * @brief Implements the top bar's preset controls, brand mark and device group.
 * @author Mustafa Garip
 */

#include "ui/TopBar.hpp"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include <imgui.h>

#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Width reserved at the bar's right edge for the settings button.
        constexpr float kSettingsSpan = 96.0f;

        /// @brief The brand mark drawn centered in the bar, independent of the side groups.
        constexpr const char* kBrandName = "SushiDSP";

        /// @brief Draws one line of muted text, cut short with an ellipsis when it will not fit,
        /// and offers the whole message as a tooltip whenever it was cut. Keeps a long message off
        /// the settings button pinned to the bar's right edge.
        /// @param text The message.
        /// @param width The space the message may occupy; nothing is drawn when none is left.
        void draw_elided_text(const std::string& text, float width)
        {
            if (width <= 0.0f)
            {
                return;
            }

            const ImVec4 color = to_imvec4(palette().muted_text);
            if (ImGui::CalcTextSize(text.c_str()).x <= width)
            {
                ImGui::TextColored(color, "%s", text.c_str());
                return;
            }

            const char* const kEllipsis = "...";
            const float room = width - ImGui::CalcTextSize(kEllipsis).x;
            const char* stopped = text.c_str();
            ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), room, 0.0f, text.c_str(),
                                            nullptr, &stopped);
            const std::size_t kept = static_cast<std::size_t>(stopped - text.c_str());
            ImGui::TextColored(color, "%s%s", text.substr(0, kept).c_str(), kEllipsis);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", text.c_str());
            }
        }
    }

    void DrawTopBar(RigController* rig, bool connected, const std::string& device_label,
                    bool& open_device_settings, std::string& status, bool& rig_replaced)
    {
        static char preset_path[256] = "preset.json";

        rig_replaced = false;

        const float window_width = ImGui::GetWindowWidth();

        // Left column: preset path, Save, Load and the last status; see apps/host_gui/ui/README.md.
        if (rig != nullptr)
        {
            ImGui::SetNextItemWidth(280.0f);
            ImGui::InputText("##preset_path", preset_path, sizeof(preset_path));
            ImGui::SameLine();

            if (ImGui::Button("Save"))
            {
                try
                {
                    // Opening the stream truncates the file, and save_to_json() can throw.
                    const std::string json_text = rig->save_to_json();
                    std::ofstream out(preset_path);
                    if (!out)
                    {
                        throw std::runtime_error("could not open '" + std::string(preset_path)
                                                 + "' for writing");
                    }
                    out << json_text;
                    status = std::string("Saved ") + preset_path;
                }
                catch (const std::exception& error)
                {
                    status = std::string("Save failed: ") + error.what();
                }
            }
            ImGui::SameLine();

            if (ImGui::Button("Load"))
            {
                try
                {
                    std::ifstream in(preset_path);
                    if (!in)
                    {
                        throw std::runtime_error("could not open '" + std::string(preset_path)
                                                 + "' for reading");
                    }
                    const std::string json_text((std::istreambuf_iterator<char>(in)),
                                                std::istreambuf_iterator<char>());
                    rig->load_from_json(json_text);
                    rig_replaced = true;
                    status = std::string("Loaded ") + preset_path;
                }
                catch (const std::exception& error)
                {
                    status = std::string("Load failed: ") + error.what();
                }
            }

            if (!status.empty())
            {
                ImGui::SameLine();
                draw_elided_text(status, window_width / 2.0f
                                         - ImGui::CalcTextSize(kBrandName).x / 2.0f
                                         - ImGui::GetCursorPosX());
            }
        }
        else
        {
            ImGui::TextColored(to_imvec4(palette().muted_text), "No device connected.");
        }

        // Center column: the brand mark, centered on the window; see apps/host_gui/ui/README.md.
        const float brand_width = ImGui::CalcTextSize(kBrandName).x;
        ImGui::SameLine((window_width - brand_width) / 2.0f);
        ImGui::TextColored(to_imvec4(palette().text), "%s", kBrandName);

        // Right column: the connection pill and Settings; see apps/host_gui/ui/README.md.
        if (connected)
        {
            const float pill_width = ImGui::CalcTextSize(device_label.c_str()).x + 8.0f;
            ImGui::SameLine(window_width - kSettingsSpan - pill_width - 12.0f);
            ImGui::TextColored(to_imvec4(palette().muted_text), "%s", device_label.c_str());
        }

        ImGui::SameLine(window_width - kSettingsSpan);
        if (ImGui::Button("Settings"))
        {
            open_device_settings = true;
        }
    }
}
