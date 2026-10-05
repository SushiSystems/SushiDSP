/****************************************************************/
/* AirAbsorption.hpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file AirAbsorption.hpp
 * @brief The attenuation of sound in air at the room module's fixed atmosphere.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>

#include "SushiDSP/room/BandResponse.hpp"

namespace SushiDSP::room
{
    /// @brief Evaluates the ISO 9613-1:1993 attenuation at the module's fixed atmosphere, on the
    /// control thread. Spec §4.2 and §11.
    class AirAbsorption
    {
    public:
        /// @brief Fixes the air temperature, in kelvin.
        static constexpr double kTemperatureKelvin = 293.15;

        /// @brief Fixes the relative humidity, in percent.
        static constexpr double kRelativeHumidityPercent = 50.0;

        /// @brief Fixes the atmospheric pressure, in kilopascals.
        static constexpr double kPressureKilopascal = 101.325;

        /// @brief Returns the pure-tone attenuation at one frequency, in decibels per metre.
        static double attenuation_db_per_metre(double frequency_hz) noexcept;

        /// @brief Returns the energy attenuation coefficient m of Eyring's denominator for one
        /// band, per metre.
        /// @pre band < kBandCount.
        static double energy_coefficient_per_metre(std::size_t band) noexcept;

        /// @brief Returns the pressure gain a path of the given length keeps, per band.
        static BandResponse over_path(double metres) noexcept;
    };
}
