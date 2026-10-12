/**************************************************************************/
/* mp3_decoder.cpp                                                        */
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
 * @file mp3_decoder.cpp
 * @brief Defines the MPEG audio decode the host runs before a dropped song becomes a clip.
 * @author Mustafa Garip & Sushi Systems
 */

#include "mp3_decoder.hpp"

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
        constexpr drmp3_uint64 K_CHUNK_FRAMES = 8192;

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

            // The wide retry opens a UTF-8 path from a window drop; see applications/host_gui/README.md.
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
    } // namespace

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

        std::vector<float> interleaved(static_cast<std::size_t>(K_CHUNK_FRAMES) * count);
        try
        {
            for (;;)
            {
                const drmp3_uint64 read =
                    drmp3_read_pcm_frames_f32(&mp3, K_CHUNK_FRAMES, interleaved.data());
                if (read == 0)
                {
                    break;
                }
                append_deinterleaved(interleaved, static_cast<std::size_t>(read), decoded.channels);
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
} // namespace SushiDSP::host
