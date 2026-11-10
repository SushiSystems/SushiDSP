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
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/math/streaming_convolution.hpp>
#include <SushiDSP/nodes/room/ambisonic_encoder.hpp>
#include <SushiDSP/nodes/room/binaural_decoder.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

namespace
{
    /// @brief Holds pi.
    constexpr float K_PI = 3.14159265358979323846f;

    /// @brief Holds the sample rate every test runs at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Holds the block size every test feeds process().
    constexpr std::uint32_t K_BLOCK = 64;

    /// @brief Counts the Ambisonic channels.
    constexpr std::uint32_t K_CHANNELS = SushiDSP::room::AmbisonicEncoder::K_CHANNELS;

    /// @brief Counts the decoder filters, left ear then right.
    constexpr std::uint32_t K_FILTERS = 2u * K_CHANNELS;

    /// @brief Holds one Ambisonic channel.
    struct ChannelRow
    {
            const char* name;
            std::uint32_t channel;
    };

    /// @brief Names a row for gtest.
    std::string channel_row_name(const ::testing::TestParamInfo<ChannelRow>& info)
    {
        return info.param.name;
    }

    /// @brief Returns the channel numbers zero to fifteen.
    std::vector<std::uint32_t> all_channels()
    {
        std::vector<std::uint32_t> channels(K_CHANNELS);
        std::iota(channels.begin(), channels.end(), 0u);
        return channels;
    }

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

    /// @brief Renders the impulse of @p channels through a fresh decoder on @p set and returns
    /// the ear outputs and the decoder's latency.
    std::uint32_t render_through_fresh_decoder(const SushiDSP::room::HrtfSet& set,
                                               const std::vector<std::uint32_t>& channels,
                                               std::vector<float>& left, std::vector<float>& right)
    {
        SushiDSP::room::BinauralDecoder decoder;
        decoder.prepare(set, K_SAMPLE_RATE, K_BLOCK);
        render_impulse(decoder, channels, 1024, left, right);
        return decoder.latency_frames();
    }

    /// @brief Expects @p samples to be zero in every frame but @p onset, which holds @p level.
    void expect_single_impulse(const std::vector<float>& samples, std::size_t onset, float level)
    {
        for (std::size_t frame = 0; frame < samples.size(); ++frame)
        {
            EXPECT_EQ(samples[frame], frame == onset ? level : 0.0f) << "frame " << frame;
        }
    }

    /// @brief Expects every sample of @p samples to be zero.
    void expect_silent(const std::vector<float>& samples)
    {
        for (std::size_t frame = 0; frame < samples.size(); ++frame)
        {
            EXPECT_EQ(samples[frame], 0.0f) << "frame " << frame;
        }
    }

    /// @brief Expects every sample of @p samples to be finite.
    void expect_all_finite(const std::vector<float>& samples)
    {
        for (const float sample : samples)
        {
            EXPECT_TRUE(std::isfinite(sample));
        }
    }

    /// @brief Holds sixteen channels of a 220 Hz tone at 0.3 of full scale and the pointer table
    /// over them.
    struct RoomToneBlock
    {
            /// @brief Stores the channels, 256 frames each.
            std::array<std::vector<float>, K_CHANNELS> channels;

            /// @brief Stores one pointer per channel.
            std::array<const float*, K_CHANNELS> inputs{};

            /// @brief Fills every channel with the tone.
            RoomToneBlock()
            {
                for (std::size_t channel = 0; channel < channels.size(); ++channel)
                {
                    channels[channel].assign(256, 0.0f);
                    for (std::size_t i = 0; i < channels[channel].size(); ++i)
                    {
                        channels[channel][i] =
                            0.3f * std::sin(2.0f * K_PI * 220.0f * static_cast<float>(i) / 48000.0f);
                    }
                    inputs[channel] = channels[channel].data();
                }
            }
    };

    /// @brief Processes @p blocks blocks of 256 frames of @p tone through @p decoder.
    void process_blocks(SushiDSP::room::BinauralDecoder& decoder, const RoomToneBlock& tone,
                        std::vector<float>& left, std::vector<float>& right, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
        {
            decoder.process(tone.inputs.data(), left.data(), right.data(), 256);
        }
    }

    class Small_BinauralDecoderChannel : public ::testing::TestWithParam<ChannelRow>
    {
    };
} // namespace

TEST(Small_BinauralDecoder, BinauralDecoderWithDeltaFiltersPassesChannelZeroThroughAndLeavesTheRightEarSilent)
{
    std::vector<float> left;
    std::vector<float> right;

    const std::uint32_t onset = render_through_fresh_decoder(delta_set({0}), {0}, left, right);

    expect_single_impulse(left, onset, 1.0f);
    expect_silent(right);
}

TEST(Small_BinauralDecoder, BinauralDecoderReportsOnePartitionOfLatency)
{
    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(delta_set({0}), K_SAMPLE_RATE, K_BLOCK);
    EXPECT_EQ(decoder.latency_frames(),
              SushiDSP::StreamingConvolution::partition_size_for(K_SAMPLE_RATE));
    EXPECT_EQ(decoder.latency_frames(), 128u);
}

TEST_P(Small_BinauralDecoderChannel, BinauralDecoderSendsEachChannelIntoTheLeftEar)
{
    std::vector<float> left;
    std::vector<float> right;

    const std::uint32_t onset =
        render_through_fresh_decoder(delta_set(all_channels()), {GetParam().channel}, left, right);

    EXPECT_EQ(left[onset], 1.0f);
}

INSTANTIATE_TEST_SUITE_P(Channels, Small_BinauralDecoderChannel,
                         ::testing::Values(ChannelRow{"Channel0", 0u}, ChannelRow{"Channel1", 1u},
                                           ChannelRow{"Channel2", 2u}, ChannelRow{"Channel3", 3u},
                                           ChannelRow{"Channel4", 4u}, ChannelRow{"Channel5", 5u},
                                           ChannelRow{"Channel6", 6u}, ChannelRow{"Channel7", 7u},
                                           ChannelRow{"Channel8", 8u}, ChannelRow{"Channel9", 9u},
                                           ChannelRow{"Channel10", 10u},
                                           ChannelRow{"Channel11", 11u},
                                           ChannelRow{"Channel12", 12u},
                                           ChannelRow{"Channel13", 13u},
                                           ChannelRow{"Channel14", 14u},
                                           ChannelRow{"Channel15", 15u}),
                         channel_row_name);

TEST(Small_BinauralDecoder, BinauralDecoderSumsSixteenChannelsIntoEachEar)
{
    std::vector<float> left;
    std::vector<float> right;

    const std::uint32_t onset =
        render_through_fresh_decoder(delta_set(all_channels()), all_channels(), left, right);

    EXPECT_EQ(left[onset], 16.0f);
    EXPECT_EQ(right[onset], 0.0f);
}

TEST(Small_BinauralDecoder, BinauralDecoderRefusesASetWithNoDecoderAndStaysNotReady)
{
    SushiDSP::room::BinauralDecoder decoder;
    EXPECT_THROW(decoder.prepare(SushiDSP::room::HrtfSet{}, K_SAMPLE_RATE, 256u),
                 std::runtime_error);
    EXPECT_FALSE(decoder.ready());
    EXPECT_EQ(decoder.latency_frames(), 0u);
}

TEST(Small_BinauralDecoder, BinauralDecoderResetClearsTheHistory)
{
    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(delta_set({0}), K_SAMPLE_RATE, K_BLOCK);
    std::vector<float> left;
    std::vector<float> right;
    render_impulse(decoder, {0}, K_BLOCK, left, right);
    decoder.reset();

    render_impulse(decoder, {}, 2048, left, right);

    expect_silent(left);
    expect_silent(right);
}

TEST(Medium_BinauralDecoder, ProcessPerformsNoHeapAllocation)
{
    // Sixteen blocks cross the 128-frame partition 32 times. See tests/nodes/room/README.md.
    const SushiDSP::room::HrtfSet set = SushiDSP::room::SphericalHeadHrtf::build(48000.0);
    SushiDSP::room::BinauralDecoder decoder;
    decoder.prepare(set, 48000.0, 256);
    ASSERT_TRUE(decoder.ready());
    const RoomToneBlock tone;
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    process_blocks(decoder, tone, left, right, 16);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    expect_all_finite(left);
    expect_all_finite(right);
}
