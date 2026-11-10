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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/nodes/room/ambisonic_encoder.hpp>
#include <SushiDSP/nodes/room/binaural_decoder.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>

#include "support/axis_hrtf.hpp"

namespace
{
    /// @brief Counts the filters of a third-order decoder, one per ear and ambisonic channel.
    constexpr std::size_t K_DECODER_FILTERS = static_cast<std::size_t>(
        SushiDSP::room::BinauralDecoder::K_EARS) * SushiDSP::room::BinauralDecoder::K_CHANNELS;

    /// @brief Builds the six-axis set of axis_hrtf.hpp at one sample rate, with a silent
    /// third-order decoder, so a node can take it as its head and prepare a decoder from it.
    SushiDSP::room::HrtfSet decoder_axis_set(double sample_rate)
    {
        SushiDSP::room::HrtfSet set = SushiDSP::test::axis_hrtf(sample_rate);
        set.set_decoder(SushiDSP::room::AmbisonicEncoder::K_ORDER,
                        std::vector<float>(K_DECODER_FILTERS * SushiDSP::room::HrtfSet::K_DECODER_TAPS));
        return set;
    }

    /// @brief Returns a node prepared at one rate whose head is the six-axis set and whose room
    /// holds no cabinet, so prepare() builds neither the spherical head, which costs 2.1 s in a
    /// debug build, nor a pair of the starter room. The set goes in before prepare(), where
    /// set_hrtf() stores it without a log line.
    std::unique_ptr<SushiDSP::room::RoomNode> node_with_axis_head(double sample_rate)
    {
        auto node = std::make_unique<SushiDSP::room::RoomNode>();
        std::string reason;
        EXPECT_TRUE(node->set_arrangement(SushiDSP::Room::Arrangement(), reason)) << reason;
        EXPECT_EQ(node->set_hrtf(decoder_axis_set(sample_rate)),
                  SushiDSP::room::HrtfStatus::Installed);
        node->prepare(SushiDSP::PrepareInfo{sample_rate, 256, 1});
        return node;
    }
} // namespace

TEST(Medium_RoomNodeLogging, RoomNodeLogsOneErrorNamingTheCauseWhenTheDecoderCannotBePrepared)
{
    constexpr double K_SAMPLE_RATE = 48000.0;

    const std::unique_ptr<SushiDSP::room::RoomNode> node = node_with_axis_head(K_SAMPLE_RATE);

    SushiDSP::room::HrtfSet set = SushiDSP::test::axis_hrtf(K_SAMPLE_RATE);
    // A first-order decoder is one the node cannot use, so preparing the binaural decoder throws.
    set.set_decoder(1, std::vector<float>(2 * 4 * SushiDSP::room::HrtfSet::K_DECODER_TAPS));
    const std::string directions = std::to_string(set.direction_count());

    SushiDSP::LogCapture capture;
    EXPECT_EQ(node->set_hrtf(std::move(set)), SushiDSP::room::HrtfStatus::DecoderRefused);
    EXPECT_EQ(node->hrtf_source(), SushiDSP::room::HrtfSource::Measured);

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

TEST(Medium_RoomNodeLogging, RoomNodeLogsOneWarnNamingBothRatesWhenTheHrtfRateDiffers)
{
    constexpr double K_NODE_RATE = 44100.0;
    constexpr double K_SET_RATE = 48000.0;

    const std::unique_ptr<SushiDSP::room::RoomNode> node = node_with_axis_head(K_NODE_RATE);

    SushiDSP::room::HrtfSet set = SushiDSP::test::axis_hrtf(K_SET_RATE);

    SushiDSP::LogCapture capture;
    EXPECT_EQ(node->set_hrtf(std::move(set)), SushiDSP::room::HrtfStatus::SampleRateMismatch);
    EXPECT_EQ(node->hrtf_source(), SushiDSP::room::HrtfSource::Measured);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "room");
    EXPECT_NE(lines[0].message.find("48000 Hz for a node at 44100 Hz"), std::string::npos)
        << lines[0].message;
}
