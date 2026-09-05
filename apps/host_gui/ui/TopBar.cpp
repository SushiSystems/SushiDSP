/**************************************************************************/
/* TopBar.cpp                                                             */
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

#include "ui/TopBar.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

#include <imgui.h>

#include "ui/Theme.hpp"

namespace ui
{
    void DrawTopBar(RigController& rig, const std::string& device_label,
                    bool& open_device_settings, std::string& status)
    {
        static char preset_path[256] = "preset.json";

        ImGui::SetNextItemWidth(280.0f);
        ImGui::InputText("##preset_path", preset_path, sizeof(preset_path));
        ImGui::SameLine();

        if (ImGui::Button("Save"))
        {
            try
            {
                std::ofstream out(preset_path);
                if (!out)
                {
                    throw std::runtime_error("could not open '" + std::string(preset_path)
                                             + "' for writing");
                }
                out << rig.save_to_json();
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
                rig.load_from_json(json_text);
                status = std::string("Loaded ") + preset_path;
            }
            catch (const std::exception& error)
            {
                status = std::string("Load failed: ") + error.what();
            }
        }

        ImGui::SameLine();
        ImGui::TextColored(to_imvec4(palette().muted_text), "%s", device_label.c_str());

        ImGui::SameLine(ImGui::GetWindowWidth() - 96.0f);
        if (ImGui::Button("Settings"))
        {
            open_device_settings = true;
        }

        if (!status.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored(to_imvec4(palette().muted_text), "%s", status.c_str());
        }
    }
}
