/**************************************************************************/
/* channel_resampler.cpp                                                  */
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
 * @file channel_resampler.cpp
 * @brief Defines the rate conversion the host applies to a decoded file before it is written.
 * @author Mustafa Garip & Sushi Systems
 */

#include "channel_resampler.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <SushiDSP/foundation/math/resampler.hpp>

namespace SushiDSP::host
{
    namespace
    {
        /// @brief How many frames one push into the resampler carries.
        constexpr std::uint32_t K_BLOCK_FRAMES = 4096;

        /// @brief How many blocks of silence follow the input at most. The last outputs read
        /// input from past the end of the file, and silence is what draws them out.
        constexpr std::size_t K_FLUSH_BLOCKS = 4;

        /// @brief Reports whether a pair of rates asks for any conversion at all.
        /// @param source_rate The rate the samples carry, in Hz.
        /// @param target_rate The rate wanted, in Hz.
        /// @return True when the samples must be converted.
        bool rates_differ(double source_rate, double target_rate)
        {
            return source_rate > 0.0 && target_rate > 0.0 && source_rate != target_rate;
        }

        /// @brief Converts one channel, flushing the resampler's look-ahead at the end so the
        /// last frames of the file are emitted.
        /// @param resampler The resampler, already prepared and reset for this channel.
        /// @param source The channel's samples.
        /// @param expected How many samples the conversion produces.
        /// @return The converted channel, exactly expected samples long; a conversion that ends
        /// short leaves the remainder at silence.
        std::vector<float> resample_one(SushiDSP::Resampler& resampler,
                                        const std::vector<float>& source, std::size_t expected)
        {
            std::vector<float> converted(expected, 0.0f);
            std::size_t written = 0;

            const auto push = [&](const float* data, std::size_t frames)
            {
                if (written >= converted.size())
                {
                    return;
                }
                const std::uint32_t room = static_cast<std::uint32_t>(converted.size() - written);
                written += resampler.process(data, static_cast<std::uint32_t>(frames),
                                             converted.data() + written, room);
            };

            for (std::size_t position = 0; position < source.size(); position += K_BLOCK_FRAMES)
            {
                push(source.data() + position,
                     std::min<std::size_t>(K_BLOCK_FRAMES, source.size() - position));
            }

            const std::vector<float> silence(K_BLOCK_FRAMES, 0.0f);
            for (std::size_t block = 0; block < K_FLUSH_BLOCKS && written < converted.size();
                 ++block)
            {
                push(silence.data(), K_BLOCK_FRAMES);
            }
            return converted;
        }
    } // namespace

    std::size_t resampled_frame_count(std::size_t source_frames, double source_rate,
                                      double target_rate)
    {
        if (!rates_differ(source_rate, target_rate))
        {
            return source_frames;
        }
        const double frames = static_cast<double>(source_frames) * target_rate / source_rate;
        return static_cast<std::size_t>(std::llround(frames));
    }

    std::vector<std::vector<float>>
    resample_channels(const std::vector<std::vector<float>>& channels, double source_rate,
                      double target_rate)
    {
        if (!rates_differ(source_rate, target_rate) || channels.empty())
        {
            return channels;
        }

        const std::size_t expected =
            resampled_frame_count(channels.front().size(), source_rate, target_rate);
        const double ratio = target_rate / source_rate;

        SushiDSP::Resampler resampler;
        resampler.prepare(source_rate, target_rate, K_BLOCK_FRAMES, std::min(ratio, 1.0));

        std::vector<std::vector<float>> converted;
        converted.reserve(channels.size());
        for (const std::vector<float>& channel : channels)
        {
            resampler.reset();
            converted.push_back(resample_one(resampler, channel, expected));
        }
        return converted;
    }
} // namespace SushiDSP::host
