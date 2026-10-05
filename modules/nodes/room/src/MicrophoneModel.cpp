/****************************************************************/
/* MicrophoneModel.cpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file MicrophoneModel.cpp
 * @brief The pattern, proximity and on-axis factors of the microphone model.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/MicrophoneModel.hpp"

#include <cmath>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double kPi = 3.14159265358979323846;

        /// @brief Holds the pressure share of each PolarPattern in enumeration order.
        constexpr std::array<double, 4> kPressureShare = { 1.0, 0.5, 0.366, 0.0 };
    }

    double pattern_pressure_share(PolarPattern pattern) noexcept
    {
        return kPressureShare[static_cast<std::size_t>(pattern)];
    }

    MicrophoneModel::MicrophoneModel(PolarPattern pattern,
                                     const std::array<float, kBandCount>& on_axis_db) noexcept
        : pressure_share_(pattern_pressure_share(pattern))
        , on_axis_(BandResponse::from_decibels(on_axis_db))
    {
    }

    double MicrophoneModel::proximity_boost(double frequency_hz, double distance_m) noexcept
    {
        const double ratio = kSpeedOfSound / (2.0 * kPi * frequency_hz * distance_m);
        return std::sqrt(1.0 + ratio * ratio);
    }

    BandResponse MicrophoneModel::gain_for(double incidence_rad, double distance_m) const noexcept
    {
        // The boost scales only the gradient share; see modules/nodes/room/README.md.
        const double gradient = (1.0 - pressure_share_) * std::cos(incidence_rad);
        BandResponse response{};
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            const double boost = proximity_boost(kBandCentresHz[band], distance_m);
            const double pattern = pressure_share_ + gradient * boost;
            response.gain[band] = static_cast<float>(
                std::fabs(pattern) * static_cast<double>(on_axis_.gain[band]));
        }
        return response;
    }
}
