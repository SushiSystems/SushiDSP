/**************************************************************************/
/* band_response_fit.hpp                                                  */
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
 * @file band_response_fit.hpp
 * @brief The fit that turns six band gains into two biquad coefficient sets.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>

#include <SushiDSP/nodes/room/band_response.hpp>

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

            /// @brief Names why the fit stopped stepping.
            enum class Ending : std::uint8_t
            {
                Converged,      /**< The last accepted step was shorter than the tolerance. */
                NoAcceptedStep, /**< No damped step lowered the cost; the best point stays. */
                StepCap         /**< The step cap was reached with the steps still long. */
            };

            /// @brief Holds a fit and how far it misses.
            struct Result
            {
                    /// @brief Stores the fitted cascade.
                    Coefficients coefficients;

                    /// @brief Stores the largest absolute error over the six centres, in decibels.
                    double worst_error_db;

                    /// @brief Stores how many steps were accepted.
                    std::uint32_t steps = 0;

                    /// @brief Stores why the fit stopped.
                    Ending ending = Ending::Converged;
            };

            /// @brief Fits the cascade to the target's six band gains at the given sample rate,
            /// pulled toward the closed-form two-shelf guess so that one target has one answer.
            /// @pre sample_rate > 2 * K_BAND_CENTRES_HZ[K_BAND_COUNT - 1].
            static Result fit(const BandResponse& target, double sample_rate);

            /// @brief Returns the cascade's magnitude in decibels at one frequency.
            static double magnitude_db(const Coefficients& coefficients, double frequency_hz,
                                       double sample_rate);
    };
} // namespace SushiDSP::room
