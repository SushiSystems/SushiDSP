/****************************************************************/
/* GearFilter.cpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "ui/GearFilter.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace ui
{
    namespace
    {
        /// @brief The routing slot ids §16 keeps off every tab, regardless of category.
        /// @param id The item's descriptor id.
        /// @return True for the hidden splitter and mixer slots.
        bool is_hidden_routing_slot(const std::string& id)
        {
            return id == "sushidsp.splitter" || id == "sushidsp.mixer";
        }

        /// @brief Lowercases a string for a case-insensitive comparison.
        /// @param text The string to fold.
        /// @return The lowercased copy.
        std::string to_lower(const std::string& text)
        {
            std::string folded = text;
            std::transform(folded.begin(), folded.end(), folded.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return folded;
        }
    }

    std::optional<GearTab> tab_of(const std::string& category)
    {
        if (category == "Amp")
        {
            return GearTab::Amps;
        }
        if (category == "Cabinet")
        {
            return GearTab::Cabs;
        }
        if (category == "Routing")
        {
            return std::nullopt;
        }
        return GearTab::Pedals;
    }

    std::vector<GearGroup> filter_gear(const std::vector<GearItem>& items, GearTab tab,
                                       const std::string& search)
    {
        const std::string needle = to_lower(search);
        std::vector<GearGroup> groups;
        std::unordered_map<std::string, std::size_t> group_index;

        for (const GearItem& item : items)
        {
            if (is_hidden_routing_slot(item.id))
            {
                continue;
            }
            const std::optional<GearTab> item_tab = tab_of(item.category);
            if (!item_tab.has_value())
            {
                continue;
            }
            if (tab != GearTab::All && tab != item_tab.value())
            {
                continue;
            }
            if (!needle.empty() && to_lower(item.name).find(needle) == std::string::npos)
            {
                continue;
            }

            const auto found = group_index.find(item.category);
            if (found == group_index.end())
            {
                group_index.emplace(item.category, groups.size());
                groups.push_back({ item.category, { &item } });
            }
            else
            {
                groups[found->second].items.push_back(&item);
            }
        }

        return groups;
    }
}
