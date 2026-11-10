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
 * @brief Tests that the directory listing logs the cause when the file system refuses a path.
 * @author Mustafa Garip
 */

#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include "ui/directory_listing.hpp"

namespace
{
    /// @brief The directory name the source refuses.
    const std::string K_REFUSED_DIRECTORY = "refused";

    /// @brief The cause the source refuses it with.
    const std::error_code K_REFUSAL = std::make_error_code(std::errc::filename_too_long);
} // namespace

TEST(Small_DirectoryListing, ListDirectoryShowsARefusedPathAsEmpty)
{
    ui::InMemoryDirectorySource source;
    source.refuse(K_REFUSED_DIRECTORY, K_REFUSAL);
    SushiDSP::LogCapture capture;

    const std::vector<ui::DirectoryEntry> entries =
        ui::list_directory(K_REFUSED_DIRECTORY, std::vector<std::string>{".wav"}, source);

    EXPECT_TRUE(entries.empty());
}

TEST(Small_DirectoryListing, ListDirectoryLogsOneWarningNamingTheCauseWhenTheFileSystemRefusesThePath)
{
    ui::InMemoryDirectorySource source;
    source.refuse(K_REFUSED_DIRECTORY, K_REFUSAL);
    SushiDSP::LogCapture capture;

    static_cast<void>(ui::list_directory(K_REFUSED_DIRECTORY, std::vector<std::string>{".wav"}, source));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "host_gui");
    EXPECT_EQ(lines[0].message, "could not list a directory: " + K_REFUSAL.message() +
                                    "; it is shown as empty: '" + K_REFUSED_DIRECTORY + "'");
}
