/**************************************************************************/
/* ambisonic_encoder.cpp                                                  */
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
 * @file ambisonic_encoder.cpp
 * @brief The closed-form real spherical harmonics to order three, SN3D normalised.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/ambisonic_encoder.hpp"

#include <cmath>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds sqrt(3).
        constexpr double K_SQRT3 = 1.7320508075688772935;

        /// @brief Holds sqrt(15).
        constexpr double K_SQRT15 = 3.8729833462074168852;

        /// @brief Holds sqrt(5 / 8).
        constexpr double K_SQRT5_OVER8 = 0.79056941504209483300;

        /// @brief Holds sqrt(3 / 8).
        constexpr double K_SQRT3_OVER8 = 0.61237243569579452455;
    } // namespace

    void AmbisonicEncoder::encode(double azimuth_rad, double elevation_rad,
                                  std::array<float, K_CHANNELS>& gains) noexcept
    {
        const double cos_elevation = std::cos(elevation_rad);
        const double x = cos_elevation * std::cos(azimuth_rad);
        const double y = cos_elevation * std::sin(azimuth_rad);
        const double z = std::sin(elevation_rad);
        const double xx = x * x;
        const double yy = y * y;
        const double zz = z * z;

        gains[0] = 1.0f;
        gains[1] = static_cast<float>(y);
        gains[2] = static_cast<float>(z);
        gains[3] = static_cast<float>(x);
        gains[4] = static_cast<float>(K_SQRT3 * x * y);
        gains[5] = static_cast<float>(K_SQRT3 * y * z);
        gains[6] = static_cast<float>((3.0 * zz - 1.0) / 2.0);
        gains[7] = static_cast<float>(K_SQRT3 * x * z);
        gains[8] = static_cast<float>(K_SQRT3 / 2.0 * (xx - yy));
        gains[9] = static_cast<float>(K_SQRT5_OVER8 * y * (3.0 * xx - yy));
        gains[10] = static_cast<float>(K_SQRT15 * x * y * z);
        gains[11] = static_cast<float>(K_SQRT3_OVER8 * y * (5.0 * zz - 1.0));
        gains[12] = static_cast<float>(z * (5.0 * zz - 3.0) / 2.0);
        gains[13] = static_cast<float>(K_SQRT3_OVER8 * x * (5.0 * zz - 1.0));
        gains[14] = static_cast<float>(K_SQRT15 / 2.0 * z * (xx - yy));
        gains[15] = static_cast<float>(K_SQRT5_OVER8 * x * (xx - 3.0 * yy));
    }
} // namespace SushiDSP::room
