/**************************************************************************/
/* test_room_node_logging.cpp                                             */
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
 * @file test_room_node_logging.cpp
 * @brief Tests that RoomNode logs the cause when it refuses an HRTF set it cannot decode.
 * @author Mustafa Garip
 */

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/log/CapturingLogSink.hpp"
#include "SushiDSP/log/DefaultLogger.hpp"
#include "SushiDSP/log/ScopedLogSink.hpp"
#include "SushiDSP/room/HrtfSet.hpp"
#include "SushiDSP/room/HrtfSetLoader.hpp"
#include "SushiDSP/room/RoomNode.hpp"

TEST(Unit_Dsp, RoomNodeLogsOneErrorNamingTheCauseWhenTheDecoderCannotBePrepared)
{
    constexpr double kSampleRate = 48000.0;

    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{ kSampleRate, 256, 1 });

    SushiDSP::room::HrtfSet set;
    std::string             error;
    ASSERT_TRUE(SushiDSP::room::HrtfSetLoader::load(
        std::string(SUSHIDSP_FIXTURE_DIR) + "/hrtf_self_test.hrtf", kSampleRate, set, error))
        << error;
    // A first-order decoder is one the node cannot use, so preparing the binaural decoder throws.
    set.set_decoder(1, std::vector<float>(2 * 4 * SushiDSP::room::HrtfSet::kDecoderTaps));
    const std::string directions = std::to_string(set.direction_count());

    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    {
        const SushiDSP::ScopedLogSink guard(SushiDSP::default_logger(), capture);
        EXPECT_FALSE(node.set_hrtf(std::move(set)));
    }
    EXPECT_EQ(node.hrtf_source(), SushiDSP::room::HrtfSource::SphericalHead);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiDSP::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "room");
    EXPECT_NE(lines[0].message.find("no third-order decoder"), std::string::npos)
        << lines[0].message;
    EXPECT_NE(lines[0].message.find(directions + " directions at 48000 Hz"), std::string::npos)
        << lines[0].message;
    EXPECT_NE(lines[0].message.find("the set is refused"), std::string::npos) << lines[0].message;
}
