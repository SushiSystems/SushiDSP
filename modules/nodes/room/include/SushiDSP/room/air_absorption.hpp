/**************************************************************************/
/* air_absorption.hpp                                                     */
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
 * @file air_absorption.hpp
 * @brief The attenuation of sound in air at the room module's fixed atmosphere.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>

#include "SushiDSP/room/band_response.hpp"

namespace SushiDSP::room
{
    /// @brief Evaluates the ISO 9613-1:1993 attenuation at the module's fixed atmosphere, on the
    /// control thread. Spec §4.2 and §11.
    class AirAbsorption
    {
        public:
            /// @brief Fixes the air temperature, in kelvin.
            static constexpr double K_TEMPERATURE_KELVIN = 293.15;

            /// @brief Fixes the relative humidity, in percent.
            static constexpr double K_RELATIVE_HUMIDITY_PERCENT = 50.0;

            /// @brief Fixes the atmospheric pressure, in kilopascals.
            static constexpr double K_PRESSURE_KILOPASCAL = 101.325;

            /// @brief Returns the pure-tone attenuation at one frequency, in decibels per metre.
            static double attenuation_db_per_metre(double frequency_hz) noexcept;

            /// @brief Returns the energy attenuation coefficient m of Eyring's denominator for one
            /// band, per metre.
            /// @pre band < K_BAND_COUNT.
            static double energy_coefficient_per_metre(std::size_t band) noexcept;

            /// @brief Returns the pressure gain a path of the given length keeps, per band.
            static BandResponse over_path(double metres) noexcept;
    };
} // namespace SushiDSP::room
