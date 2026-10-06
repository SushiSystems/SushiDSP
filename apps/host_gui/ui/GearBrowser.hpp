/**************************************************************************/
/* GearBrowser.hpp                                                        */
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
