/**************************************************************************/
/* PatchList.hpp                                                          */
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

/**
 * @file PatchList.hpp
 * @brief Pure edits of a room scene's patch list, kept apart from the panel that draws it.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>

#include "SushiDSP/room/Materials.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace ui
{
    /// @brief Appends one patch to the scene's patches.
    /// @param scene The scene to edit.
    /// @param surface The face the patch sits on.
    /// @param material The patch's material.
    /// @param area_m2 The patch's area in square metres; must be finite and above zero. It is
    /// not compared with the face, which RoomScene::is_valid does.
    /// @return True when the patch was appended; false, with the scene unchanged, when the list
    /// is full or the area is refused.
    bool add_patch(SushiDSP::room::RoomScene& scene, SushiDSP::room::Surface surface,
                   SushiDSP::room::MaterialId material, double area_m2) noexcept;

    /// @brief Deletes one patch and shifts the later entries down; ignores an index at or past
    /// the live count.
    /// @param scene The scene to edit.
    /// @param index The entry to delete.
    void remove_patch(SushiDSP::room::RoomScene& scene, std::uint32_t index) noexcept;

    /// @brief Replaces the area of one patch.
    /// @param scene The scene to edit.
    /// @param index The entry to change.
    /// @param area_m2 The new area in square metres; must be finite and above zero.
    /// @return True when the area was stored; false, with the scene unchanged, when the index
    /// is past the live count or the area is refused.
    bool set_patch_area(SushiDSP::room::RoomScene& scene, std::uint32_t index,
                        double area_m2) noexcept;

    /// @brief Returns the area the add button appends for a face.
    /// @param surface The face.
    /// @return A tenth of that face's area in the default scene, in square metres.
    double default_patch_area(SushiDSP::room::Surface surface) noexcept;
}
