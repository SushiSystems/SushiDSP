/****************************************************************/
/* WavWriter.hpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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

        /// @brief Appends frames, interleaving the channels. Does nothing once close() has run.
        /// @param channels One pointer per channel, each with at least `frames` samples.
        /// @param frames Number of frames to append, per channel.
        void append(const float* const* channels, std::uint32_t frames);

        /// @brief Rewrites the RIFF and data sizes and closes the file. Safe to call twice.
        void close();

        /// @brief Returns the number of frames appended so far.
        std::uint64_t frames_written() const;

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
}
