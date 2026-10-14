/**************************************************************************/
/* wav_file.hpp                                                           */
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
 * @file wav_file.hpp
 * @brief Declares the RIFF/WAVE reader that loads impulse responses.
 * @author Mustafa Garip
 */

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
            static std::vector<float> load_mono(const std::string& path,
                                                double* out_sample_rate = nullptr);

            /// @brief Loads a WAV file, keeping its channels apart and converting to float in [-1, 1].
            /// @param path Filesystem path to the WAV file.
            /// @param out_sample_rate If non-null, receives the file's sample rate in Hz.
            /// @return One vector of samples per channel, in the file's own channel order.
            /// @throws std::runtime_error if the file cannot be opened or parsed.
            static std::vector<std::vector<float>> load_channels(const std::string& path,
                                                                 double* out_sample_rate = nullptr);
    };
} // namespace SushiDSP
