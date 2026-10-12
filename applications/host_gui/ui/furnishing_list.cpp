/**************************************************************************/
/* furnishing_list.cpp                                                    */
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
 * @file furnishing_list.cpp
 * @brief Implements the edits of a room scene's furnishing list.
 * @author Mustafa Garip
 */

#include "ui/furnishing_list.hpp"

#include <cmath>

namespace ui
{
    namespace
    {
        using SushiDSP::room::FurnitureId;
        using SushiDSP::room::RoomScene;

        /// @brief Tells whether a quantity is one a furnishing may hold.
        bool is_valid_quantity(double quantity) noexcept
        {
            return std::isfinite(quantity) && quantity > 0.0;
        }
    } // namespace

    bool add_furnishing(RoomScene& scene, FurnitureId item, double quantity) noexcept
    {
        if (scene.furnishing_count >= RoomScene::K_MAX_FURNISHINGS || !is_valid_quantity(quantity))
        {
            return false;
        }
        scene.furnishings[scene.furnishing_count] = RoomScene::Furnishing{item, quantity};
        ++scene.furnishing_count;
        return true;
    }

    void remove_furnishing(RoomScene& scene, std::uint32_t index) noexcept
    {
        if (index >= scene.furnishing_count)
        {
            return;
        }
        for (std::uint32_t entry = index + 1; entry < scene.furnishing_count; ++entry)
        {
            scene.furnishings[entry - 1] = scene.furnishings[entry];
        }
        --scene.furnishing_count;
        scene.furnishings[scene.furnishing_count] = RoomScene::Furnishing{};
    }

    bool set_furnishing_quantity(RoomScene& scene, std::uint32_t index, double quantity) noexcept
    {
        if (index >= scene.furnishing_count || !is_valid_quantity(quantity))
        {
            return false;
        }
        scene.furnishings[index].quantity = quantity;
        return true;
    }

    double default_furnishing_quantity(FurnitureId) noexcept
    {
        return 1.0;
    }
} // namespace ui
