/**************************************************************************/
/* AirAbsorption.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file AirAbsorption.cpp
 * @brief The ISO 9613-1:1993 closed form evaluated at the module's fixed atmosphere.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/AirAbsorption.hpp"

#include <array>
#include <cmath>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds the reference temperature T0 of ISO 9613-1, in kelvin.
        constexpr double kReferenceTemperatureKelvin = 293.15;

        /// @brief Holds the triple-point temperature T01 of ISO 9613-1, in kelvin.
        constexpr double kTriplePointKelvin = 273.16;

        /// @brief Holds the reference pressure p_r of ISO 9613-1, in kilopascals.
        constexpr double kReferencePressureKilopascal = 101.325;

        /// @brief Holds the relaxation frequencies and the temperature ratios the closed form
        /// evaluates once for the fixed atmosphere.
        struct Atmosphere
        {
            /// @brief Stores p_a / p_r.
            double pressure_ratio;

            /// @brief Stores T / T0.
            double temperature_ratio;

            /// @brief Stores the oxygen relaxation frequency F_rO, in hertz.
            double oxygen_relaxation_hz;

            /// @brief Stores the nitrogen relaxation frequency F_rN, in hertz.
            double nitrogen_relaxation_hz;
        };

        /// @brief Evaluates the relaxation frequencies of the fixed atmosphere.
        Atmosphere fixed_atmosphere() noexcept
        {
            const double temperature = AirAbsorption::kTemperatureKelvin;
            const double pressure_ratio =
                AirAbsorption::kPressureKilopascal / kReferencePressureKilopascal;
            const double temperature_ratio = temperature / kReferenceTemperatureKelvin;
            const double saturation_ratio = std::pow(
                10.0, -6.8346 * std::pow(kTriplePointKelvin / temperature, 1.261) + 4.6151);
            const double molar_humidity =
                AirAbsorption::kRelativeHumidityPercent * saturation_ratio / pressure_ratio;
            const double oxygen = pressure_ratio
                * (24.0 + 4.04e4 * molar_humidity * (0.02 + molar_humidity)
                              / (0.391 + molar_humidity));
            const double nitrogen = pressure_ratio * std::pow(temperature_ratio, -0.5)
                * (9.0 + 280.0 * molar_humidity
                             * std::exp(-4.170 * (std::pow(temperature_ratio, -1.0 / 3.0) - 1.0)));
            return Atmosphere{ pressure_ratio, temperature_ratio, oxygen, nitrogen };
        }
    }

    double AirAbsorption::attenuation_db_per_metre(double frequency_hz) noexcept
    {
        static const Atmosphere atmosphere = fixed_atmosphere();
        const double temperature = kTemperatureKelvin;
        const double frequency_squared = frequency_hz * frequency_hz;
        const double classical = 1.84e-11 / atmosphere.pressure_ratio
            * std::sqrt(atmosphere.temperature_ratio);
        const double oxygen = 0.01275 * std::exp(-2239.1 / temperature)
            / (atmosphere.oxygen_relaxation_hz
               + frequency_squared / atmosphere.oxygen_relaxation_hz);
        const double nitrogen = 0.1068 * std::exp(-3352.0 / temperature)
            / (atmosphere.nitrogen_relaxation_hz
               + frequency_squared / atmosphere.nitrogen_relaxation_hz);
        const double relaxation =
            std::pow(atmosphere.temperature_ratio, -2.5) * (oxygen + nitrogen);
        return 8.686 * frequency_squared * (classical + relaxation);
    }

    double AirAbsorption::energy_coefficient_per_metre(std::size_t band) noexcept
    {
        static const std::array<double, kBandCount> coefficients = []
        {
            std::array<double, kBandCount> table{};
            const double decibels_per_neper = 10.0 * std::log10(std::exp(1.0));
            for (std::size_t index = 0; index < kBandCount; ++index)
            {
                table[index] =
                    attenuation_db_per_metre(kBandCentresHz[index]) / decibels_per_neper;
            }
            return table;
        }();
        return coefficients[band];
    }

    BandResponse AirAbsorption::over_path(double metres) noexcept
    {
        BandResponse response{};
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            const double decibels = attenuation_db_per_metre(kBandCentresHz[band]) * metres;
            response.gain[band] = static_cast<float>(std::pow(10.0, -decibels / 20.0));
        }
        return response;
    }
}
