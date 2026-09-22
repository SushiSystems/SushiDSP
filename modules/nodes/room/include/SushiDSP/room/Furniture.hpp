/**************************************************************************/
/* Furniture.hpp                                                          */
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
 * @file Furniture.hpp
 * @brief The equivalent absorption area table of the room module's furnishings.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "SushiDSP/room/BandResponse.hpp"

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
    inline constexpr std::size_t kFurnitureCount = 7;

    /// @brief Holds one transcribed row of the furniture table.
    struct FurnitureItem
    {
        /// @brief Names the furnishing as its source names it.
        const char* name;

        /// @brief Names the furnishing as a preset file spells it.
        const char* key;

        /// @brief Stores the absorption area in square metres per item and octave band.
        std::array<float, kBandCount> absorption_area;

        /// @brief Names the source the row was transcribed from.
        const char* citation;
    };

    /// @brief Returns the transcribed row of the given furnishing.
    const FurnitureItem& furniture(FurnitureId id) noexcept;

    /// @brief Finds the row whose key equals the given text exactly.
    /// @param key A null-terminated preset word.
    /// @param id Written only when a row carries the key.
    /// @return true when a row carries the key.
    bool find_furniture(const char* key, FurnitureId& id) noexcept;
}
