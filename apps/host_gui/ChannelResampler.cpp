/**************************************************************************/
/* ChannelResampler.cpp                                                   */
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
 * @file ChannelResampler.cpp
 * @brief Defines the rate conversion the host applies to a decoded file before it is written.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ChannelResampler.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "SushiDSP/math/Resampler.hpp"

namespace SushiDSP::host
{
    namespace
    {
        /// @brief How many frames one push into the resampler carries.
        constexpr std::uint32_t kBlockFrames = 4096;

        /// @brief How many blocks of silence follow the input at most. The last outputs read
        /// input from past the end of the file, and silence is what draws them out.
        constexpr std::size_t kFlushBlocks = 4;

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
            std::size_t        written = 0;

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

            for (std::size_t position = 0; position < source.size(); position += kBlockFrames)
            {
                push(source.data() + position,
                     std::min<std::size_t>(kBlockFrames, source.size() - position));
            }

            const std::vector<float> silence(kBlockFrames, 0.0f);
            for (std::size_t block = 0; block < kFlushBlocks && written < converted.size(); ++block)
            {
                push(silence.data(), kBlockFrames);
            }
            return converted;
        }
    }

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

    std::vector<std::vector<float>> resample_channels(
        const std::vector<std::vector<float>>& channels, double source_rate, double target_rate)
    {
        if (!rates_differ(source_rate, target_rate) || channels.empty())
        {
            return channels;
        }

        const std::size_t expected =
            resampled_frame_count(channels.front().size(), source_rate, target_rate);
        const double ratio = target_rate / source_rate;

        SushiDSP::Resampler resampler;
        resampler.prepare(source_rate, target_rate, kBlockFrames, std::min(ratio, 1.0));

        std::vector<std::vector<float>> converted;
        converted.reserve(channels.size());
        for (const std::vector<float>& channel : channels)
        {
            resampler.reset();
            converted.push_back(resample_one(resampler, channel, expected));
        }
        return converted;
    }
}
