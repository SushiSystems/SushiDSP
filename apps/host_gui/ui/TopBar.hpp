/****************************************************************/
/* TopBar.hpp                                                   */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file TopBar.hpp
 * @brief Declares the top bar's preset controls, brand mark and device group.
 * @author Mustafa Garip
 */

#pragma once

#include <string>

#include "RigController.hpp"

namespace ui
{
    /// @brief Draws the top bar: the preset controls, the brand mark and the device group.
    /// @param rig The rig to save from and load into; null draws the disconnected left group.
    /// @param connected Whether an audio device is connected.
    /// @param device_label Names the connected device; read only when connected is true.
    /// @param open_device_settings Out: set to true on the frame the settings button is clicked.
    /// @param status Out: describes the last Save or Load, empty when neither has run.
    /// @param rig_replaced Out: true on the frame a Load succeeded; drop every held SlotId then.
    /// @see apps/host_gui/ui/README.md
    void DrawTopBar(RigController* rig, bool connected, const std::string& device_label,
                    bool& open_device_settings, std::string& status, bool& rig_replaced);
}
