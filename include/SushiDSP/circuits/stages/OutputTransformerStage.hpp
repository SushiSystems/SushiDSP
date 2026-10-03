/**************************************************************************/
/* OutputTransformerStage.hpp                                            */
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

#include "SushiDSP/math/Biquad.hpp"

namespace SushiDSP
{
    /// @brief Models the output transformer: primary-inductance low-end rolloff, leakage-
    /// inductance top-end shaping, an explicit tanh core-saturation waveshaper, and a secondary
    /// that carries no mean (docs/archive/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md
    /// section 14).
    class OutputTransformerStage
    {
    public:
        /// @param sample_rate The sample rate in Hz.
        /// @param low_rolloff_hz The primary inductance's low-end rolloff corner.
        /// @param leakage_shelf_hz The leakage inductance's top-end shaping corner.
        /// @param saturation_drive Controls how hard the core saturates; smaller values
        /// saturate at a higher output ceiling.
        OutputTransformerStage(double sample_rate, float low_rolloff_hz, float leakage_shelf_hz, float saturation_drive);

        /// @brief Processes one sample: rolloff, leakage shaping, core saturation, then the
        /// removal of the saturated signal's running mean.
        /// @param input The input sample.
        /// @return The transformed, saturated output sample. Always finite.
        float process_sample(float input);

        /// @brief Clears both filters' state and the secondary's running mean.
        void reset();

    private:
        /// @brief The primary-inductance rolloff, a second-order high-pass.
        Biquad rolloff_;

        /// @brief The leakage-inductance shaping, a second-order low-pass.
        Biquad leakage_;

        /// @brief Scales the tanh argument; the output ceiling is its reciprocal.
        float saturation_drive_;

        /// @brief The one-pole coefficient that tracks the saturated signal's mean, a decade
        /// under the rolloff's corner.
        double secondary_mean_coefficient_;

        /// @brief The running mean of the saturated signal, subtracted from the output.
        double secondary_mean_ = 0.0;
    };
}
