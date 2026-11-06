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
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file top_bar.hpp
 * @brief Declares the top bar's preset controls, brand mark and device group.
 * @author Mustafa Garip
 */

#pragma once

#include <string>

#include "session.hpp"

namespace ui
{
    /// @brief Draws the top bar: the rig's preset path with Save and Load, the brand mark and
    /// the device group. The preset is the selected track's rig, alone.
    /// @param session The session whose selected rig is saved and loaded; null draws the
    /// disconnected left group.
    /// @param status Out: describes the last Save or Load, empty when neither has run.
    /// @param rig_replaced Out: true on the frame a Load succeeded; drop every held SlotId then.
    /// @see applications/host_gui/ui/README.md
    void DrawTopBar(Session* session, bool connected, const std::string& device_label,
                    bool& open_device_settings, std::string& status, bool& rig_replaced);
} // namespace ui
