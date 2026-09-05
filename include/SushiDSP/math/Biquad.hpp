/**************************************************************************/
/* Biquad.hpp                                                             */
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

#pragma once

namespace SushiDSP
{
    /// @brief Direct Form I biquad filter with RBJ cookbook coefficient derivation.
    class Biquad
    {
    public:
        /// @brief Configures this filter as a low-pass with the given cutoff and Q.
        /// @param sample_rate Sample rate in Hz.
        /// @param cutoff_hz Cutoff frequency in Hz.
        /// @param q Quality factor (0.707 = Butterworth).
        void set_low_pass(double sample_rate, double cutoff_hz, double q);

        /// @brief Configures this filter as a first-order (one-pole) low-pass.
        ///
        /// A single RC section, bilinear-transformed with the corner prewarped, so the second
        /// numerator and denominator taps are zero. This is the right shape for a physical RC
        /// pole - a Butterworth biquad would roll off at twice the rate a real one does.
        /// @param sample_rate Sample rate in Hz.
        /// @param cutoff_hz Corner frequency in Hz, where the response is -3 dB.
        void set_one_pole_low_pass(double sample_rate, double cutoff_hz);

        /// @brief Configures this filter as a high-pass with the given cutoff and Q.
        /// @param sample_rate Sample rate in Hz.
        /// @param cutoff_hz Cutoff frequency in Hz.
        /// @param q Quality factor (0.707 = Butterworth).
        void set_high_pass(double sample_rate, double cutoff_hz, double q);

        /// @brief Configures this filter as a low-shelf with the given corner, gain, and Q.
        /// @param sample_rate Sample rate in Hz.
        /// @param cutoff_hz Shelf corner frequency in Hz.
        /// @param gain_db Shelf gain in decibels (positive boosts, negative cuts).
        /// @param q Shelf slope parameter (RBJ cookbook S=1 equivalent at q=1.0).
        void set_low_shelf(double sample_rate, double cutoff_hz, double gain_db, double q);

        /// @brief Configures this filter as a high-shelf with the given corner, gain, and Q.
        /// @param sample_rate Sample rate in Hz.
        /// @param cutoff_hz Shelf corner frequency in Hz.
        /// @param gain_db Shelf gain in decibels.
        /// @param q Shelf slope parameter.
        void set_high_shelf(double sample_rate, double cutoff_hz, double gain_db, double q);

        /// @brief Configures this filter as a peaking EQ with the given center, gain, and Q.
        /// @param sample_rate Sample rate in Hz.
        /// @param center_hz Center frequency in Hz.
        /// @param gain_db Peak gain in decibels (positive boosts, negative cuts).
        /// @param q Bandwidth parameter (higher Q = narrower peak).
        void set_peaking(double sample_rate, double center_hz, double gain_db, double q);

        /// @brief Configures this filter as a first-order shelf with independent zero and pole.
        ///
        /// Both corners are prewarped separately, so a mirrored pair cancels exactly; see
        /// docs/superpowers/specs/2026-09-05-dd3-spx90-design.md for the derivation.
        /// @param sample_rate Sample rate in Hz.
        /// @param zero_hz Frequency of the transfer function's zero, in Hz.
        /// @param pole_hz Frequency of its pole, in Hz.
        /// @param low_frequency_gain Gain below both corners; 1.0 is unity.
        void set_first_order_shelf(double sample_rate, double zero_hz, double pole_hz,
                                   double low_frequency_gain);

        /// @brief Processes one sample through the filter.
        /// @param input The input sample.
        /// @return The filtered output sample.
        float process_sample(float input);

        /// @brief Clears the filter's internal state.
        void reset();

    private:
        float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
        float a1_ = 0.0f, a2_ = 0.0f;
        float x1_ = 0.0f, x2_ = 0.0f;
        float y1_ = 0.0f, y2_ = 0.0f;
    };
}
