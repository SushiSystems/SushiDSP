/**************************************************************************/
/* audio_file_writer.hpp                                                  */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file audio_file_writer.hpp
 * @brief Declares the port through which a recorder appends frames to an audio file.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace SushiDSP
{
    /// @brief The write side of an audio file: append frames, close, count. WavWriter is the
    /// production adapter and InMemoryAudioFileStore holds the in-memory one. Not real-time
    /// safe: an adapter may allocate, throw and touch the filesystem, so call it from the control
    /// thread or a writer thread.
    class AudioFileWriter
    {
        public:
            virtual ~AudioFileWriter() = default;

            /// @brief Appends frames, interleaving the channels. Does nothing once close() has run.
            /// @param channels One pointer per channel, each with at least `frames` samples.
            /// @param frames Number of frames to append, per channel.
            virtual void append(const float* const* channels, std::uint32_t frames) = 0;

            /// @brief Finishes the file. Safe to call twice.
            virtual void close() = 0;

            /// @brief Returns the number of frames appended so far.
            virtual std::uint64_t frames_written() const = 0;
    };

    /// @brief Opens a writer: path, sample rate in Hz and channel count in, an open writer out.
    /// An opener that cannot open the path throws std::runtime_error.
    using AudioFileWriterOpener = std::function<std::unique_ptr<AudioFileWriter>(
        const std::string& path, double sample_rate, std::uint32_t channels)>;
} // namespace SushiDSP
