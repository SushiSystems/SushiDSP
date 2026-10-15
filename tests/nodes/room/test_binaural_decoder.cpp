/**************************************************************************/
/* test_binaural_decoder.cpp                                              */
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
 * @file test_binaural_decoder.cpp
 * @brief Tests for BinauralDecoder on a synthetic set whose decoder filters are deltas.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/math/streaming_convolution.hpp>
#include <SushiDSP/nodes/room/ambisonic_encoder.hpp>
#include <SushiDSP/nodes/room/binaural_decoder.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    /// @brief Holds the sample rate every test runs at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Holds the block size every test feeds process().
    constexpr std::uint32_t K_BLOCK = 64;

    /// @brief Counts the Ambisonic channels.
    constexpr std::uint32_t K_CHANNELS = SushiDSP::room::AmbisonicEncoder::K_CHANNELS;

    /// @brief Counts the decoder filters, left ear then right.
    constexpr std::uint32_t K_FILTERS = 2u * K_CHANNELS;

    /// @brief Builds a set whose left-ear decoder filters are deltas at tap 0 for the channels
    /// listed and zero everywhere else, the right ear included.
    SushiDSP::room::HrtfSet delta_set(const std::vector<std::uint32_t>& left_channels)
    {
        SushiDSP::room::HrtfSet set;
        std::vector<float> filters(
            static_cast<std::size_t>(K_FILTERS) * SushiDSP::room::HrtfSet::K_DECODER_TAPS, 0.0f);
        for (const std::uint32_t channel : left_channels)
        {
            filters[static_cast<std::size_t>(channel) * SushiDSP::room::HrtfSet::K_DECODER_TAPS] =
                1.0f;
        }
        set.set_decoder(SushiDSP::room::AmbisonicEncoder::K_ORDER, filters);
        return set;
    }

    /// @brief Holds sixteen channel buffers of one block and the pointer table over them.
    struct Block
    {
            /// @brief Stores the sixteen channels, K_BLOCK frames each, zero unless written.
            std::array<std::array<float, K_BLOCK>, K_CHANNELS> channels{};

            /// @brief Stores one pointer per channel.
            std::array<const float*, K_CHANNELS> pointers{};

            /// @brief Points every entry at its channel.
            Block()
            {
                for (std::uint32_t channel = 0; channel < K_CHANNELS; ++channel)
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
        for (std::uint32_t start = 0; start < total_frames; start += K_BLOCK)
        {
            decoder.process(block.pointers.data(), left.data() + start, right.data() + start,
                            K_BLOCK);
            for (const std::uint32_t channel : channels)
            {
                block.channels[channel][0] = 0.0f;
            }
        }
    }
} // namespace

TEST(BinauralDecoder, BinauralDecoderWithDeltaFiltersPassesChannelZeroThrough)
{
    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(delta_set({0}), K_SAMPLE_RATE, K_BLOCK);
    ASSERT_TRUE(decoder.ready());

    std::vector<float> left;
    std::vector<float> right;
    render_impulse(decoder, {0}, 1024, left, right);

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

TEST(BinauralDecoder, BinauralDecoderReportsOnePartitionOfLatency)
{
    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(delta_set({0}), K_SAMPLE_RATE, K_BLOCK);
    EXPECT_EQ(decoder.latency_frames(),
              SushiDSP::StreamingConvolution::partition_size_for(K_SAMPLE_RATE));
    EXPECT_EQ(decoder.latency_frames(), 128u);
}

TEST(BinauralDecoder, BinauralDecoderSumsSixteenChannelsIntoEachEar)
{
    std::vector<std::uint32_t> all_channels;
    for (std::uint32_t channel = 0; channel < K_CHANNELS; ++channel)
    {
        all_channels.push_back(channel);
    }
    const SushiDSP::room::HrtfSet set = delta_set(all_channels);

    for (const std::uint32_t channel : all_channels)
    {
        SushiDSP::room::BinauralDecoder decoder;
        decoder.prepare(set, K_SAMPLE_RATE, K_BLOCK);
        std::vector<float> left;
        std::vector<float> right;
        render_impulse(decoder, {channel}, 256, left, right);
        EXPECT_EQ(left[decoder.latency_frames()], 1.0f) << "channel " << channel;
    }

    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(set, K_SAMPLE_RATE, K_BLOCK);
    std::vector<float> left;
    std::vector<float> right;
    render_impulse(decoder, all_channels, 256, left, right);
    EXPECT_EQ(left[decoder.latency_frames()], 16.0f);
    EXPECT_EQ(right[decoder.latency_frames()], 0.0f);
}

TEST(BinauralDecoder, BinauralDecoderRefusesASetWithNoDecoder)
{
    SushiDSP::room::BinauralDecoder decoder;
    EXPECT_THROW(decoder.prepare(SushiDSP::room::HrtfSet{}, K_SAMPLE_RATE, 256u),
                 std::runtime_error);
    EXPECT_FALSE(decoder.ready());
    EXPECT_EQ(decoder.latency_frames(), 0u);
}

TEST(BinauralDecoder, BinauralDecoderResetClearsTheHistory)
{
    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(delta_set({0}), K_SAMPLE_RATE, K_BLOCK);

    std::vector<float> left;
    std::vector<float> right;
    render_impulse(decoder, {0}, K_BLOCK, left, right);
    decoder.reset();

    render_impulse(decoder, {}, 2048, left, right);
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_EQ(left[frame], 0.0f) << "left frame " << frame;
        EXPECT_EQ(right[frame], 0.0f) << "right frame " << frame;
    }
}

namespace
{
    /// @brief Fills a block with a 220 Hz tone at 0.3 of full scale.
    void fill_room_tone(std::vector<float>& block)
    {
        for (std::size_t i = 0; i < block.size(); ++i)
        {
            block[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f *
                                       static_cast<float>(i) / 48000.0f);
        }
    }
} // namespace

TEST(BinauralDecoder, ProcessPerformsNoHeapAllocation)
{
    // Sixteen blocks cross the 128-frame partition 32 times. See tests/nodes/room/README.md.
    const SushiDSP::room::HrtfSet set = SushiDSP::room::SphericalHeadHrtf::build(48000.0);
    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(set, 48000.0, 256);
    ASSERT_TRUE(decoder.ready());

    std::array<std::vector<float>, SushiDSP::room::BinauralDecoder::K_CHANNELS> channels;
    std::array<const float*, SushiDSP::room::BinauralDecoder::K_CHANNELS> inputs{};
    for (std::size_t channel = 0; channel < channels.size(); ++channel)
    {
        channels[channel].assign(256, 0.0f);
        fill_room_tone(channels[channel]);
        inputs[channel] = channels[channel].data();
    }
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 16; ++block)
    {
        decoder.process(inputs.data(), left.data(), right.data(), 256);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(left[i]));
        EXPECT_TRUE(std::isfinite(right[i]));
    }
}
