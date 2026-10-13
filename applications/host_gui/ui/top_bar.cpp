/**************************************************************************/
/* top_bar.cpp                                                            */
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
 * @file top_bar.cpp
 * @brief Implements the top bar's preset controls, brand mark and device group.
 * @author Mustafa Garip
 */

#include "ui/top_bar.hpp"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <string>
#include <exception>

#include <imgui.h>

#include "session_file.hpp"
#include "ui/theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Width reserved at the bar's right edge for the settings button.
        constexpr float K_SETTINGS_SPAN = 96.0f;

        /// @brief The brand mark drawn centered in the bar, independent of the side groups.
        constexpr const char* K_BRAND_NAME = "SushiDSP";

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

            const char* const ellipsis = "...";
            const float room = width - ImGui::CalcTextSize(ellipsis).x;
            const char* stopped = text.c_str();
            ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), room, 0.0f, text.c_str(), nullptr,
                                            &stopped);
            const std::size_t kept = static_cast<std::size_t>(stopped - text.c_str());
            ImGui::TextColored(color, "%s%s", text.substr(0, kept).c_str(), ellipsis);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", text.c_str());
            }
        }
    } // namespace

    void DrawTopBar(Session* session, bool connected, const std::string& device_label,
                    bool& open_device_settings, std::string& status, bool& rig_replaced,
                    SessionBounce& bounce, const std::string& hrtf_path)
    {
        static char preset_path[256] = "preset.json";
        static char export_path[256] = "mix.wav";
        static int export_format = 0;

        rig_replaced = false;

        const float window_width = ImGui::GetWindowWidth();

        std::string finished;
        if (bounce.take_result(finished))
        {
            status = finished.empty() ? std::string("Exported ") + export_path
                                      : "Export failed: " + finished;
        }

        // Left column: preset path, Save, Load and the last status; see applications/host_gui/ui/README.md.
        if (session != nullptr)
        {
            ImGui::SetNextItemWidth(220.0f);
            ImGui::InputText("##preset_path", preset_path, sizeof(preset_path));
            ImGui::SameLine();

            if (ImGui::Button("Save"))
            {
                try
                {
                    // The session is serialized before the file is opened, which truncates it.
                    const std::string json_text = session_to_json(*session);
                    std::string refusal;
                    status = write_text_file(preset_path, json_text, refusal)
                                 ? std::string("Saved ") + preset_path
                                 : "Save failed: " + refusal;
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
                    std::string json_text;
                    std::string refusal;
                    if (!read_text_file(preset_path, json_text, refusal))
                    {
                        throw std::runtime_error(refusal);
                    }
                    session_from_json(*session, json_text);
                    rig_replaced = true;
                    status = std::string("Loaded ") + preset_path;
                }
                catch (const std::exception& error)
                {
                    status = std::string("Load failed: ") + error.what();
                }
            }
            ImGui::SameLine();

            if (bounce.running())
            {
                ImGui::ProgressBar(bounce.progress(), ImVec2(110.0f, 0.0f), "Exporting");
            }
            else if (ImGui::Button("Export"))
            {
                ImGui::OpenPopup("export_mix");
            }
            if (ImGui::BeginPopup("export_mix"))
            {
                ImGui::TextUnformatted("Export the mix of every track");
                ImGui::SetNextItemWidth(280.0f);
                ImGui::InputText("file", export_path, sizeof(export_path));
                ImGui::SetNextItemWidth(280.0f);
                ImGui::Combo("format", &export_format,
                             "WAV, 16-bit\0WAV, 32-bit float\0MP3, 320 kbit/s\0MP3, 192 kbit/s\0");
                if (ImGui::Button("Export"))
                {
                    try
                    {
                        BounceRequest request;
                        request.session_json = session_to_json(*session);
                        request.path = export_path;
                        request.hrtf_path = hrtf_path;
                        request.info = session->info();
                        request.format = export_format == 0   ? BounceFormat::Wav16
                                         : export_format == 1 ? BounceFormat::Wav32Float
                                                              : BounceFormat::Mp3;
                        request.mp3_kilobits_per_second = export_format == 3 ? 192 : 320;
                        status = bounce.start(std::move(request))
                                     ? std::string("Exporting ") + export_path
                                     : std::string("An export is already running.");
                    }
                    catch (const std::exception& error)
                    {
                        status = std::string("Export failed: ") + error.what();
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (!status.empty())
            {
                ImGui::SameLine();
                draw_elided_text(status, window_width / 2.0f -
                                             ImGui::CalcTextSize(K_BRAND_NAME).x / 2.0f -
                                             ImGui::GetCursorPosX());
            }
        }
        else
        {
            ImGui::TextColored(to_imvec4(palette().muted_text), "No device connected.");
        }

        // Center column: the brand mark, centered on the window; see applications/host_gui/ui/README.md.
        const float brand_width = ImGui::CalcTextSize(K_BRAND_NAME).x;
        ImGui::SameLine((window_width - brand_width) / 2.0f);
        ImGui::TextColored(to_imvec4(palette().text), "%s", K_BRAND_NAME);

        // Right column: the connection pill and Settings; see applications/host_gui/ui/README.md.
        if (connected)
        {
            const float pill_width = ImGui::CalcTextSize(device_label.c_str()).x + 8.0f;
            ImGui::SameLine(window_width - K_SETTINGS_SPAN - pill_width - 12.0f);
            ImGui::TextColored(to_imvec4(palette().muted_text), "%s", device_label.c_str());
        }

        ImGui::SameLine(window_width - K_SETTINGS_SPAN);
        if (ImGui::Button("Settings"))
        {
            open_device_settings = true;
        }
    }
} // namespace ui
