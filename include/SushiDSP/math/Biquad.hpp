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
