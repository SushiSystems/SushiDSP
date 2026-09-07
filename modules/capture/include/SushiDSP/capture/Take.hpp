/**************************************************************************/
/* Take.hpp                                                               */
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

        /// @brief Reads a WAV file into a single-channel Take. Control thread only.
        /// @param path Filesystem path to the WAV file.
        /// @return The loaded recording.
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
        std::vector<std::vector<float>> samples_;
        double                          sample_rate_;
        std::size_t                     frames_;
        std::uint32_t                   channels_;
    };
}
