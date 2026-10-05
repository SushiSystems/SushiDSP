/****************************************************************/
/* BandResponseFit.hpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
    /// @brief Fits two biquads whose cascade matches six band gains, on the control thread, so
    /// that nearby targets fit to nearby coefficients. Spec §4.2.
    class BandResponseFit
    {
    public:
        /// @brief Holds the two normalised coefficient sets of the cascade in canonical form:
        /// the low shelf first carrying the whole gain, the high shelf second at unity gain at
        /// zero frequency.
        struct Coefficients
        {
            /// @brief Stores b0, b1, b2, a1, a2 of the first section, the low shelf.
            std::array<double, 5> section_a;

            /// @brief Stores b0, b1, b2, a1, a2 of the second section, the high shelf.
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

        /// @brief Fits the cascade to the target's six band gains at the given sample rate,
        /// pulled toward the closed-form two-shelf guess so that one target has one answer.
        /// @pre sample_rate > 2 * kBandCentresHz[kBandCount - 1].
        static Result fit(const BandResponse& target, double sample_rate);

        /// @brief Returns the cascade's magnitude in decibels at one frequency.
        static double magnitude_db(const Coefficients& coefficients, double frequency_hz,
                                   double sample_rate);
    };
}
