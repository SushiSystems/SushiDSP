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
 * @brief Tests that SD_ENABLE_LOGGING 0 cuts every call below ERROR and keeps ERROR.
 * @author Mustafa Garip
 */

/// @brief Turns logging off in this translation unit, whatever the build type.
#define SD_ENABLE_LOGGING 0

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/capturing_log_sink.hpp>
#include <SushiDSP/foundation/log/log.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>
#include <SushiDSP/foundation/log/scoped_log_sink.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

namespace
{
    /// @brief Names the lines this file logs.
    inline constexpr SushiDSP::LogCategory K_CUT_TEST_LOG_CATEGORY =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "cut_test");
} // namespace

TEST(Small_Log, CallBelowErrorWithLoggingOffDoesNotEvaluateItsArgumentsAndErrorStillPasses)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger& logger = SushiDSP::default_logger();
    const SushiDSP::LogLevel level_before = logger.log_level(K_CUT_TEST_LOG_CATEGORY);
    int evaluations = 0;
    {
        const SushiDSP::ScopedLogSink guard(logger, capture);
        // The runtime level admits everything, so only the compile-time cut can stop a line.
        logger.set_log_level(K_CUT_TEST_LOG_CATEGORY, SushiDSP::LogLevel::Trace);

        SD_LOG_TRACE(K_CUT_TEST_LOG_CATEGORY, "trace {}", ++evaluations);
        SD_LOG_DEBUG(K_CUT_TEST_LOG_CATEGORY, "debug {}", ++evaluations);
        SD_LOG_INFO(K_CUT_TEST_LOG_CATEGORY, "info {}", ++evaluations);
        SD_LOG_WARN(K_CUT_TEST_LOG_CATEGORY, "warn {}", ++evaluations);
        EXPECT_EQ(evaluations, 0);
        EXPECT_TRUE(capture->lines().empty());

        SD_LOG_ERROR(K_CUT_TEST_LOG_CATEGORY, "error {}", ++evaluations);
        EXPECT_EQ(evaluations, 1);
    }
    logger.set_log_level(K_CUT_TEST_LOG_CATEGORY, level_before);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].message, "error 1");
}

TEST(Small_Log, LogCeilingIsOffWhenLoggingIsTurnedOff)
{
    EXPECT_EQ(SD_LOG_CEILING, SD_LOG_LEVEL_OFF);
}
