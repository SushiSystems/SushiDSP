/**************************************************************************/
/* Geometry.hpp                                                           */
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
 * @file Geometry.hpp
 * @brief Declares the slot id, point and color types the UI layer shares.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>

namespace ui
{
    /// @brief A rack slot's stable identity. It lives here, in the layer that owns no ImGui and no
    /// rig, so a unit that only does arithmetic on slot ids never has to reach up to the
    /// controller for the type. RigController::SlotId is an alias of this.
    using SlotId = std::uint32_t;

    /// @brief A point or a size, in panel-local units.
    struct Vec2
    {
        float x;
        float y;
    };

    /// @brief A color packed as 0xRRGGBBAA.
    using Color = std::uint32_t;

    /// @brief Packs four 8-bit channels into a Color.
    /// @param r Red.
    /// @param g Green.
    /// @param b Blue.
    /// @param a Alpha; opaque by default.
    /// @return The packed color.
    constexpr Color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
    {
        return (static_cast<Color>(r) << 24) | (static_cast<Color>(g) << 16)
             | (static_cast<Color>(b) << 8) | static_cast<Color>(a);
    }
}
