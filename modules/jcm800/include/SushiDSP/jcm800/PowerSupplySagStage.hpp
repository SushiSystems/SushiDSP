/**************************************************************************/
/* PowerSupplySagStage.hpp                                               */
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
    /// @brief Models power-supply sag: an envelope follower on the rectified signal drives a
    /// dynamic gain-reduction factor, so sustained loud playing compresses ("bloom").
    class PowerSupplySagStage
    {
    public:
        /// @param sample_rate The sample rate in Hz.
        /// @param sag_amount How much gain reduction the envelope can cause, in [0, 1].
        /// @param recovery_time_seconds The envelope follower's one-pole time constant.
        PowerSupplySagStage(double sample_rate, float sag_amount, float recovery_time_seconds);

        /// @brief Processes one sample: updates the envelope, then applies gain reduction.
        /// @param input The input sample.
        /// @return The dynamically gain-reduced output sample. Always finite.
        float process_sample(float input);

        /// @brief Clears the envelope follower's state.
        void reset();

    private:
        float envelope_coefficient_;
        float sag_amount_;
        float envelope_ = 0.0f;
    };
}
