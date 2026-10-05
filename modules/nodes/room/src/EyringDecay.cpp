/****************************************************************/
/* EyringDecay.cpp                                              */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file EyringDecay.cpp
 * @brief Eyring's formula with the air and furnishing terms, over the scene's mean absorption.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/EyringDecay.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "SushiDSP/room/MediumAbsorption.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds Sabine's constant 24 * ln(10) / c at 20 degrees, in seconds per metre.
        constexpr double kSabineConstant = 0.161;

        /// @brief Bounds the mean absorption from above so that ln(1 - alpha) stays finite.
        constexpr double kMaxMeanAbsorption = 1.0 - 1e-9;

        /// @brief Names the band whose decay time sets the Schroeder frequency: 500 Hz.
        constexpr std::size_t kMidBand = 2;

        /// @brief Evaluates Eyring's denominator -S ln(1 - alpha) + 4 m V for one band, in square
        /// metres, with m taken from the scene's medium: the air and the furnishings together.
        double eyring_denominator(const MediumAbsorption& medium, double mean_absorption,
                                  std::size_t band, double volume_m3,
                                  double surface_area_m2) noexcept
        {
            const double bounded = std::min(mean_absorption, kMaxMeanAbsorption);
            const double surface_term = -surface_area_m2 * std::log(1.0 - bounded);
            const double medium_term = 4.0 * medium.energy_coefficient_per_metre(band) * volume_m3;
            return surface_term + medium_term;
        }
    }

    std::array<double, kBandCount> EyringDecay::rt60_seconds(const RoomScene& scene)
    {
        const double volume = scene.volume_m3();
        const double surface_area = scene.surface_area_m2();
        const std::array<double, kBandCount> mean_absorption = scene.mean_absorption();
        const MediumAbsorption medium(scene);

        std::array<double, kBandCount> result{};
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            // A band whose mean absorption reaches 1 has no reverberant field left to decay.
            if (mean_absorption[band] >= 1.0)
            {
                result[band] = 0.0;
                continue;
            }
            result[band] = kSabineConstant * volume
                / eyring_denominator(medium, mean_absorption[band], band, volume, surface_area);
        }
        return result;
    }

    double EyringDecay::schroeder_frequency_hz(const RoomScene& scene)
    {
        return 2000.0 * std::sqrt(rt60_seconds(scene)[kMidBand] / scene.volume_m3());
    }
}
