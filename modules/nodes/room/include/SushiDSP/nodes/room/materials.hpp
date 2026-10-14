/**************************************************************************/
/* materials.hpp                                                          */
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
 * @file materials.hpp
 * @brief The absorption table of the room module and the reflection factor each row yields.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <SushiDSP/nodes/room/band_response.hpp>

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
        OpenDoorway,
        PlasterOnMasonryPainted,
        CeramicTile,
        ParquetOnConcrete,
        WoodPanel,
        WindowDoubleGlazed,
        WoodenDoor,
        ThinCarpetOnConcrete,
        CurtainHeavyVelour,
        CurtainLight
    };

    /// @brief Counts the rows of the shipped absorption table.
    inline constexpr std::size_t K_MATERIAL_COUNT = 15;

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

            /// @brief Names the surface as a preset file spells it.
            const char* key;

            /// @brief Stores the random-incidence absorption coefficient per octave band.
            std::array<float, K_BAND_COUNT> absorption;

            /// @brief Names the source and the page the row was transcribed from.
            const char* citation;
    };

    /// @brief Returns the transcribed row of the given material.
    const Material& material(MaterialId id) noexcept;

    /// @brief Finds the row whose key equals the given text exactly.
    /// @param key A null-terminated preset word.
    /// @param id Written only when a row carries the key.
    /// @return true when a row carries the key.
    bool find_material(const char* key, MaterialId& id) noexcept;

    /// @brief Returns the pressure reflection factor sqrt(1 - alpha) per band, phase taken as
    /// zero.
    BandResponse reflection_factor(MaterialId id) noexcept;

    /// @brief Returns the fraction of reflected energy the given scattering word hands to the
    /// diffuse tail.
    float scattering_coefficient(Scattering scattering) noexcept;
} // namespace SushiDSP::room
