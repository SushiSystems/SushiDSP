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
    /// @brief Draws the preset path box, Save, Load, the connected-device label and the settings
    /// button into the current window.
    /// @param rig The rig to save from and load into.
    /// @param device_label Text naming the connected device and its channel selection.
    /// @param open_device_settings Out: set to true on the frame the settings button is clicked.
    /// @param status Out: a message describing the last Save or Load, empty when neither has run.
    void DrawTopBar(RigController& rig, const std::string& device_label,
                    bool& open_device_settings, std::string& status);
}
