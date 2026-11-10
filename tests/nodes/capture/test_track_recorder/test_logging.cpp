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
 * @brief Tests that TrackRecorder logs the cause when it cannot open its take file.
 * @author Mustafa Garip
 */

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/io/in_memory_audio_file_writer.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/nodes/capture/track_recorder.hpp>

TEST(Small_TrackRecorder, LogsOneErrorNamingThePathWhenTheTakeFileCannotBeOpened)
{
    const std::string path = "folder/missing/take.wav";
    SushiDSP::InMemoryAudioFileStore store;
    store.refuse(path);

    SushiDSP::LogCapture capture;
    {
        SushiDSP::TrackRecorder recorder(1u, 48000.0, 8192, store.opener());
        EXPECT_FALSE(recorder.arm(path));
        EXPECT_FALSE(recorder.is_recording());
    }

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "capture");
    EXPECT_NE(lines[0].message.find(path), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("could not open"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("the recorder stays disarmed"), std::string::npos)
        << lines[0].message;
}
