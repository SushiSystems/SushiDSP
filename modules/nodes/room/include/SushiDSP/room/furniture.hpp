/**************************************************************************/
/* furniture.hpp                                                          */
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
 * @file furniture.hpp
 * @brief The equivalent absorption area table of the room module's furnishings.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "SushiDSP/room/band_response.hpp"

namespace SushiDSP::room
{
    /// @brief Names each furnishing the shipped furniture table carries.
    enum class FurnitureId : std::uint8_t
    {
        SofaTwoSeat = 0,
        BedDouble,
        Desk,
        WardrobeModule,
        ChairUpholstered,
        BookcaseFilled,
        Person
    };

    /// @brief Counts the rows of the shipped furniture table.
    inline constexpr std::size_t K_FURNITURE_COUNT = 7;

    /// @brief Holds one transcribed row of the furniture table.
    struct FurnitureItem
    {
            /// @brief Names the furnishing as its source names it.
            const char* name;

            /// @brief Names the furnishing as a preset file spells it.
            const char* key;

            /// @brief Stores the absorption area in square metres per item and octave band.
            std::array<float, K_BAND_COUNT> absorption_area;

            /// @brief Names the source the row was transcribed from.
            const char* citation;

            /// @brief Stores the item's nominal width, depth and height, in metres: the box it is
            /// drawn as and its absorption is spread around.
            std::array<float, 3> box_m;
    };

    /// @brief Returns the transcribed row of the given furnishing.
    const FurnitureItem& furniture(FurnitureId id) noexcept;

    /// @brief Finds the row whose key equals the given text exactly.
    /// @param key A null-terminated preset word.
    /// @param id Written only when a row carries the key.
    /// @return true when a row carries the key.
    bool find_furniture(const char* key, FurnitureId& id) noexcept;
} // namespace SushiDSP::room
