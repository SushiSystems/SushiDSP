/****************************************************************/
/* Materials.hpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
    inline constexpr std::size_t kMaterialCount = 15;

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
        std::array<float, kBandCount> absorption;

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
}
