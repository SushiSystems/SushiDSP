/**************************************************************************/
/* mixer_panel.hpp                                                        */
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
 * @file mixer_panel.hpp
 * @brief Declares the mixer panel: one vertical strip per microphone, bus and master.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <optional>

#include "cabinet_section_wiring.hpp"
#include "rig_controller.hpp"
#include "rig_shape.hpp"
#include "ui/geometry.hpp"

namespace ui
{
    /// @brief Says what an edit on the mixer asks of whoever owns the rig's wiring. Levels, pans,
    /// mutes and solos are written through the rig as they change and need nothing more.
    struct MixerEdit
    {
            bool rewire = false; ///< True when a microphone's room flag changed, which moves its strip.
            std::optional<SlotId> remove_microphone; ///< The microphone whose remove button was pressed.
    };

    /// @brief Draws the mixer: one vertical strip per microphone, then Room, Listener, Cabinet or
    /// Line, Bus, DI and Master, each with the controls its slot has. The panel does not rewire and does
    /// not publish.
    /// @param id The ImGui id of the panel.
    /// @param rig The rig that owns the slots.
    /// @param slots The hidden slots to show; a strip whose slot is not built is not drawn.
    /// @param join What the chain's join carries, which names its strip.
    /// @return What the edit asks of the caller.
    MixerEdit draw_mixer_panel(const char* id, RigController& rig, const HiddenSlots& slots,
                               JoinContent join);
} // namespace ui
