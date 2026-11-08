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
 * @file test_audio_device.cpp
 * @brief Tests the promises every AudioDevice keeps, once per implementation.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <memory>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_device.hpp>
#include <SushiDSP/foundation/core/in_memory_audio_device.hpp>

namespace
{
    /// @brief Makes and drives the in-memory device for the contract.
    struct InMemoryAudioDeviceSubject
    {
            /// @brief Makes a stopped device of @p format.
            static std::unique_ptr<SushiDSP::InMemoryAudioDevice> make(SushiDSP::DeviceFormat format)
            {
                return std::make_unique<SushiDSP::InMemoryAudioDevice>(format);
            }

            /// @brief Runs @p blocks blocks of @p device.
            static void drive(SushiDSP::InMemoryAudioDevice& device, std::uint32_t blocks)
            {
                device.render(blocks);
            }
    };

    /// @brief What one callback saw.
    struct CallbackRecord
    {
            /// @brief Counts the calls.
            std::uint32_t calls = 0;
            /// @brief Holds the frame count of the last call.
            std::uint32_t frames = 0;
            /// @brief Holds whether the last call's inputs were null.
            bool inputs_null = false;
            /// @brief Holds whether the last call's first two output buffers were both non-null.
            bool two_outputs_present = false;
    };

    /// @brief Returns a callback that writes what it sees into @p record.
    SushiDSP::DeviceCallback recording_callback(CallbackRecord& record)
    {
        return [&record](const float* const* inputs, float* const* outputs, std::uint32_t frames)
        {
            ++record.calls;
            record.frames = frames;
            record.inputs_null = inputs == nullptr;
            record.two_outputs_present = outputs[0] != nullptr && outputs[1] != nullptr;
        };
    }

    const SushiDSP::DeviceFormat K_STEREO_OUT_FORMAT{48000.0, 128, 0, 2};

    template <typename Subject> class Small_AudioDeviceContract : public ::testing::Test
    {
    };

    using AudioDeviceSubjects = ::testing::Types<InMemoryAudioDeviceSubject>;
} // namespace

TYPED_TEST_SUITE(Small_AudioDeviceContract, AudioDeviceSubjects);

TYPED_TEST(Small_AudioDeviceContract, ReportsTheFormatItWasMadeWith)
{
    const auto device = TypeParam::make({44100.0, 256, 1, 2});
    const SushiDSP::DeviceFormat format = device->format();
    EXPECT_EQ(format.sample_rate, 44100.0);
    EXPECT_EQ(format.block_size, 256u);
    EXPECT_EQ(format.input_channels, 1u);
    EXPECT_EQ(format.output_channels, 2u);
}

TYPED_TEST(Small_AudioDeviceContract, CallsTheCallbackOncePerBlock)
{
    auto device = TypeParam::make(K_STEREO_OUT_FORMAT);
    CallbackRecord record;
    device->start(recording_callback(record));
    TypeParam::drive(*device, 5);
    EXPECT_EQ(record.calls, 5u);
}

TYPED_TEST(Small_AudioDeviceContract, HandsTheCallbackBlockSizeFrames)
{
    auto device = TypeParam::make(K_STEREO_OUT_FORMAT);
    CallbackRecord record;
    device->start(recording_callback(record));
    TypeParam::drive(*device, 1);
    EXPECT_EQ(record.frames, 128u);
}

TYPED_TEST(Small_AudioDeviceContract, HandsTheCallbackOneBufferPerOutputChannel)
{
    auto device = TypeParam::make(K_STEREO_OUT_FORMAT);
    CallbackRecord record;
    device->start(recording_callback(record));
    TypeParam::drive(*device, 1);
    EXPECT_TRUE(record.two_outputs_present);
}

TYPED_TEST(Small_AudioDeviceContract, PassesNullInputsWhenTheFormatHasNone)
{
    auto device = TypeParam::make(K_STEREO_OUT_FORMAT);
    CallbackRecord record;
    device->start(recording_callback(record));
    TypeParam::drive(*device, 1);
    EXPECT_TRUE(record.inputs_null);
}

TYPED_TEST(Small_AudioDeviceContract, CallsNoCallbackAfterStop)
{
    auto device = TypeParam::make(K_STEREO_OUT_FORMAT);
    CallbackRecord record;
    device->start(recording_callback(record));
    device->stop();
    TypeParam::drive(*device, 3);
    EXPECT_EQ(record.calls, 0u);
}
