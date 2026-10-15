/**************************************************************************/
/* test_logger.cpp                                                        */
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
 * @file test_logger.cpp
 * @brief Tests the logger's runtime level filter, its sink ownership and the SD_LOG macros.
 * @author Mustafa Garip
 */

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/capturing_log_sink.hpp>
#include <SushiDSP/foundation/log/log.hpp>
#include <SushiDSP/foundation/log/scoped_log_sink.hpp>

namespace
{
    /// @brief Names the lines this file logs.
    inline constexpr SushiDSP::LogCategory K_LOGGER_TEST_LOG_CATEGORY =
        SushiDSP::make_log_category("logger_test");
} // namespace

TEST(Small_Logger, CapturingSinkReceivesTheCategoryTheLevelAndTheFormattedLine)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Trace);

    logger.log(SushiDSP::LogLevel::Warn, K_LOGGER_TEST_LOG_CATEGORY, "slot {} refused '{}'", 3,
               "cab.wav");

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiDSP::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "logger_test");
    EXPECT_EQ(lines[0].message, "slot 3 refused 'cab.wav'");
}

TEST(Small_Logger, LoggerDiscardsLinesBelowTheRuntimeMinimum)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Warn);

    logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, "info");
    logger.log(SushiDSP::LogLevel::Warn, K_LOGGER_TEST_LOG_CATEGORY, "warn");
    logger.log(SushiDSP::LogLevel::Error, K_LOGGER_TEST_LOG_CATEGORY, "error");
    EXPECT_FALSE(logger.is_enabled(SushiDSP::LogLevel::Info));
    EXPECT_TRUE(logger.is_enabled(SushiDSP::LogLevel::Warn));

    logger.set_minimum_level(SushiDSP::LogLevel::Info);
    logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, "info again");
    logger.log(SushiDSP::LogLevel::Debug, K_LOGGER_TEST_LOG_CATEGORY, "debug");

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0].message, "warn");
    EXPECT_EQ(lines[1].message, "error");
    EXPECT_EQ(lines[2].message, "info again");
}

TEST(Small_Logger, MacroBelowTheRuntimeMinimumDoesNotEvaluateItsArguments)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger& logger = SushiDSP::default_logger();
    const SushiDSP::LogLevel level_before = logger.minimum_level();
    int evaluations = 0;
    {
        const SushiDSP::ScopedLogSink guard(logger, capture);
        logger.set_minimum_level(SushiDSP::LogLevel::Error);

        SD_LOG_WARN(K_LOGGER_TEST_LOG_CATEGORY, "warn {}", ++evaluations);
        EXPECT_EQ(evaluations, 0);

        SD_LOG_ERROR(K_LOGGER_TEST_LOG_CATEGORY, "error {}", ++evaluations);
        EXPECT_EQ(evaluations, 1);
    }
    logger.set_minimum_level(level_before);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiDSP::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "logger_test");
    EXPECT_EQ(lines[0].message, "error 1");
}

TEST(Small_Logger, LoggerCutsAMessageAtTheMaximumLength)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Trace);
    const std::string twice_the_limit(2 * SushiDSP::K_MAX_LOG_MESSAGE_LENGTH, 'a');

    logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, "{}", twice_the_limit);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].message, std::string(SushiDSP::K_MAX_LOG_MESSAGE_LENGTH, 'a'));
}

TEST(Small_Logger, LoggerFlushesTheSinkAfterAFatalLine)
{
    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(capture, SushiDSP::LogLevel::Trace);

    logger.log(SushiDSP::LogLevel::Error, K_LOGGER_TEST_LOG_CATEGORY, "error");
    EXPECT_EQ(capture->flush_count(), 0u);

    logger.log(SushiDSP::LogLevel::Fatal, K_LOGGER_TEST_LOG_CATEGORY, "fatal");
    EXPECT_EQ(capture->flush_count(), 1u);
}

TEST(Small_Logger, ScopedSinkTakesTheLinesOfItsScopeAndThenRestoresTheOldSink)
{
    const auto outer = std::make_shared<SushiDSP::CapturingLogSink>();
    const auto inner = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(outer, SushiDSP::LogLevel::Trace);

    logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, "before");
    {
        const SushiDSP::ScopedLogSink guard(logger, inner);
        logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, "inside");
    }
    logger.log(SushiDSP::LogLevel::Info, K_LOGGER_TEST_LOG_CATEGORY, "after");

    const std::vector<SushiDSP::CapturedLogLine> outer_lines = outer->lines();
    const std::vector<SushiDSP::CapturedLogLine> inner_lines = inner->lines();
    ASSERT_EQ(outer_lines.size(), 2u);
    EXPECT_EQ(outer_lines[0].message, "before");
    EXPECT_EQ(outer_lines[1].message, "after");
    ASSERT_EQ(inner_lines.size(), 1u);
    EXPECT_EQ(inner_lines[0].message, "inside");
    EXPECT_EQ(inner->flush_count(), 1u);
    EXPECT_EQ(outer->flush_count(), 0u);
}
