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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/io/in_memory_audio_file_writer.hpp>
#include <SushiDSP/nodes/capture/in_memory_capture_log.hpp>
#include <SushiDSP/nodes/capture/track_recorder.hpp>

TEST(Small_TrackRecorder, ReportsOneFailureNamingThePathWhenTheTakeFileCannotBeOpened)
{
    const std::string path = "folder/missing/take.wav";
    SushiDSP::InMemoryAudioFileStore store;
    store.refuse(path);
    SushiDSP::InMemoryCaptureLog log;

    {
        SushiDSP::TrackRecorder recorder(1u, 48000.0, 8192, log, store.opener());
        EXPECT_FALSE(recorder.arm(path));
        EXPECT_FALSE(recorder.is_recording());
    }

    const std::vector<SushiDSP::CaptureFailure> failures = log.failures();
    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures[0].cause, SushiDSP::CaptureFailureCause::TakeFileNotOpened);
    EXPECT_EQ(failures[0].path, path);
    EXPECT_FALSE(failures[0].detail.empty());
}
