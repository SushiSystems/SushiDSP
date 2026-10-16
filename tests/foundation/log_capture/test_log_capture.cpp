/**************************************************************************/
/* test_log_capture.cpp                                                   */
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
 * @file test_log_capture.cpp
 * @brief Tests the scope that collects every SushiLog record.
 * @author Mustafa Garip
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>

namespace
{
    /// @brief Names the records this file logs.
    constexpr SushiLog::LogCategory K_CAPTURE_TEST_LOG_CATEGORY =
        SushiLog::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "log_capture_test");
} // namespace

TEST(Small_LogCapture, CollectsEachRecordWithItsLevelModuleMessageAndLine)
{
    SushiDSP::LogCapture capture;
    SD_LOG_ERROR(K_CAPTURE_TEST_LOG_CATEGORY, "slot {} refused", 3);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "log_capture_test");
    EXPECT_EQ(lines[0].message, "slot 3 refused");
    EXPECT_NE(lines[0].line.find("[ERROR] [sushidsp.log_capture_test] [t"), std::string::npos)
        << lines[0].line;
}