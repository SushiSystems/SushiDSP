/**************************************************************************/
/* PatchList.cpp                                                          */
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
 * @file PatchList.cpp
 * @brief Implements the pure patch-list edits the room panel calls.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/PatchList.hpp"

#include <cmath>

namespace ui
{
    namespace
    {
        using SushiDSP::room::MaterialId;
        using SushiDSP::room::RoomScene;
        using SushiDSP::room::Surface;

        /// @brief Holds the share of a face the add button covers.
        constexpr double kDefaultFaceShare = 0.1;

        /// @brief Tells whether an area is one a patch may hold.
        bool is_valid_area(double area_m2) noexcept
        {
            return std::isfinite(area_m2) && area_m2 > 0.0;
        }
    }

    bool add_patch(RoomScene& scene, Surface surface, MaterialId material, double area_m2) noexcept
    {
        if (scene.patch_count >= RoomScene::kMaxPatches || !is_valid_area(area_m2))
        {
            return false;
        }
        scene.patches[scene.patch_count] = RoomScene::Patch{ surface, material, area_m2 };
        ++scene.patch_count;
        return true;
    }

    void remove_patch(RoomScene& scene, std::uint32_t index) noexcept
    {
        if (index >= scene.patch_count)
        {
            return;
        }
        for (std::uint32_t entry = index + 1; entry < scene.patch_count; ++entry)
        {
            scene.patches[entry - 1] = scene.patches[entry];
        }
        --scene.patch_count;
        scene.patches[scene.patch_count] = RoomScene::Patch{};
    }

    bool set_patch_area(RoomScene& scene, std::uint32_t index, double area_m2) noexcept
    {
        if (index >= scene.patch_count || !is_valid_area(area_m2))
        {
            return false;
        }
        scene.patches[index].area_m2 = area_m2;
        return true;
    }

    double default_patch_area(Surface surface) noexcept
    {
        return kDefaultFaceShare * RoomScene{}.face_area_m2(surface);
    }
}
