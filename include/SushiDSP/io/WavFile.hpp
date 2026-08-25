/**************************************************************************/
/* WavFile.hpp                                                            */
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

namespace SushiDSP
{
    /// @brief Reads a subset of the RIFF/WAVE format (PCM 16/24/32-bit or 32-bit IEEE float,
    /// any channel count) sufficient for loading impulse responses. Not real-time safe — call
    /// only at load time, off the audio thread, exactly like PartitionedConvolution's constructor.
    class WavFile
    {
    public:
        /// @brief Loads a WAV file, downmixing to mono and converting to float in [-1, 1].
        /// @param path Filesystem path to the WAV file.
        /// @param out_sample_rate If non-null, receives the file's sample rate in Hz.
        /// @return The mono sample data.
        /// @throws std::runtime_error if the file cannot be opened or parsed.
        static std::vector<float> load_mono(const std::string& path, double* out_sample_rate = nullptr);
    };
}
