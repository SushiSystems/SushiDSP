/**************************************************************************/
/* SincResampler.hpp                                                     */
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

#include <vector>

namespace SushiDSP
{
    /// @brief Offline windowed-sinc sample-rate converter (64 taps, Kaiser beta 8.6).
    ///
    /// Control-thread only: allocates and is not real-time safe. Intended for load-time
    /// conversion of impulse responses to the host's sample rate.
    class SincResampler
    {
    public:
        /// @brief Resamples a signal from one sample rate to another.
        /// @param input Source samples.
        /// @param input_rate Sample rate the input was captured at, in Hz.
        /// @param output_rate Sample rate to produce, in Hz.
        /// @return The resampled signal, rounded to the nearest whole sample count.
        /// @throws std::invalid_argument if either rate is not positive.
        static std::vector<float> resample(const std::vector<float>& input, double input_rate, double output_rate);
    };
}
