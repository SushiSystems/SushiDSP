/**************************************************************************/
/* BandResponseFit.hpp                                                    */
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
 * @file BandResponseFit.hpp
 * @brief The fit that turns six band gains into two biquad coefficient sets.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>

#include "SushiDSP/room/BandResponse.hpp"

namespace SushiDSP::room
{
    /// @brief Fits two biquads whose cascade matches six band gains, on the control thread.
    /// Spec §4.2.
    class BandResponseFit
    {
    public:
        /// @brief Holds the two normalised coefficient sets of the cascade.
        struct Coefficients
        {
            /// @brief Stores b0, b1, b2, a1, a2 of the first section.
            std::array<double, 5> section_a;

            /// @brief Stores b0, b1, b2, a1, a2 of the second section.
            std::array<double, 5> section_b;
        };

        /// @brief Holds a fit and how far it misses.
        struct Result
        {
            /// @brief Stores the fitted cascade.
            Coefficients coefficients;

            /// @brief Stores the largest absolute error over the six centres, in decibels.
            double worst_error_db;
        };

        /// @brief Fits the cascade to the target's six band gains at the given sample rate.
        /// @pre sample_rate > 2 * kBandCentresHz[kBandCount - 1].
        static Result fit(const BandResponse& target, double sample_rate);

        /// @brief Returns the cascade's magnitude in decibels at one frequency.
        static double magnitude_db(const Coefficients& coefficients, double frequency_hz,
                                   double sample_rate);
    };
}
