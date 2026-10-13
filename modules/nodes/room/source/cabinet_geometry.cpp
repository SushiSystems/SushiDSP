/**************************************************************************/
/* cabinet_geometry.cpp                                                   */
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
 * @file cabinet_geometry.cpp
 * @brief Holds the table of shipped cabinets and the lookups over it.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/cabinet_geometry.hpp"

#include <array>
#include <cstring>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds the shipped cabinets, ordered as CabinetKind.
        constexpr std::array<CabinetPreset, K_CABINET_KIND_COUNT> K_PRESETS = {
            CabinetPreset{"1x12", "1x12", K_CABINET1X12},
            CabinetPreset{"2x12", "2x12", K_CABINET2X12},
            CabinetPreset{"4x12", "4x12", K_CABINET4X12}};
    } // namespace

    const CabinetPreset& cabinet_preset(CabinetKind kind) noexcept
    {
        const auto index = static_cast<std::size_t>(kind);
        return K_PRESETS[index < K_CABINET_KIND_COUNT ? index : K_CABINET_KIND_COUNT - 1u];
    }

    bool find_cabinet(const char* key, CabinetKind& kind) noexcept
    {
        for (std::size_t index = 0; index < K_CABINET_KIND_COUNT; ++index)
        {
            if (std::strcmp(K_PRESETS[index].key, key) == 0)
            {
                kind = static_cast<CabinetKind>(index);
                return true;
            }
        }
        return false;
    }

    bool cabinet_kind_of(const CabinetGeometry& geometry, CabinetKind& kind) noexcept
    {
        for (std::size_t index = 0; index < K_CABINET_KIND_COUNT; ++index)
        {
            if (same_geometry(K_PRESETS[index].geometry, geometry))
            {
                kind = static_cast<CabinetKind>(index);
                return true;
            }
        }
        return false;
    }

    bool same_geometry(const CabinetGeometry& left, const CabinetGeometry& right) noexcept
    {
        return left.cone_radius_m == right.cone_radius_m &&
               left.driver_spacing_m == right.driver_spacing_m &&
               left.baffle_width_m == right.baffle_width_m &&
               left.breakup_frequency_hz == right.breakup_frequency_hz &&
               left.driver_columns == right.driver_columns &&
               left.driver_rows == right.driver_rows && left.height_m == right.height_m &&
               left.depth_m == right.depth_m;
    }
} // namespace SushiDSP::room
