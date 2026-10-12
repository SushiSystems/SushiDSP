/**************************************************************************/
/* gear_filter.hpp                                                        */
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
 * @file gear_filter.hpp
 * @brief Declares the gear browser's tabs, items and the filter that groups them.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ui
{
    /// @brief One tab of the gear browser.
    enum class GearTab : std::uint8_t
    {
        All,
        Amps,
        Pedals,
        Cabs
    };

    /// @brief How the gear browser lays out its items.
    enum class GearViewMode : std::uint8_t
    {
        List,
        Grid
    };

    /// @brief One item the gear browser can list, independent of the node registry it came from.
    struct GearItem
    {
            std::string id;
            std::string name;
            std::string category;
    };

    /// @brief Maps a node registry category to the tab that lists it.
    /// @param category The registry's `NodeCategory::name` ("Amp", "Cabinet", "Distortion",
    /// "Delay", "Routing", "Room").
    /// @return Amps for "Amp"; Cabs for "Cabinet"; Pedals for every other category except
    /// "Routing", which returns nullopt because §16 keeps routing nodes off every tab.
    std::optional<GearTab> tab_of(const std::string& category);

    /// @brief One category's matching items, in the order filter_gear first saw the category.
    struct GearGroup
    {
            std::string category;
            std::vector<const GearItem*> items;
    };

    /// @brief Groups the items a tab and a search term match, by category, first-seen order.
    /// @param items The catalog to filter; sushidsp.splitter and sushidsp.mixer are always
    /// excluded, matching tab_of's Routing exclusion.
    /// @param tab The tab to filter by; All keeps every non-routing item.
    /// @param search Case-insensitive substring match on name; empty matches everything.
    /// @return The matching items, grouped by category in first-seen order.
    std::vector<GearGroup> filter_gear(const std::vector<GearItem>& items, GearTab tab,
                                       const std::string& search);
} // namespace ui
