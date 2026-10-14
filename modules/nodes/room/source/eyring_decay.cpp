/**************************************************************************/
/* eyring_decay.cpp                                                       */
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
 * @file eyring_decay.cpp
 * @brief Eyring's formula with the air and furnishing terms, over the scene's mean absorption.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/eyring_decay.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <SushiDSP/nodes/room/medium_absorption.hpp>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds Sabine's constant 24 * ln(10) / c at 20 degrees, in seconds per metre.
        constexpr double K_SABINE_CONSTANT = 0.161;

        /// @brief Bounds the mean absorption from above so that ln(1 - alpha) stays finite.
        constexpr double K_MAX_MEAN_ABSORPTION = 1.0 - 1e-9;

        /// @brief Names the band whose decay time sets the Schroeder frequency: 500 Hz.
        constexpr std::size_t K_MID_BAND = 2;

        /// @brief Evaluates Eyring's denominator -S ln(1 - alpha) + 4 m V for one band, in square
        /// metres, with m taken from the scene's medium: the air and the furnishings together.
        double eyring_denominator(const MediumAbsorption& medium, double mean_absorption,
                                  std::size_t band, double volume_m3,
                                  double surface_area_m2) noexcept
        {
            const double bounded = std::min(mean_absorption, K_MAX_MEAN_ABSORPTION);
            const double surface_term = -surface_area_m2 * std::log(1.0 - bounded);
            const double medium_term = 4.0 * medium.energy_coefficient_per_metre(band) * volume_m3;
            return surface_term + medium_term;
        }
    } // namespace

    std::array<double, K_BAND_COUNT> EyringDecay::rt60_seconds(const RoomScene& scene)
    {
        const double volume = scene.volume_m3();
        const double surface_area = scene.surface_area_m2();
        const std::array<double, K_BAND_COUNT> mean_absorption = scene.mean_absorption();
        const MediumAbsorption medium(scene);

        std::array<double, K_BAND_COUNT> result{};
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            // A band whose mean absorption reaches 1 has no reverberant field left to decay.
            if (mean_absorption[band] >= 1.0)
            {
                result[band] = 0.0;
                continue;
            }
            result[band] =
                K_SABINE_CONSTANT * volume /
                eyring_denominator(medium, mean_absorption[band], band, volume, surface_area);
        }
        return result;
    }

    double EyringDecay::schroeder_frequency_hz(const RoomScene& scene)
    {
        return 2000.0 * std::sqrt(rt60_seconds(scene)[K_MID_BAND] / scene.volume_m3());
    }
} // namespace SushiDSP::room
