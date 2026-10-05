/****************************************************************/
/* Take.hpp                                                     */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Take.hpp
 * @brief Declares an in-memory multi-channel recording.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SushiDSP
{
    /// @brief An in-memory multi-channel recording: one contiguous vector per channel, plus the
    /// sample rate it was captured at. Every allocation happens in the constructor, so a Take
    /// built and published on the control thread is safe for the audio thread to read from.
    class Take
    {
    public:
        /// @brief Allocates zeroed storage for the given shape.
        /// @param channels Number of channels.
        /// @param sample_rate The recording's sample rate in Hz.
        /// @param frames Number of frames per channel.
        Take(std::uint32_t channels, double sample_rate, std::size_t frames);

        /// @brief Reads a WAV file into a Take of the file's own channel count. Control thread only.
        /// @param path Filesystem path to the WAV file.
        /// @return The loaded recording, at the file's sample rate and channel count.
        /// @throws std::runtime_error if the file cannot be opened or parsed.
        static Take load(const std::string& path);

        /// @brief Returns a channel's samples for reading.
        /// @param index Channel index.
        /// @return Pointer to frames() samples, or nullptr if the index is out of range.
        const float* channel(std::size_t index) const;

        /// @brief Returns a channel's samples for writing while the Take is still being filled.
        /// @param index Channel index.
        /// @return Pointer to frames() samples, or nullptr if the index is out of range.
        /// @pre Called on the control thread, before the Take is published to the audio thread.
        float* mutable_channel(std::size_t index);

        /// @brief Returns the number of frames per channel.
        std::size_t frames() const { return frames_; }

        /// @brief Returns the number of channels.
        std::uint32_t channels() const { return channels_; }

        /// @brief Returns the sample rate in Hz.
        double sample_rate() const { return sample_rate_; }

    private:
        /// @brief Holds one contiguous vector of samples per channel.
        std::vector<std::vector<float>> samples_;

        /// @brief Holds the rate the recording was captured at, in Hz.
        double                          sample_rate_;

        /// @brief Holds the number of frames each channel carries.
        std::size_t                     frames_;

        /// @brief Holds the number of channels.
        std::uint32_t                   channels_;
    };
}
