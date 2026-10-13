/**************************************************************************/
/* top_bar.hpp                                                            */
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
 * @file top_bar.hpp
 * @brief Declares the top bar's preset controls, brand mark and device group.
 * @author Mustafa Garip
 */

#pragma once

#include <string>

#include "session.hpp"
#include "session_bounce.hpp"

namespace ui
{
    /// @brief Draws the top bar: the session's Save and Load, the Export list, the brand mark
    /// and the device group; applications/host_gui/ui/README.md describes each parameter.
    /// @param session The session; null draws the disconnected left group.
    /// @param status Out: describes the last Save, Load or Export.
    /// @param rig_replaced Out: true on the frame a Load succeeded; drop every held SlotId then.
    /// @param bounce The export; started here and polled every frame.
    /// @param hrtf_path The HRTF set an export's rooms read; empty for none.
    void DrawTopBar(Session* session, bool connected, const std::string& device_label,
                    bool& open_device_settings, std::string& status, bool& rig_replaced,
                    SessionBounce& bounce, const std::string& hrtf_path);
} // namespace ui
