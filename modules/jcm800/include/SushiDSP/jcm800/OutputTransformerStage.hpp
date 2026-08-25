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
    /// inductance top-end shaping, and an explicit tanh core-saturation waveshaper.
    class OutputTransformerStage
    {
    public:
        /// @param sample_rate The sample rate in Hz.
        /// @param low_rolloff_hz The primary inductance's low-end rolloff corner.
        /// @param leakage_shelf_hz The leakage inductance's top-end shaping corner.
        /// @param saturation_drive Controls how hard the core saturates; smaller values
        /// saturate at a higher output ceiling.
        OutputTransformerStage(double sample_rate, float low_rolloff_hz, float leakage_shelf_hz, float saturation_drive);

        /// @brief Processes one sample: rolloff, then leakage shaping, then core saturation.
        /// @param input The input sample.
        /// @return The transformed, saturated output sample. Always finite.
        float process_sample(float input);

        /// @brief Clears both filters' state.
        void reset();

    private:
        Biquad rolloff_;
        Biquad leakage_;
        float saturation_drive_;
    };
}
