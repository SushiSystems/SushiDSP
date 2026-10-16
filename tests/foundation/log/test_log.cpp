/**************************************************************************/
/* test_log.cpp                                                           */
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
 * @file test_log.cpp
 * @brief Tests the SD_LOG front over SushiLog.
 * @author Mustafa Garip
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiLog/log/log_calls.hpp>

namespace
{
    /// @brief Names the records this file logs.
    constexpr SushiLog::LogCategory K_FRONT_TEST_LOG_CATEGORY =
        SushiLog::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "front_test");

    /// @brief Counts how often an argument is evaluated.
    int evaluated = 0;

    /// @brief Returns its argument and counts the call.
    int counted(int value)
    {
        ++evaluated;
        return value;
    }
} // namespace

TEST(Small_Log, CeilingIsDebugUnderNdebugAndTraceOtherwise)
{
#if defined(NDEBUG)
    EXPECT_EQ(SushiDSP::K_SUSHIDSP_LOG_CEILING, SushiLog::LogLevel::Debug);
#else
    EXPECT_EQ(SushiDSP::K_SUSHIDSP_LOG_CEILING, SushiLog::LogLevel::Trace);
#endif
}

TEST(Small_Log, CallBelowTheCategoryLevelEvaluatesNoArgumentAndOneAboveIsDelivered)
{
    SushiDSP::LogCapture capture;
    evaluated = 0;
    SD_LOG_INFO(K_FRONT_TEST_LOG_CATEGORY, "hidden {}", counted(1));
    SD_LOG_WARN(K_FRONT_TEST_LOG_CATEGORY, "shown {}", counted(2));

    EXPECT_EQ(evaluated, 1);
    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].message, "shown 2");
}

TEST(Small_Log, LevelSetForTheCategoryLetsItsDebugCallsThrough)
{
    SushiDSP::LogCapture capture;
    SushiLog::set_log_level(K_FRONT_TEST_LOG_CATEGORY, SushiLog::LogLevel::Debug);
    SD_LOG_DEBUG(K_FRONT_TEST_LOG_CATEGORY, "state {}", 7);
    SushiLog::set_log_level(K_FRONT_TEST_LOG_CATEGORY, SushiLog::LogLevel::Warn);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Debug);
}