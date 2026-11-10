/**************************************************************************/
/* test_logging.cpp                                                       */
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
 * @file test_logging.cpp
 * @brief Tests that RoomNode logs the cause when it refuses an HRTF set it cannot use.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>

namespace
{
    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Names the number of axis directions the set below holds.
    constexpr std::uint32_t K_AXIS_COUNT = 6;

    /// @brief Builds the set of the six axis directions, +x, +y, +z, -x, -y, -z, whose taps are
    /// all zero, at one sample rate. The grid encloses the origin, so the set is valid.
    SushiDSP::room::HrtfSet six_axis_set(double sample_rate)
    {
        const auto half_pi = static_cast<float>(K_PI / 2.0);
        const auto pi = static_cast<float>(K_PI);
        std::vector<SushiDSP::room::HrtfSet::Direction> directions = {
            {0.0f, 0.0f, 0.0f},  {half_pi, 0.0f, 0.0f}, {0.0f, half_pi, 0.0f},
            {pi, 0.0f, 0.0f},    {-half_pi, 0.0f, 0.0f}, {0.0f, -half_pi, 0.0f}};
        const std::size_t taps = K_AXIS_COUNT * SushiDSP::room::HrtfSet::K_TAP_COUNT;
        return SushiDSP::room::HrtfSet(sample_rate, std::move(directions), std::vector<float>(taps),
                                       std::vector<float>(taps));
    }
} // namespace

TEST(Small_RoomNodeLogging, RoomNodeLogsOneErrorNamingTheCauseWhenTheDecoderCannotBePrepared)
{
    constexpr double K_SAMPLE_RATE = 48000.0;

    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, 256, 1});

    SushiDSP::room::HrtfSet set = six_axis_set(K_SAMPLE_RATE);
    // A first-order decoder is one the node cannot use, so preparing the binaural decoder throws.
    set.set_decoder(1, std::vector<float>(2 * 4 * SushiDSP::room::HrtfSet::K_DECODER_TAPS));
    const std::string directions = std::to_string(set.direction_count());

    SushiDSP::LogCapture capture;
    EXPECT_EQ(node.set_hrtf(std::move(set)), SushiDSP::room::HrtfStatus::DecoderRefused);
    EXPECT_EQ(node.hrtf_source(), SushiDSP::room::HrtfSource::SphericalHead);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "room");
    EXPECT_NE(lines[0].message.find("no third-order decoder"), std::string::npos)
        << lines[0].message;
    EXPECT_NE(lines[0].message.find(directions + " directions at 48000 Hz"), std::string::npos)
        << lines[0].message;
    EXPECT_NE(lines[0].message.find("the set is refused"), std::string::npos) << lines[0].message;
}

TEST(Small_RoomNodeLogging, RoomNodeLogsOneWarnNamingBothRatesWhenTheHrtfRateDiffers)
{
    constexpr double K_NODE_RATE = 44100.0;
    constexpr double K_SET_RATE = 48000.0;

    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{K_NODE_RATE, 256, 1});

    SushiDSP::room::HrtfSet set = six_axis_set(K_SET_RATE);

    SushiDSP::LogCapture capture;
    EXPECT_EQ(node.set_hrtf(std::move(set)), SushiDSP::room::HrtfStatus::SampleRateMismatch);
    EXPECT_EQ(node.hrtf_source(), SushiDSP::room::HrtfSource::SphericalHead);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "room");
    EXPECT_NE(lines[0].message.find("48000 Hz for a node at 44100 Hz"), std::string::npos)
        << lines[0].message;
}
