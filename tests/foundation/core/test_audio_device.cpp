/**************************************************************************/
/* test_audio_device.cpp                                                  */
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
 * @file test_audio_device.cpp
 * @brief Tests that NullAudioDevice reports its format and invokes its callback per block.
 * @author Mustafa Garip
 */

#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_device.hpp>

namespace
{
    class NullAudioDevice : public SushiDSP::AudioDevice
    {
        public:
            NullAudioDevice(SushiDSP::DeviceFormat format, std::uint32_t blocks_to_run)
                : format_(format), blocks_to_run_(blocks_to_run)
            {
            }

            SushiDSP::DeviceFormat format() const override
            {
                return format_;
            }

            void start(SushiDSP::DeviceCallback callback) override
            {
                std::vector<std::vector<float>> channel_storage(
                    format_.output_channels, std::vector<float>(format_.block_size, 0.0f));
                std::vector<float*> channel_pointers(format_.output_channels);
                for (std::uint32_t channel = 0; channel < format_.output_channels; ++channel)
                {
                    channel_pointers[channel] = channel_storage[channel].data();
                }

                for (std::uint32_t block = 0; block < blocks_to_run_; ++block)
                {
                    callback(nullptr, channel_pointers.data(), format_.block_size);
                }
            }

            void stop() override {}

        private:
            SushiDSP::DeviceFormat format_;
            std::uint32_t blocks_to_run_;
    };
} // namespace

TEST(Small_AudioDevice, NullAudioDeviceReportsConfiguredFormat)
{
    SushiDSP::DeviceFormat expected{48000.0, 256, 0, 1};
    NullAudioDevice device(expected, 0);

    SushiDSP::DeviceFormat actual = device.format();

    EXPECT_EQ(actual.sample_rate, expected.sample_rate);
    EXPECT_EQ(actual.block_size, expected.block_size);
    EXPECT_EQ(actual.input_channels, expected.input_channels);
    EXPECT_EQ(actual.output_channels, expected.output_channels);
}

TEST(Small_AudioDevice, NullAudioDeviceInvokesCallbackOncePerBlock)
{
    NullAudioDevice device({48000.0, 256, 0, 1}, 5);

    int call_count = 0;
    device.start(
        [&call_count](const float* const*, float* const*, std::uint32_t)
        {
            ++call_count;
        });

    EXPECT_EQ(call_count, 5);
}

TEST(Small_AudioDevice, NullAudioDeviceCallbackReceivesRequestedFrameCount)
{
    NullAudioDevice device({48000.0, 128, 0, 2}, 1);

    std::uint32_t received_frames = 0;
    std::uint32_t received_channels_nonnull = 0;
    device.start(
        [&received_frames, &received_channels_nonnull](const float* const*, float* const* outputs,
                                                       std::uint32_t frames)
        {
            received_frames = frames;
            received_channels_nonnull = (outputs[0] != nullptr && outputs[1] != nullptr) ? 2 : 0;
        });

    EXPECT_EQ(received_frames, 128u);
    EXPECT_EQ(received_channels_nonnull, 2u);
}
