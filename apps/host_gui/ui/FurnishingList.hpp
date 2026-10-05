/****************************************************************/
/* FurnishingList.hpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file FurnishingList.hpp
 * @brief Pure edits of a room scene's furnishing list, kept apart from the panel that draws it.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>

#include "SushiDSP/room/Furniture.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace ui
{
    /// @brief Appends a quantity of one furniture row to the scene's furnishings.
    /// @param scene The scene to edit.
    /// @param item The furniture row to add.
    /// @param quantity The number of items; must be finite and above zero.
    /// @return True when the entry was appended; false, with the scene unchanged, when the list
    /// is full or the quantity is refused.
    bool add_furnishing(SushiDSP::room::RoomScene& scene, SushiDSP::room::FurnitureId item,
                        double quantity) noexcept;

    /// @brief Deletes one furnishing and shifts the later entries down; ignores an index at or
    /// past the live count.
    /// @param scene The scene to edit.
    /// @param index The entry to delete.
    void remove_furnishing(SushiDSP::room::RoomScene& scene, std::uint32_t index) noexcept;

    /// @brief Replaces the quantity of one furnishing.
    /// @param scene The scene to edit.
    /// @param index The entry to change.
    /// @param quantity The new quantity; must be finite and above zero.
    /// @return True when the quantity was stored; false, with the scene unchanged, when the index
    /// is past the live count or the quantity is refused.
    bool set_furnishing_quantity(SushiDSP::room::RoomScene& scene, std::uint32_t index,
                                 double quantity) noexcept;

    /// @brief Returns the quantity the add button appends for a furniture row.
    /// @param item The furniture row.
    /// @return 1.0, one item.
    double default_furnishing_quantity(SushiDSP::room::FurnitureId item) noexcept;
}
