/**************************************************************************/
/* test_log_line.cpp                                                      */
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
 * @file test_log_line.cpp
 * @brief Tests the shape of a record line: time, level, category, thread, site and message.
 * @author Mustafa Garip
 */

#include <chrono>
#include <ctime>
#include <regex>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log_line.hpp>
#include <SushiDSP/foundation/log/log_record.hpp>

namespace
{
    /// @brief Returns the fields of a WARN line from room on thread 3 at a.cpp:88.
    SushiDSP::LogLineFields fields_with(std::chrono::system_clock::time_point time,
                                        std::string_view message)
    {
        return SushiDSP::LogLineFields{time, SushiDSP::LogLevel::Warn, "sushidsp", "room", 3,
                                       "a.cpp", 88, message};
    }
} // namespace

TEST(Small_LogLine, LineHasTheSixFieldsInOrder)
{
    char buffer[SushiDSP::K_MAX_LOG_LINE_LENGTH];
    const std::size_t length = SushiDSP::format_log_line(
        buffer, sizeof(buffer), fields_with(std::chrono::system_clock::now(), "share refused"));

    const std::string line(buffer, length);
    const std::regex shape(R"(\[\d\d:\d\d:\d\d\.\d\d\d\] \[WARN\] \[sushidsp\.room\] \[t3\] \[a\.cpp:88\] share refused)");
    EXPECT_TRUE(std::regex_match(line, shape)) << line;
}

TEST(Small_LogLine, TimeIsTheLocalClockToTheMillisecond)
{
    std::tm noon{};
    noon.tm_year = 126;
    noon.tm_mon = 9;
    noon.tm_mday = 16;
    noon.tm_hour = 12;
    noon.tm_min = 4;
    noon.tm_sec = 31;
    noon.tm_isdst = -1;
    const auto time = std::chrono::system_clock::from_time_t(std::mktime(&noon)) +
                      std::chrono::milliseconds(207);

    char buffer[SushiDSP::K_MAX_LOG_LINE_LENGTH];
    const std::size_t length =
        SushiDSP::format_log_line(buffer, sizeof(buffer), fields_with(time, "x"));

    EXPECT_EQ(std::string(buffer, length).substr(0, 14), "[12:04:31.207]");
}

TEST(Small_LogLine, MessageIsWholeAtTheEndOfTheLine)
{
    const std::string message(SushiDSP::K_MAX_LOG_MESSAGE_LENGTH, 'm');
    char buffer[SushiDSP::K_MAX_LOG_LINE_LENGTH];
    const std::size_t length = SushiDSP::format_log_line(
        buffer, sizeof(buffer), fields_with(std::chrono::system_clock::now(), message));

    ASSERT_GT(length, message.size());
    EXPECT_EQ(std::string(buffer + length - message.size(), message.size()), message);
}
