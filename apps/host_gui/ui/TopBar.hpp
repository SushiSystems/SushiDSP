/**************************************************************************/
/* TopBar.hpp                                                             */
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

#pragma once

#include <string>

#include "RigController.hpp"

namespace ui
{
    /// @brief Draws the three-column top bar into the current window: the preset path box, Save
    /// and Load on the left; the "SushiDSP" brand mark centered on the window regardless of the
    /// side groups' widths; and, on the right, the connection pill (or, with no device, the
    /// "No device connected." label) plus the settings button.
    /// @param rig The rig to save from and load into; null draws the disconnected left group.
    /// @param connected Whether an audio device is connected, selecting the right group's pill
    /// or its absence.
    /// @param device_label Text naming the connected device and its channel selection; read only
    /// when connected is true.
    /// @param open_device_settings Out: set to true on the frame the settings button is clicked.
    /// @param status Out: a message describing the last Save or Load, empty when neither has run.
    /// @param rig_replaced Out: set to true on the frame a Load succeeded. A load rebuilds the
    /// slot id space from zero, so ids that survive the load name different gear; every caller
    /// holding a SlotId must drop it on the frame this reads true.
    void DrawTopBar(RigController* rig, bool connected, const std::string& device_label,
                    bool& open_device_settings, std::string& status, bool& rig_replaced);
}
