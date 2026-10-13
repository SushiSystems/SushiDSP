/**************************************************************************/
/* mp3_encoder.hpp                                                        */
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
 *  @file mp3_encoder.hpp
 *  @brief Declares the MP3 writer: frames in, an MP3 file out, with LAME behind it.
 *  @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace SushiDSP::host
{
    /// @brief Streams stereo float frames into an MP3 file at a constant bit rate. The one class
    /// that touches LAME; no LAME type leaves it. Control thread: it allocates, throws and writes
    /// to disk.
    class Mp3Encoder
    {
        public:
            /// @brief Opens the file and sets the encoder up.
            /// @param path The file to create or overwrite.
            /// @param sample_rate The frames' rate in hertz; LAME resamples what MP3 cannot carry.
            /// @param kilobits_per_second The constant bit rate, one of MP3's: 128, 192, 256, 320.
            /// @throws std::runtime_error when the file cannot be opened or LAME refuses the setup.
            Mp3Encoder(const std::string& path, double sample_rate, int kilobits_per_second);

            /// @brief Flushes and closes the file if the caller did not call close().
            ~Mp3Encoder();
            Mp3Encoder(const Mp3Encoder&) = delete;
            Mp3Encoder& operator=(const Mp3Encoder&) = delete;

            /// @brief Encodes and writes a run of frames.
            /// @param left The left channel, frames samples in [-1, 1].
            /// @param right The right channel, frames samples in [-1, 1].
            /// @throws std::runtime_error when LAME or the file fails.
            void append(const float* left, const float* right, std::uint32_t frames);

            /// @brief Writes what the encoder still holds and closes the file. Safe to call twice.
            void close();

        private:
            /// @brief Holds LAME's encoder state, as the untyped handle the header may name.
            void* lame_ = nullptr;

            /// @brief Holds the file being written.
            std::FILE* file_ = nullptr;

            /// @brief Stores the encoded bytes of one call before they are written.
            std::vector<unsigned char> encoded_;
    };
} // namespace SushiDSP::host
