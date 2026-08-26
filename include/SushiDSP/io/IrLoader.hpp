/**************************************************************************/
/* IrLoader.hpp                                                           */
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

#include <string>
#include <vector>

#include "SushiDSP/io/Ir.hpp"

namespace SushiDSP
{
    /// @brief Conditioning applied to an impulse response as it is loaded.
    ///
    /// A max_length_ms of zero or less disables truncation, leaving the impulse response at
    /// whatever length the source file yields after resampling.
    struct IrLoadOptions
    {
        float max_length_ms    = 200.0f;
        float fade_ms          = 5.0f;
        bool  normalise        = true;
        float normalise_low_hz = 100.0f;
        float normalise_high_hz = 5000.0f;
    };

    /// @brief Loads cabinet impulse responses and conditions them for convolution.
    ///
    /// Resamples to the host rate, truncates to a bounded length with a raised-cosine fade-out,
    /// and normalises the mid-band average magnitude to 0 dB so swapping IRs does not change
    /// perceived loudness. Control-thread only: allocates, throws, and touches the filesystem.
    class IrLoader
    {
    public:
        /// @brief Loads an impulse response from a WAV file and conditions it.
        /// @param path Filesystem path to the WAV file.
        /// @param target_sample_rate Sample rate the impulse response is needed at, in Hz.
        /// @param options Conditioning options.
        /// @return The conditioned impulse response at target_sample_rate.
        /// @throws std::runtime_error if the file cannot be opened or parsed.
        static Ir load(const std::string& path, double target_sample_rate, const IrLoadOptions& options = {});

        /// @brief Conditions in-memory samples through the same pipeline load() uses.
        /// @param samples Source impulse response samples.
        /// @param source_rate Sample rate the samples were captured at, in Hz.
        /// @param target_sample_rate Sample rate the impulse response is needed at, in Hz.
        /// @param options Conditioning options.
        /// @return The conditioned impulse response at target_sample_rate.
        /// @throws std::invalid_argument if either sample rate is not positive.
        static Ir prepare(std::vector<float> samples, double source_rate, double target_sample_rate,
                          const IrLoadOptions& options = {});
    };
}
