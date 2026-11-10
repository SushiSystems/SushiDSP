/**************************************************************************/
/* wav_writer.hpp                                                         */
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
 * @file wav_writer.hpp
 * @brief Declares the streaming WAV writer.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>

#include <SushiDSP/foundation/io/audio_file_writer.hpp>

namespace SushiDSP
{
    /// @brief Streaming WAV output: open, append frames, close. Not real-time safe — it
    /// allocates, throws and touches the filesystem, exactly like PartitionedConvolution's
    /// constructor and WavFile; call only from the control thread.
    class WavWriter final : public AudioFileWriter
    {
        public:
            /// @brief Sample encoding written to the file.
            enum class Format
            {
                Pcm16,
                Float32
            };

            /// @brief Opens the file and writes a placeholder header.
            /// @param path Filesystem path to create or overwrite.
            /// @param sample_rate The file's sample rate in Hz.
            /// @param channels Number of interleaved channels.
            /// @param format Sample encoding to write.
            /// @throws std::runtime_error if the path cannot be opened for writing.
            WavWriter(const std::string& path, double sample_rate, std::uint32_t channels,
                      Format format);

            /// @brief Closes the file if the caller did not already call close().
            ~WavWriter() override;

            WavWriter(const WavWriter&) = delete;
            WavWriter& operator=(const WavWriter&) = delete;

            /// @brief Appends frames, interleaving the channels. Does nothing once close() has run.
            /// @param channels One pointer per channel, each with at least `frames` samples.
            /// @param frames Number of frames to append, per channel.
            void append(const float* const* channels, std::uint32_t frames) override;

            /// @brief Rewrites the RIFF and data sizes and closes the file. Safe to call twice.
            void close() override;

            /// @brief Returns the number of frames appended so far.
            std::uint64_t frames_written() const override;

        private:
            /// @brief The underlying file stream.
            std::ofstream file_;
            /// @brief Number of interleaved channels per frame.
            std::uint32_t channels_;
            /// @brief Sample encoding written to the file.
            Format format_;
            /// @brief Number of frames appended so far.
            std::uint64_t frames_written_ = 0;
            /// @brief Whether close() has already run.
            bool closed_ = false;
    };

    /// @brief The production AudioFileWriterOpener: opens a Float32 WavWriter.
    /// @throws std::runtime_error if the path cannot be opened for writing.
    std::unique_ptr<AudioFileWriter> open_float32_wav_writer(const std::string& path,
                                                             double sample_rate,
                                                             std::uint32_t channels);
} // namespace SushiDSP
