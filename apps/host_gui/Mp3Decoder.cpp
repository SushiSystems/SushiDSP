/**************************************************************************/
/* Mp3Decoder.cpp                                                         */
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


/**
 * @file Mp3Decoder.cpp
 * @brief Defines the MPEG audio decode the host runs before a dropped song becomes a clip.
 * @author Mustafa Garip & Sushi Systems
 */

#include "Mp3Decoder.hpp"

#include <cstddef>
#include <filesystem>
#include <stdexcept>

#define DR_MP3_IMPLEMENTATION
#include "dr_libs/dr_mp3.h"

namespace SushiDSP::host
{
    namespace
    {
        /// @brief How many frames one read pulls out of the decoder.
        constexpr drmp3_uint64 kChunkFrames = 8192;

        /// @brief Opens an mp3 for decoding.
        /// @param mp3 Receives the opened decoder.
        /// @param path Filesystem path to the mp3.
        /// @return True when the file is open and mp3 is initialised.
        bool open_mp3(drmp3& mp3, const std::string& path)
        {
            if (drmp3_init_file(&mp3, path.c_str(), nullptr) != DRMP3_FALSE)
            {
                return true;
            }

            // A path reaches this call narrow from the browser and UTF-8 from the window's drop
            // callback; the wide retry is what opens the second kind.
            const std::wstring wide = std::filesystem::u8path(path).wstring();
            return drmp3_init_file_w(&mp3, wide.c_str(), nullptr) != DRMP3_FALSE;
        }

        /// @brief Appends one interleaved chunk to the planar channels.
        /// @param interleaved frames * channels.size() samples, channel-major within a frame.
        /// @param frames How many frames the chunk carries.
        /// @param channels The planar destination, already holding one vector per channel.
        void append_deinterleaved(const std::vector<float>& interleaved, std::size_t frames,
                                  std::vector<std::vector<float>>& channels)
        {
            const std::size_t count = channels.size();
            for (std::size_t channel = 0; channel < count; ++channel)
            {
                std::vector<float>& destination = channels[channel];
                for (std::size_t frame = 0; frame < frames; ++frame)
                {
                    destination.push_back(interleaved[frame * count + channel]);
                }
            }
        }
    }

    DecodedAudio decode_mp3(const std::string& path)
    {
        drmp3 mp3{};
        if (!open_mp3(mp3, path))
        {
            throw std::runtime_error("cannot open mp3: " + path);
        }

        const std::size_t count = static_cast<std::size_t>(mp3.channels);
        if (count == 0 || mp3.sampleRate == 0)
        {
            drmp3_uninit(&mp3);
            throw std::runtime_error("mp3 carries no channels or no sample rate: " + path);
        }

        DecodedAudio decoded;
        decoded.sample_rate = static_cast<double>(mp3.sampleRate);
        decoded.channels.resize(count);

        std::vector<float> interleaved(static_cast<std::size_t>(kChunkFrames) * count);
        try
        {
            for (;;)
            {
                const drmp3_uint64 read =
                    drmp3_read_pcm_frames_f32(&mp3, kChunkFrames, interleaved.data());
                if (read == 0)
                {
                    break;
                }
                append_deinterleaved(interleaved, static_cast<std::size_t>(read),
                                     decoded.channels);
            }
        }
        catch (...)
        {
            drmp3_uninit(&mp3);
            throw;
        }
        drmp3_uninit(&mp3);

        if (decoded.channels.front().empty())
        {
            throw std::runtime_error("mp3 decoded to no frames: " + path);
        }
        return decoded;
    }
}
