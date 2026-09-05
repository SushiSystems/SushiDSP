/**************************************************************************/
/* GearBrowser.cpp                                                        */
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

#include "ui/GearBrowser.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#include <imgui.h>

#include "SushiDSP/node_registry/NodeRegistry.hpp"

#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Reports whether needle occurs in haystack, ignoring case.
        bool contains_ignoring_case(const char* haystack, const std::string& needle)
        {
            if (needle.empty())
            {
                return true;
            }
            std::string lowered(haystack);
            std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::string wanted = needle;
            std::transform(wanted.begin(), wanted.end(), wanted.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lowered.find(wanted) != std::string::npos;
        }

        /// @brief Lists every category the registry names, once each, in registry order.
        std::vector<const char*> registry_categories()
        {
            std::vector<const char*> categories;
            for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
            {
                const bool seen = std::any_of(categories.begin(), categories.end(),
                    [&entry](const char* name)
                    { return std::strcmp(name, entry.category.name) == 0; });
                if (!seen)
                {
                    categories.push_back(entry.category.name);
                }
            }
            return categories;
        }

        /// @brief Draws a card's icon: a filled rounded rectangle tinted by category.
        void draw_card_icon(ImVec2 top_left, float size, const char* category)
        {
            const ImU32 tint = std::strcmp(category, "Amp") == 0
                ? to_imu32(rgba(158, 124, 58))
                : std::strcmp(category, "Distortion") == 0
                    ? to_imu32(rgba(46, 112, 58))
                    : to_imu32(rgba(70, 84, 120));
            ImGui::GetWindowDrawList()->AddRectFilled(
                top_left, ImVec2(top_left.x + size, top_left.y + size), tint, 6.0f);
        }

        /// @brief Draws one product's card and makes it a drag source for the rack.
        void draw_gear_card(const SushiDSP::NodeEntry& entry)
        {
            ImGui::PushID(entry.descriptor->id);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("card", ImVec2(ImGui::GetContentRegionAvail().x, 44.0f));

            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin,
                                ImVec2(origin.x + ImGui::GetItemRectSize().x, origin.y + 44.0f),
                                to_imu32(ImGui::IsItemHovered() ? rgba(48, 48, 54) : rgba(34, 34, 39)),
                                5.0f);
            draw_card_icon(ImVec2(origin.x + 6.0f, origin.y + 6.0f), 32.0f, entry.category.name);
            draw->AddText(ImVec2(origin.x + 48.0f, origin.y + 15.0f),
                          to_imu32(palette().text), entry.descriptor->name);

            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload("GEAR_DESCRIPTOR_ID", entry.descriptor->id,
                                          std::strlen(entry.descriptor->id) + 1);
                ImGui::Text("%s", entry.descriptor->name);
                ImGui::EndDragDropSource();
            }
            ImGui::PopID();
        }
    }

    void DrawGearBrowser()
    {
        static char search[128] = "";
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##gear_search", "Search gear", search, sizeof(search));
        ImGui::Separator();

        const std::string needle(search);

        for (const char* category : registry_categories())
        {
            // One header per category, so a repeat further down the registry cannot collide ids.
            if (!ImGui::CollapsingHeader(category, ImGuiTreeNodeFlags_DefaultOpen))
            {
                continue;
            }

            for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
            {
                if (std::strcmp(entry.category.name, category) != 0
                    || !contains_ignoring_case(entry.descriptor->name, needle))
                {
                    continue;
                }

                draw_gear_card(entry);
            }
        }
    }
}
