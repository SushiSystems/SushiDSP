/**************************************************************************/
/* cabinet_geometry.hpp                                                   */
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
 * @file cabinet_geometry.hpp
 * @brief Declares a cabinet's box, its cones on the baffle and the three shipped cabinets.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace SushiDSP::room
{
    /// @brief Holds a cabinet's box and its cones. The cones stand on a grid of one or two columns
    /// by one or two rows, centred on the baffle, driver_spacing_m apart. Spec §7.
    struct CabinetGeometry
    {
            /// @brief Bounds the cones one cabinet carries.
            static constexpr std::uint32_t K_MAX_DRIVERS = 4;

            /// @brief Stores the effective cone radius of one driver, in metres.
            double cone_radius_m;

            /// @brief Stores the centre-to-centre driver spacing, in metres; zero for one driver.
            double driver_spacing_m;

            /// @brief Stores the baffle width, in metres.
            double baffle_width_m;

            /// @brief Stores the cone breakup frequency that shrinks the effective radius, in hertz.
            double breakup_frequency_hz;

            /// @brief Counts the cones side by side: 1 or 2.
            std::uint32_t driver_columns = 2;

            /// @brief Counts the cones one above the other: 1 or 2.
            std::uint32_t driver_rows = 2;

            /// @brief Stores the box's height, in metres.
            double height_m = 0.75;

            /// @brief Stores the box's depth behind the baffle, in metres.
            double depth_m = 0.36;

            /// @brief Returns the number of cones.
            constexpr std::uint32_t driver_count() const noexcept
            {
                return driver_columns * driver_rows;
            }

            /// @brief Returns how far one cone's centre stands to the left of the baffle's centre,
            /// in metres. Cones are numbered row by row from the lower right.
            /// @param driver The cone, below driver_count().
            constexpr double driver_left_m(std::uint32_t driver) const noexcept
            {
                return driver_columns < 2u
                           ? 0.0
                           : (static_cast<double>(driver % driver_columns) - 0.5) * driver_spacing_m;
            }

            /// @brief Returns how far one cone's centre stands above the baffle's centre, in metres.
            /// @param driver The cone, below driver_count().
            constexpr double driver_up_m(std::uint32_t driver) const noexcept
            {
                return driver_rows < 2u
                           ? 0.0
                           : (static_cast<double>(driver / driver_columns) - 0.5) * driver_spacing_m;
            }
    };

    /// @brief Describes a 4x12 cabinet: four cones in a square.
    inline constexpr CabinetGeometry K_CABINET4X12{0.125, 0.30, 0.75, 2500.0, 2, 2, 0.75, 0.36};

    /// @brief Describes a 2x12 cabinet: two cones side by side.
    inline constexpr CabinetGeometry K_CABINET2X12{0.125, 0.30, 0.75, 2500.0, 2, 1, 0.50, 0.30};

    /// @brief Describes a 1x12 cabinet: one cone.
    inline constexpr CabinetGeometry K_CABINET1X12{0.125, 0.00, 0.40, 2500.0, 1, 1, 0.45, 0.28};

    /// @brief Names each shipped cabinet.
    enum class CabinetKind : std::uint8_t
    {
        OneByTwelve = 0,
        TwoByTwelve,
        FourByTwelve
    };

    /// @brief Counts the shipped cabinets.
    inline constexpr std::size_t K_CABINET_KIND_COUNT = 3;

    /// @brief Holds one shipped cabinet: what a preset calls it, what a person calls it and its
    /// geometry.
    struct CabinetPreset
    {
            /// @brief Names the cabinet as a preset file spells it.
            const char* key;

            /// @brief Names the cabinet on screen.
            const char* name;

            /// @brief Stores the geometry.
            CabinetGeometry geometry;
    };

    /// @brief Returns one shipped cabinet.
    const CabinetPreset& cabinet_preset(CabinetKind kind) noexcept;

    /// @brief Finds the shipped cabinet whose key equals the given text exactly.
    /// @param key A null-terminated preset word.
    /// @param kind Written only when a cabinet carries the key.
    /// @return true when a cabinet carries the key.
    bool find_cabinet(const char* key, CabinetKind& kind) noexcept;

    /// @brief Finds the shipped cabinet a geometry equals, field for field.
    /// @param kind Written only when a cabinet matches.
    /// @return true when the geometry is a shipped one.
    bool cabinet_kind_of(const CabinetGeometry& geometry, CabinetKind& kind) noexcept;

    /// @brief Reports whether two geometries are equal, field for field.
    bool same_geometry(const CabinetGeometry& left, const CabinetGeometry& right) noexcept;
} // namespace SushiDSP::room
