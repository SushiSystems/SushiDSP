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
 * @file test_logging.cpp
 * @brief Tests that TrackRecorder logs the cause when it cannot open its take file.
 * @author Mustafa Garip
 */

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/capture/track_recorder.hpp>
#include <SushiDSP/foundation/log/capturing_log_sink.hpp>
#include <SushiDSP/foundation/log/default_logger.hpp>
#include <SushiDSP/foundation/log/scoped_log_sink.hpp>

TEST(Unit_Dsp, TrackRecorderLogsOneErrorNamingThePathWhenTheTakeFileCannotBeOpened)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "sushidsp_recorder_logging";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    // No directory named "missing" exists under root, so the take file cannot be created.
    const std::string path = (root / "missing" / "take.wav").string();

    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    {
        const SushiDSP::ScopedLogSink guard(SushiDSP::default_logger(), capture);
        SushiDSP::TrackRecorder recorder(1u, 48000.0, 8192);
        EXPECT_FALSE(recorder.arm(path));
        EXPECT_FALSE(recorder.is_recording());
    }

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiDSP::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "capture");
    EXPECT_NE(lines[0].message.find(path), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("could not open"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("the recorder stays disarmed"), std::string::npos)
        << lines[0].message;

    std::filesystem::remove_all(root);
}
