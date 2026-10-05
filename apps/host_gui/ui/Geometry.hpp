/****************************************************************/
/* Geometry.hpp                                                 */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
