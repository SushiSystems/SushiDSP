/****************************************************************/
/* GearBrowser.hpp                                              */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file GearBrowser.hpp
 * @brief Declares the gear browser and the state it keeps between frames.
 * @author Mustafa Garip
 */

#pragma once

#include "ui/GearFilter.hpp"

namespace ui
{
    /// @brief The gear browser's UI state: the caller owns and persists it across frames.
    struct GearBrowserState
    {
        GearTab tab = GearTab::All;
        GearViewMode mode = GearViewMode::List;
        char search[128] = "";
    };

    /// @brief Draws the gear browser's header, search box, tabs and cards into the current
    /// window. Each card is a drag source carrying the GEAR_DESCRIPTOR_ID payload the chain strip
    /// accepts.
    /// @param state The browser's tab, view mode and search text; updated in place.
    void DrawGearBrowser(GearBrowserState& state);
}
