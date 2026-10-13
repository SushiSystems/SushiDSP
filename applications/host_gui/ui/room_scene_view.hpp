/**************************************************************************/
/* room_scene_view.hpp                                                    */
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
 * @file room_scene_view.hpp
 * @brief Declares the perspective view of the room and the things placed in it.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <optional>
#include <vector>

#include <imgui.h>

#include "SushiDSP/room/room_arrangement.hpp"

#include "ui/scene_camera.hpp"
#include "ui/scene_interaction.hpp"

namespace ui
{
    /// @brief Holds what the room scene keeps between frames: its camera, the selected item and
    /// the drag in progress.
    struct RoomSceneState
    {
            SceneCamera camera;                ///< The orbit camera the room is drawn through.
            bool framed = false;               ///< True once the camera has been aimed at the room.
            std::optional<SceneItem> selected; ///< The selected item, by kind and identity.
            bool dragging_item = false;        ///< True while a drag moves the selected item.
    };

    /// @brief Lists everything an arrangement places, in the order the scene draws it: the
    /// cabinets, the microphones, then the listener.
    /// @param arrangement The room.
    /// @return The items.
    std::vector<SceneItem> scene_items(const SushiDSP::Room::Arrangement& arrangement);

    /// @brief Draws the room in perspective with its cabinets, microphones and listener, and moves
    /// them under the pointer; applications/host_gui/ui/README.md lists the drags.
    /// @param id The ImGui id of the view.
    /// @param arrangement In/out: the room; a moved item is written back into it.
    /// @param state In/out: the camera, the selection and the drag.
    /// @param size The view's size in pixels.
    /// @return True on the frames an item moved.
    bool draw_room_scene(const char* id, SushiDSP::Room::Arrangement& arrangement,
                         RoomSceneState& state, ImVec2 size);
} // namespace ui
