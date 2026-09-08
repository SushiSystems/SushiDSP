/**************************************************************************/
/* test_binaural_decoder.cpp                                              */
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
 * @file test_binaural_decoder.cpp
 * @brief Tests for BinauralDecoder on a synthetic set whose decoder filters are deltas.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "SushiDSP/math/StreamingConvolution.hpp"
#include "SushiDSP/room/AmbisonicEncoder.hpp"
#include "SushiDSP/room/BinauralDecoder.hpp"
#include "SushiDSP/room/HrtfSet.hpp"

namespace
{
    /// @brief Holds the sample rate every test runs at.
    constexpr double kSampleRate = 48000.0;

    /// @brief Holds the block size every test feeds process().
    constexpr std::uint32_t kBlock = 64;

    /// @brief Counts the Ambisonic channels.
    constexpr std::uint32_t kChannels = SushiDSP::room::AmbisonicEncoder::kChannels;

    /// @brief Counts the decoder filters, left ear then right.
    constexpr std::uint32_t kFilters = 2u * kChannels;

    /// @brief Builds a set whose left-ear decoder filters are deltas at tap 0 for the channels
    /// listed and zero everywhere else, the right ear included.
    SushiDSP::room::HrtfSet delta_set(const std::vector<std::uint32_t>& left_channels)
    {
        SushiDSP::room::HrtfSet set;
        std::vector<float> filters(
            static_cast<std::size_t>(kFilters) * SushiDSP::room::HrtfSet::kDecoderTaps, 0.0f);
        for (const std::uint32_t channel : left_channels)
        {
            filters[static_cast<std::size_t>(channel) * SushiDSP::room::HrtfSet::kDecoderTaps] =
                1.0f;
        }
        set.set_decoder(SushiDSP::room::AmbisonicEncoder::kOrder, filters);
        return set;
    }

    /// @brief Holds sixteen channel buffers of one block and the pointer table over them.
    struct Block
    {
        /// @brief Stores the sixteen channels, kBlock frames each, zero unless written.
        std::array<std::array<float, kBlock>, kChannels> channels{};

        /// @brief Stores one pointer per channel.
        std::array<const float*, kChannels> pointers{};

        /// @brief Points every entry at its channel.
        Block()
        {
            for (std::uint32_t channel = 0; channel < kChannels; ++channel)
            {
                pointers[channel] = channels[channel].data();
            }
        }
    };

    /// @brief Feeds an impulse at frame 0 on the listed channels, then silence, and collects
    /// total_frames of the left and right ears.
    void render_impulse(SushiDSP::room::BinauralDecoder& decoder,
                        const std::vector<std::uint32_t>& channels, std::uint32_t total_frames,
                        std::vector<float>& left, std::vector<float>& right)
    {
        left.assign(total_frames, -1.0f);
        right.assign(total_frames, -1.0f);
        Block block;
        for (const std::uint32_t channel : channels)
        {
            block.channels[channel][0] = 1.0f;
        }
        for (std::uint32_t start = 0; start < total_frames; start += kBlock)
        {
            decoder.process(block.pointers.data(), left.data() + start, right.data() + start,
                            kBlock);
            for (const std::uint32_t channel : channels)
            {
                block.channels[channel][0] = 0.0f;
            }
        }
    }
}

TEST(Unit_Dsp, BinauralDecoderWithDeltaFiltersPassesChannelZeroThrough)
{
    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(delta_set({ 0 }), kSampleRate, kBlock);
    ASSERT_TRUE(decoder.ready());

    std::vector<float> left;
    std::vector<float> right;
    render_impulse(decoder, { 0 }, 1024, left, right);

    const std::uint32_t onset = decoder.latency_frames();
    EXPECT_EQ(left[onset], 1.0f);
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        if (frame != onset)
        {
            EXPECT_EQ(left[frame], 0.0f) << "left frame " << frame;
        }
        EXPECT_EQ(right[frame], 0.0f) << "right frame " << frame;
    }
}

TEST(Unit_Dsp, BinauralDecoderReportsOnePartitionOfLatency)
{
    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(delta_set({ 0 }), kSampleRate, kBlock);
    EXPECT_EQ(decoder.latency_frames(),
              SushiDSP::StreamingConvolution::partition_size_for(kSampleRate));
    EXPECT_EQ(decoder.latency_frames(), 128u);
}

TEST(Unit_Dsp, BinauralDecoderSumsSixteenChannelsIntoEachEar)
{
    std::vector<std::uint32_t> all_channels;
    for (std::uint32_t channel = 0; channel < kChannels; ++channel)
    {
        all_channels.push_back(channel);
    }
    const SushiDSP::room::HrtfSet set = delta_set(all_channels);

    for (const std::uint32_t channel : all_channels)
    {
        SushiDSP::room::BinauralDecoder decoder;
        decoder.prepare(set, kSampleRate, kBlock);
        std::vector<float> left;
        std::vector<float> right;
        render_impulse(decoder, { channel }, 256, left, right);
        EXPECT_EQ(left[decoder.latency_frames()], 1.0f) << "channel " << channel;
    }

    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(set, kSampleRate, kBlock);
    std::vector<float> left;
    std::vector<float> right;
    render_impulse(decoder, all_channels, 256, left, right);
    EXPECT_EQ(left[decoder.latency_frames()], 16.0f);
    EXPECT_EQ(right[decoder.latency_frames()], 0.0f);
}

TEST(Unit_Dsp, BinauralDecoderRefusesASetWithNoDecoder)
{
    SushiDSP::room::BinauralDecoder decoder;
    EXPECT_THROW(decoder.prepare(SushiDSP::room::HrtfSet{}, kSampleRate, 256u),
                 std::runtime_error);
    EXPECT_FALSE(decoder.ready());
    EXPECT_EQ(decoder.latency_frames(), 0u);
}

TEST(Unit_Dsp, BinauralDecoderResetClearsTheHistory)
{
    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(delta_set({ 0 }), kSampleRate, kBlock);

    std::vector<float> left;
    std::vector<float> right;
    render_impulse(decoder, { 0 }, kBlock, left, right);
    decoder.reset();

    render_impulse(decoder, {}, 2048, left, right);
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_EQ(left[frame], 0.0f) << "left frame " << frame;
        EXPECT_EQ(right[frame], 0.0f) << "right frame " << frame;
    }
}
