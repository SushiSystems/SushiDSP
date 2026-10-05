/****************************************************************/
/* MediumAbsorption.cpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file MediumAbsorption.cpp
 * @brief The air's loss and the furnishings' A_f / 4V loss folded into one per-metre figure.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/MediumAbsorption.hpp"

#include <cmath>

#include "SushiDSP/room/AirAbsorption.hpp"

namespace SushiDSP::room
{
    MediumAbsorption::MediumAbsorption(const RoomScene& scene)
        : furnished_(scene.furnishing_count > 0)
    {
        if (!furnished_)
        {
            return;
        }
        const std::array<double, kBandCount> area = scene.furnishing_absorption_area();
        const double four_volumes = 4.0 * scene.volume_m3();
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            furnishing_[band] = area[band] / four_volumes;
        }
    }

    double MediumAbsorption::energy_coefficient_per_metre(std::size_t band) const noexcept
    {
        const double air = AirAbsorption::energy_coefficient_per_metre(band);
        return furnished_ ? air + furnishing_[band] : air;
    }

    double MediumAbsorption::furnishing_coefficient_per_metre(std::size_t band) const noexcept
    {
        return furnishing_[band];
    }

    BandResponse MediumAbsorption::over_path(double metres) const noexcept
    {
        BandResponse response = AirAbsorption::over_path(metres);
        if (!furnished_)
        {
            return response;
        }
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            response.gain[band] *= static_cast<float>(std::exp(-0.5 * furnishing_[band] * metres));
        }
        return response;
    }
}
