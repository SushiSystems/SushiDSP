/**************************************************************************/
/* test_sushilog_capture_log.cpp                                          */
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
 * @file test_sushilog_capture_log.cpp
 * @brief Tests SushiLogCaptureLog, which writes each TrackRecorder failure as a SushiLog ERROR.
 * @author Mustafa Garip
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/nodes/capture/sushilog_capture_log.hpp>

namespace
{
    /// @brief Writes @p failure through a SushiLogCaptureLog and returns the lines it wrote.
    std::vector<SushiDSP::CapturedLogLine> lines_written_for(const SushiDSP::CaptureFailure& failure)
    {
        SushiDSP::LogCapture capture;
        SushiDSP::SushiLogCaptureLog log;

        log.report_failure(failure);

        return capture.lines();
    }
} // namespace

TEST(Small_SushiLogCaptureLog, AnUnopenedTakeFileWritesOneErrorNamingThePath)
{
    const std::vector<SushiDSP::CapturedLogLine> lines = lines_written_for(
        {SushiDSP::CaptureFailureCause::TakeFileNotOpened, "folder/take.wav", "denied", 0, 0});

    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "capture");
    EXPECT_NE(lines[0].message.find("folder/take.wav"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("could not open"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("denied"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("the recorder stays disarmed"), std::string::npos)
        << lines[0].message;
}

TEST(Small_SushiLogCaptureLog, AnUnstartedWriterThreadWritesOneErrorNamingThePath)
{
    const std::vector<SushiDSP::CapturedLogLine> lines = lines_written_for(
        {SushiDSP::CaptureFailureCause::WriterThreadNotStarted, "take.wav", "no threads", 0, 0});

    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_NE(lines[0].message.find("take.wav"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("writer thread"), std::string::npos) << lines[0].message;
}

TEST(Small_SushiLogCaptureLog, AFailedAppendWritesOneErrorNamingBothFrameCounts)
{
    const std::vector<SushiDSP::CapturedLogLine> lines = lines_written_for(
        {SushiDSP::CaptureFailureCause::AppendFailed, "", "disk full", 4096, 8192});

    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_NE(lines[0].message.find("4096"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("8192"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("disk full"), std::string::npos) << lines[0].message;
}
