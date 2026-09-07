/**************************************************************************/
/* WavWriter.hpp                                                          */
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

#include <cstdint>
#include <fstream>
#include <string>

namespace SushiDSP
{
    /// @brief Streaming WAV output: open, append frames, close. Not real-time safe — it
    /// allocates, throws and touches the filesystem, exactly like PartitionedConvolution's
    /// constructor and WavFile; call only from the control thread.
    class WavWriter
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
        WavWriter(const std::string& path, double sample_rate, std::uint32_t channels, Format format);

        /// @brief Closes the file if the caller did not already call close().
        ~WavWriter();

        WavWriter(const WavWriter&) = delete;
        WavWriter& operator=(const WavWriter&) = delete;

        /// @brief Appends frames, interleaving the channels.
        /// @param channels One pointer per channel, each with at least `frames` samples.
        /// @param frames Number of frames to append, per channel.
        void append(const float* const* channels, std::uint32_t frames);

        /// @brief Rewrites the RIFF and data sizes and closes the file. Safe to call twice.
        void close();

        /// @brief Returns the number of frames appended so far.
        std::uint64_t frames_written() const;

    private:
        std::ofstream m_file;
        std::uint32_t m_channels;
        Format m_format;
        std::uint64_t m_frames_written = 0;
        bool m_closed = false;
    };
}
