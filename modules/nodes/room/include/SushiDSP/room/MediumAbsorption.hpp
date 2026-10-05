/****************************************************************/
/* MediumAbsorption.hpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file MediumAbsorption.hpp
 * @brief The energy the room's air and furnishings take from a path or a decay, per metre.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>

#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace SushiDSP::room
{
    /// @brief Evaluates the volumetric loss of one scene's medium: the air's ISO 9613-1 figure
    /// plus A_f(f) / 4V for the furnishings, on the control thread. Design:
    /// docs/design/ROOM_FURNISHINGS.md section 3.
    class MediumAbsorption
    {
    public:
        /// @brief Sums the scene's furnishing area per band and divides it by four times the
        /// scene's volume.
        /// @pre scene.is_valid() holds.
        explicit MediumAbsorption(const RoomScene& scene);

        /// @brief Returns the energy attenuation coefficient of the air and the furnishings
        /// together for one band, per metre; the air's own figure, unchanged, when the scene holds
        /// no furnishing.
        /// @pre band < kBandCount.
        double energy_coefficient_per_metre(std::size_t band) const noexcept;

        /// @brief Returns the energy attenuation coefficient of the furnishings alone for one
        /// band, per metre; zero when the scene holds none.
        /// @pre band < kBandCount.
        double furnishing_coefficient_per_metre(std::size_t band) const noexcept;

        /// @brief Returns the pressure gain a reflected path of the given length keeps, per band,
        /// through the air and the furnishings; the air's own gain, unchanged, when the scene
        /// holds no furnishing.
        BandResponse over_path(double metres) const noexcept;

    private:
        /// @brief Stores A_f(f) / 4V per band, in energy attenuation per metre.
        std::array<double, kBandCount> furnishing_{};

        /// @brief Records that the scene holds at least one furnishing.
        bool furnished_ = false;
    };
}
