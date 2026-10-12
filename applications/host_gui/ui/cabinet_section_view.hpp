/**************************************************************************/
/* cabinet_section_view.hpp                                               */
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
 * @file cabinet_section_view.hpp
 * @brief Declares the cabinet section's plan view: a top-down drawing of the room with the
 * listener, the placed cabinets and the microphones as draggable markers. A plan, not a
 * perspective 3D scene.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "room_layout.hpp"

namespace ui
{
    /// @brief Draws the room's plan with its markers, the add and remove microphone buttons and
    /// a yaw slider per marker, and writes every edit into the layout.
    /// @param id The ImGui id of the view.
    /// @param layout The layout to draw and edit.
    /// @param height The plan's height in pixels.
    /// @return True when the layout changed.
    bool draw_cabinet_section_view(const char* id, RoomLayout& layout, float height);
} // namespace ui
