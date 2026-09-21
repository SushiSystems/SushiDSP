/**************************************************************************/
/* Materials.cpp                                                          */
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
 * @file Materials.cpp
 * @brief The six transcribed absorption rows and the arithmetic on them.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/Materials.hpp"

#include <cmath>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Names the source every transcribed row below cites.
        constexpr const char* kEverest =
            "Everest and Pohlmann, Master Handbook of Acoustics, 5th ed., McGraw-Hill 2009, "
            "Appendix, Absorption coefficients";

        /// @brief Holds the whole shipped table, one row per MaterialId in enumeration order.
        constexpr std::array<Material, kMaterialCount> kTable = { {
            { "Concrete, unpainted", { 0.01f, 0.01f, 0.02f, 0.02f, 0.02f, 0.03f }, kEverest },
            { "Concrete block, painted", { 0.10f, 0.05f, 0.06f, 0.07f, 0.09f, 0.08f }, kEverest },
            { "Gypsum board 12.7 mm on studs", { 0.29f, 0.10f, 0.05f, 0.04f, 0.07f, 0.09f },
              kEverest },
            { "Heavy carpet on concrete", { 0.02f, 0.06f, 0.14f, 0.37f, 0.60f, 0.65f }, kEverest },
            { "Glass, ordinary window", { 0.35f, 0.25f, 0.18f, 0.12f, 0.07f, 0.04f }, kEverest },
            { "Open doorway", { 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f },
              "pseudo-material, alpha = 1 in every band; spec section 4.2" },
        } };

        /// @brief Holds the scattering coefficient of each Scattering word in enumeration order.
        constexpr std::array<float, 3> kScattering = { 0.10f, 0.25f, 0.40f };
    }

    const Material& material(MaterialId id) noexcept
    {
        return kTable[static_cast<std::size_t>(id)];
    }

    BandResponse reflection_factor(MaterialId id) noexcept
    {
        const Material& row = material(id);
        BandResponse response{};
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            response.gain[band] = static_cast<float>(
                std::sqrt(1.0 - static_cast<double>(row.absorption[band])));
        }
        return response;
    }

    float scattering_coefficient(Scattering scattering) noexcept
    {
        return kScattering[static_cast<std::size_t>(scattering)];
    }
}
