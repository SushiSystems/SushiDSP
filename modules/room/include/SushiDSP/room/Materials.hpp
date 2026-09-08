/**************************************************************************/
/* Materials.hpp                                                          */
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
 * @file Materials.hpp
 * @brief The absorption table of the room module and the reflection factor each row yields.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "SushiDSP/room/BandResponse.hpp"

namespace SushiDSP::room
{
    /// @brief Names each surface the shipped absorption table carries. Spec §4.2.
    enum class MaterialId : std::uint8_t
    {
        ConcreteUnpainted = 0,
        ConcreteBlockPainted,
        GypsumBoardOnStuds,
        HeavyCarpetOnConcrete,
        GlassWindow,
        OpenDoorway
    };

    /// @brief Counts the rows of the shipped absorption table.
    inline constexpr std::size_t kMaterialCount = 6;

    /// @brief Names how much of a reflection leaves the surface diffusely. Spec §4.2.
    enum class Scattering : std::uint8_t
    {
        Flat = 0,
        Textured,
        Cluttered
    };

    /// @brief Holds one transcribed row of the absorption table.
    struct Material
    {
        /// @brief Names the surface as the cited table names it.
        const char* name;

        /// @brief Stores the random-incidence absorption coefficient per octave band.
        std::array<float, kBandCount> absorption;

        /// @brief Names the source and the page the row was transcribed from.
        const char* citation;
    };

    /// @brief Returns the transcribed row of the given material.
    const Material& material(MaterialId id) noexcept;

    /// @brief Returns the pressure reflection factor sqrt(1 - alpha) per band, phase taken as
    /// zero.
    BandResponse reflection_factor(MaterialId id) noexcept;

    /// @brief Returns the fraction of reflected energy the given scattering word hands to the
    /// diffuse tail.
    float scattering_coefficient(Scattering scattering) noexcept;
}
