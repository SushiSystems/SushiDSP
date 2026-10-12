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

#include "rig_controller.hpp"

namespace ui
{
    /// @brief Draws the top bar: the preset controls, the brand mark and the device group.
    /// @param rig The rig to save from and load into; null draws the disconnected left group.
    /// @param connected Whether an audio device is connected.
    /// @param device_label Names the connected device; read only when connected is true.
    /// @param open_device_settings Out: set to true on the frame the settings button is clicked.
    /// @param status Out: describes the last Save or Load, empty when neither has run.
    /// @param rig_replaced Out: true on the frame a Load succeeded; drop every held SlotId then.
    /// @see applications/host_gui/ui/README.md
    void DrawTopBar(RigController* rig, bool connected, const std::string& device_label,
                    bool& open_device_settings, std::string& status, bool& rig_replaced);
} // namespace ui
