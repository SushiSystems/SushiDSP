/**************************************************************************/
/* gear_browser.cpp                                                       */
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
 * @file gear_browser.cpp
 * @brief Implements the gear browser's header, search box, tabs and cards.
 * @author Mustafa Garip
 */

#include "ui/gear_browser.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include <SushiDSP/catalog/node_registry/node_registry.hpp>

#include "ui/theme.hpp"

namespace ui
{
    namespace
    {
        constexpr float K_LIST_CARD_HEIGHT = 44.0f;
        constexpr float K_GRID_TILE_WIDTH = 76.0f;
        constexpr float K_GRID_TILE_HEIGHT = 76.0f;
        constexpr float K_GRID_SPACING = 8.0f;

        /// @brief The catalog the gear browser lists, built once from the node registry.
        /// @return The catalog; stable for the process lifetime, matching node_registry's own
        /// guarantee that it never changes at runtime.
        const std::vector<GearItem>& gear_catalog()
        {
            static const std::vector<GearItem> catalog = []
            {
                std::vector<GearItem> items;
                items.reserve(SushiDSP::node_registry().size());
                for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
                {
                    items.push_back(GearItem{entry.descriptor->id, entry.descriptor->name,
                                             entry.category.name});
                }
                return items;
            }();
            return catalog;
        }

        /// @brief Draws a card's icon: a filled rounded rectangle tinted by category.
        void draw_card_icon(ImVec2 top_left, float size, const std::string& category)
        {
            const ImU32 tint = category == "Amp"          ? to_imu32(rgba(158, 124, 58))
                               : category == "Distortion" ? to_imu32(rgba(46, 112, 58))
                                                          : to_imu32(rgba(70, 84, 120));
            ImGui::GetWindowDrawList()->AddRectFilled(
                top_left, ImVec2(top_left.x + size, top_left.y + size), tint, 6.0f);
        }

        /// @brief Makes the item just drawn a drag source carrying its descriptor id.
        void begin_gear_drag_source(const GearItem& item)
        {
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload("GEAR_DESCRIPTOR_ID", item.id.c_str(),
                                          item.id.size() + 1);
                ImGui::Text("%s", item.name.c_str());
                ImGui::EndDragDropSource();
            }
        }

        /// @brief Draws one item as today's full-width list card: icon, name, drag source.
        void draw_list_card(const GearItem& item)
        {
            ImGui::PushID(item.id.c_str());
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("card",
                                   ImVec2(ImGui::GetContentRegionAvail().x, K_LIST_CARD_HEIGHT));

            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(
                origin,
                ImVec2(origin.x + ImGui::GetItemRectSize().x, origin.y + K_LIST_CARD_HEIGHT),
                to_imu32(ImGui::IsItemHovered() ? rgba(48, 48, 54) : rgba(34, 34, 39)), 5.0f);
            draw_card_icon(ImVec2(origin.x + 6.0f, origin.y + 6.0f), 32.0f, item.category);
            draw->AddText(ImVec2(origin.x + 48.0f, origin.y + 15.0f), to_imu32(palette().text),
                          item.name.c_str());

            begin_gear_drag_source(item);
            ImGui::PopID();
        }

        /// @brief Draws one item as a grid tile: icon above name, no subtitle.
        void draw_grid_tile(const GearItem& item)
        {
            ImGui::PushID(item.id.c_str());
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("tile", ImVec2(K_GRID_TILE_WIDTH, K_GRID_TILE_HEIGHT));

            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(
                origin, ImVec2(origin.x + K_GRID_TILE_WIDTH, origin.y + K_GRID_TILE_HEIGHT),
                to_imu32(ImGui::IsItemHovered() ? rgba(48, 48, 54) : rgba(34, 34, 39)), 5.0f);

            const float icon_size = 32.0f;
            const float icon_x = origin.x + (K_GRID_TILE_WIDTH - icon_size) * 0.5f;
            draw_card_icon(ImVec2(icon_x, origin.y + 8.0f), icon_size, item.category);

            const ImVec2 text_size =
                ImGui::CalcTextSize(item.name.c_str(), nullptr, false, K_GRID_TILE_WIDTH - 8.0f);
            const float text_x = origin.x + (K_GRID_TILE_WIDTH - text_size.x) * 0.5f;
            draw->AddText(nullptr, 0.0f, ImVec2(text_x, origin.y + icon_size + 14.0f),
                          to_imu32(palette().text), item.name.c_str(), nullptr,
                          K_GRID_TILE_WIDTH - 8.0f);

            begin_gear_drag_source(item);
            ImGui::PopID();
        }

        /// @brief Draws the header row: title, item count and the list/grid view toggle.
        void draw_header_row(std::size_t item_count, GearViewMode& mode)
        {
            ImGui::TextUnformatted("Gear browser");
            ImGui::SameLine();
            ImGui::TextColored(to_imvec4(palette().muted_text), "%zu items", item_count);

            // Each toggle is as wide as its caption plus padding; see applications/host_gui/ui/README.md.
            constexpr float K_TOGGLE_PADDING = 10.0f;
            constexpr float K_TOGGLE_GAP = 2.0f;
            const float list_width = ImGui::CalcTextSize("List").x + K_TOGGLE_PADDING;
            const float grid_width = ImGui::CalcTextSize("Grid").x + K_TOGGLE_PADDING;
            const float toggle_width = list_width + K_TOGGLE_GAP + grid_width;

            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() -
                            toggle_width);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
            if (ImGui::Selectable("List", mode == GearViewMode::List, 0, ImVec2(list_width, 20.0f)))
            {
                mode = GearViewMode::List;
            }
            ImGui::SameLine(0.0f, K_TOGGLE_GAP);
            if (ImGui::Selectable("Grid", mode == GearViewMode::Grid, 0, ImVec2(grid_width, 20.0f)))
            {
                mode = GearViewMode::Grid;
            }
            ImGui::PopStyleVar();
        }

        /// @brief Draws the four tabs as equal-width selectable buttons.
        void draw_tab_row(GearTab& tab)
        {
            static constexpr std::pair<GearTab, const char*> K_TABS[] = {
                {GearTab::All, "All"},
                {GearTab::Amps, "Amps"},
                {GearTab::Pedals, "Pedals"},
                {GearTab::Cabs, "Cabs"},
            };

            const float spacing = 6.0f;
            const float width = (ImGui::GetContentRegionAvail().x - spacing * 3.0f) / 4.0f;
            // The caption is centered in its box; applications/host_gui/ui/README.md says why.
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
            for (std::size_t i = 0; i < 4; ++i)
            {
                if (i != 0)
                {
                    ImGui::SameLine(0.0f, spacing);
                }
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(K_TABS[i].second, tab == K_TABS[i].first, 0,
                                      ImVec2(width, 24.0f)))
                {
                    tab = K_TABS[i].first;
                }
                ImGui::PopID();
            }
            ImGui::PopStyleVar();
        }

        /// @brief Draws one category's items in list mode: a label, then a full-width card each.
        void draw_group_list(const GearGroup& group)
        {
            ImGui::TextColored(to_imvec4(palette().muted_text), "%s", group.category.c_str());
            for (const GearItem* item : group.items)
            {
                draw_list_card(*item);
            }
        }

        /// @brief Draws one category's items in grid mode: a label, then wrapping tiles.
        void draw_group_grid(const GearGroup& group)
        {
            ImGui::TextColored(to_imvec4(palette().muted_text), "%s", group.category.c_str());

            const float avail_width = ImGui::GetContentRegionAvail().x;
            const int columns = std::max(1, static_cast<int>((avail_width + K_GRID_SPACING) /
                                                             (K_GRID_TILE_WIDTH + K_GRID_SPACING)));

            int column = 0;
            for (const GearItem* item : group.items)
            {
                if (column != 0)
                {
                    ImGui::SameLine(0.0f, K_GRID_SPACING);
                }
                draw_grid_tile(*item);
                column = (column + 1) % columns;
                if (column == 0)
                {
                    ImGui::Dummy(ImVec2(0.0f, K_GRID_SPACING));
                }
            }
            if (column != 0)
            {
                ImGui::Dummy(ImVec2(0.0f, K_GRID_SPACING));
            }
        }
    } // namespace

    void DrawGearBrowser(GearBrowserState& state)
    {
        const std::vector<GearItem>& catalog = gear_catalog();

        draw_header_row(catalog.size(), state.mode);
        ImGui::Separator();

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##gear_search", "Search gear", state.search,
                                 sizeof(state.search));

        draw_tab_row(state.tab);
        ImGui::Separator();

        const std::vector<GearGroup> groups = filter_gear(catalog, state.tab, state.search);

        ImGui::BeginChild("##gear_body", ImVec2(0.0f, 0.0f), false);
        for (const GearGroup& group : groups)
        {
            if (state.mode == GearViewMode::List)
            {
                draw_group_list(group);
            }
            else
            {
                draw_group_grid(group);
            }
        }
        ImGui::EndChild();
    }
} // namespace ui
