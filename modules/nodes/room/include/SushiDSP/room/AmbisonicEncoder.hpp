/**************************************************************************/
/* AmbisonicEncoder.hpp                                                   */
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
 * @file AmbisonicEncoder.hpp
 * @brief Third-order ACN/SN3D encode gains for one direction.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>

namespace SushiDSP::room
{
    /// @brief Produces third-order ACN/SN3D encode gains for one direction, the convention
    /// SADIE's published decoders use. Spec §5.1 and §9.1.
    class AmbisonicEncoder
    {
    public:
        /// @brief Names the Ambisonic order.
        static constexpr std::uint32_t kOrder = 3;

        /// @brief Counts the channels, (kOrder + 1)^2.
        static constexpr std::uint32_t kChannels = 16;

        /// @brief Fills gains with the 16 ACN-ordered SN3D coefficients for a direction, on
        /// whichever thread builds the tap set; no allocation, no state.
        /// @param azimuth_rad 0 front, positive to the left.
        /// @param elevation_rad 0 horizontal, positive up.
        static void encode(double azimuth_rad, double elevation_rad,
                           std::array<float, kChannels>& gains) noexcept;
    };
}
