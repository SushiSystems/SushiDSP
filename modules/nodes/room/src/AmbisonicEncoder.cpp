/****************************************************************/
/* AmbisonicEncoder.cpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file AmbisonicEncoder.cpp
 * @brief The closed-form real spherical harmonics to order three, SN3D normalised.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/AmbisonicEncoder.hpp"

#include <cmath>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds sqrt(3).
        constexpr double kSqrt3 = 1.7320508075688772935;

        /// @brief Holds sqrt(15).
        constexpr double kSqrt15 = 3.8729833462074168852;

        /// @brief Holds sqrt(5 / 8).
        constexpr double kSqrt5Over8 = 0.79056941504209483300;

        /// @brief Holds sqrt(3 / 8).
        constexpr double kSqrt3Over8 = 0.61237243569579452455;
    }

    void AmbisonicEncoder::encode(double azimuth_rad, double elevation_rad,
                                  std::array<float, kChannels>& gains) noexcept
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
        gains[4] = static_cast<float>(kSqrt3 * x * y);
        gains[5] = static_cast<float>(kSqrt3 * y * z);
        gains[6] = static_cast<float>((3.0 * zz - 1.0) / 2.0);
        gains[7] = static_cast<float>(kSqrt3 * x * z);
        gains[8] = static_cast<float>(kSqrt3 / 2.0 * (xx - yy));
        gains[9] = static_cast<float>(kSqrt5Over8 * y * (3.0 * xx - yy));
        gains[10] = static_cast<float>(kSqrt15 * x * y * z);
        gains[11] = static_cast<float>(kSqrt3Over8 * y * (5.0 * zz - 1.0));
        gains[12] = static_cast<float>(z * (5.0 * zz - 3.0) / 2.0);
        gains[13] = static_cast<float>(kSqrt3Over8 * x * (5.0 * zz - 1.0));
        gains[14] = static_cast<float>(kSqrt15 / 2.0 * z * (xx - yy));
        gains[15] = static_cast<float>(kSqrt5Over8 * x * (xx - 3.0 * yy));
    }
}
