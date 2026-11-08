/**************************************************************************/
/* test_in_memory_audio_device.cpp                                        */
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
 * @file test_in_memory_audio_device.cpp
 * @brief Tests what the in-memory AudioDevice adds to the port: its inputs, its outputs and render().
 * @author Mustafa Garip
 */

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/in_memory_audio_device.hpp>

namespace
{
    /// @brief Returns a callback that writes the first sample of input 0, plus @p offset, to every output frame.
    SushiDSP::DeviceCallback echo_input_plus(float offset)
    {
        return [offset](const float* const* inputs, float* const* outputs, std::uint32_t frames)
        {
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                outputs[0][frame] += inputs[0][0] + offset;
            }
        };
    }
} // namespace

TEST(Small_InMemoryAudioDevice, AnInputChannelCarriesTheLevelSetForIt)
{
    SushiDSP::InMemoryAudioDevice device({48000.0, 4, 1, 1});
    device.set_input_level(0, 0.5f);
    device.start(echo_input_plus(0.0f));
    device.render(1);
    EXPECT_EQ(device.output(0), std::vector<float>(4, 0.5f));
}

TEST(Small_InMemoryAudioDevice, EachBlockStartsFromSilentOutputs)
{
    SushiDSP::InMemoryAudioDevice device({48000.0, 4, 1, 1});
    device.start(echo_input_plus(0.25f));
    device.render(3);
    EXPECT_EQ(device.output(0), std::vector<float>(4, 0.25f));
}

TEST(Small_InMemoryAudioDevice, RenderReportsTheBlocksItRan)
{
    SushiDSP::InMemoryAudioDevice device({48000.0, 4, 0, 1});
    device.start([](const float* const*, float* const*, std::uint32_t) {});
    EXPECT_EQ(device.render(3), 3u);
}

TEST(Small_InMemoryAudioDevice, RenderWhileStoppedRunsNoBlock)
{
    SushiDSP::InMemoryAudioDevice device({48000.0, 4, 0, 1});
    EXPECT_EQ(device.render(3), 0u);
}

TEST(Small_InMemoryAudioDevice, IsRunningBetweenStartAndStop)
{
    SushiDSP::InMemoryAudioDevice device({48000.0, 4, 0, 1});
    device.start([](const float* const*, float* const*, std::uint32_t) {});
    EXPECT_TRUE(device.is_running());
}

TEST(Small_InMemoryAudioDevice, IsNotRunningAfterStop)
{
    SushiDSP::InMemoryAudioDevice device({48000.0, 4, 0, 1});
    device.start([](const float* const*, float* const*, std::uint32_t) {});
    device.stop();
    EXPECT_FALSE(device.is_running());
}
