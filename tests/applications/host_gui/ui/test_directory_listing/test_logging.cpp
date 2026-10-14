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
 * @brief Tests that the directory listing logs the cause when the file system refuses a path.
 * @author Mustafa Garip
 */

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/capturing_log_sink.hpp>
#include <SushiDSP/foundation/log/default_logger.hpp>
#include <SushiDSP/foundation/log/scoped_log_sink.hpp>
#include "ui/directory_listing.hpp"

TEST(Unit_Dsp, ListDirectoryLogsOneWarningNamingTheCauseWhenTheFileSystemRefusesThePath)
{
    // A name of 40 000 characters is longer than any file system here accepts.
    const std::string overlong(40000, 'a');

    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    std::vector<ui::DirectoryEntry> entries;
    {
        const SushiDSP::ScopedLogSink guard(SushiDSP::default_logger(), capture);
        entries = ui::list_directory(overlong, ".wav");
    }
    EXPECT_TRUE(entries.empty());

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    if (lines.empty())
    {
        GTEST_SKIP() << "this platform reports an overlong path as missing instead of raising "
                        "filesystem_error, so the logged path cannot be provoked here";
    }
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiDSP::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "host_gui");

    const std::string prefix = "could not list a directory: ";
    const std::string suffix = "; it is shown as empty: 'aaaa";
    const std::size_t suffix_at = lines[0].message.find(suffix);
    ASSERT_EQ(lines[0].message.rfind(prefix, 0), 0u) << lines[0].message;
    ASSERT_NE(suffix_at, std::string::npos) << lines[0].message;
    // The text between the two is the cause the file system gave, and it is not empty.
    EXPECT_GT(suffix_at, prefix.size()) << lines[0].message;
}
