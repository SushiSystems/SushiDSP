/**************************************************************************/
/* test_default_logger.cpp                                                */
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
 * @file test_default_logger.cpp
 * @brief Tests that the process has one default logger.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include <SushiDSP/foundation/log/capturing_log_sink.hpp>
#include <SushiDSP/foundation/log/default_logger.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

TEST(Small_DefaultLogger, ReturnsTheSameLoggerEveryTime)
{
    EXPECT_EQ(&SushiDSP::default_logger(), &SushiDSP::default_logger());
}

TEST(Small_DefaultLogger, ProjectLevelSetsEveryCategoryWithoutALevelOfItsOwn)
{
    constexpr SushiDSP::LogCategory category =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "default_logger_project");
    const SushiDSP::LogLevel before = SushiDSP::log_level(SushiDSP::K_SUSHIDSP_LOG_PROJECT);

    SushiDSP::set_log_level(SushiDSP::K_SUSHIDSP_LOG_PROJECT, SushiDSP::LogLevel::Error);
    EXPECT_EQ(SushiDSP::log_level(SushiDSP::K_SUSHIDSP_LOG_PROJECT), SushiDSP::LogLevel::Error);
    EXPECT_EQ(SushiDSP::log_level(category), SushiDSP::LogLevel::Error);
    SushiDSP::set_log_level(SushiDSP::LogProject{"other", "OTHER_LOG_LEVEL"}, SushiDSP::LogLevel::Trace);
    EXPECT_EQ(SushiDSP::log_level(SushiDSP::K_SUSHIDSP_LOG_PROJECT), SushiDSP::LogLevel::Error);

    SushiDSP::set_log_level(SushiDSP::K_SUSHIDSP_LOG_PROJECT, before);
}

TEST(Small_DefaultLogger, CategoryLevelAndSinkReachTheLoggerOfTheProcess)
{
    constexpr SushiDSP::LogCategory category =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "default_logger_test");
    const SushiDSP::LogLevel level_before = SushiDSP::log_level(category);
    std::vector<std::string> texts;

    SushiDSP::set_log_level(category, SushiDSP::LogLevel::Debug);
    EXPECT_EQ(SushiDSP::log_level(category), SushiDSP::LogLevel::Debug);
    SushiDSP::set_log_sink(SushiDSP::LogSink(
        [&texts](const SushiDSP::LogRecord& record)
        {
            texts.emplace_back(record.text);
        }));
    SushiDSP::default_logger().log(SushiDSP::LogLevel::Debug, category,
                                   SushiDSP::LogSite{__FILE__, __LINE__}, "seen");
    SushiDSP::set_log_sink(SushiDSP::LogSink());
    SushiDSP::set_log_level(category, level_before);

    ASSERT_EQ(texts.size(), 1u);
    EXPECT_NE(texts[0].find("[DEBUG] [sushidsp.default_logger_test] [t"), std::string::npos)
        << texts[0];
    EXPECT_EQ(SushiDSP::default_logger().delivery(), SushiDSP::LogDelivery::Synchronous);
}

TEST(Small_DefaultLogger, ForwardedRecordPassesNoGateKeepsItsTimeAndIsWrittenHere)
{
    std::vector<SushiDSP::CapturedLogLine> seen;
    std::vector<std::int64_t> times;
    SushiDSP::set_log_sink(SushiDSP::LogSink(
        [&seen, &times](const SushiDSP::LogRecord& record)
        {
            seen.push_back(SushiDSP::CapturedLogLine{record.level, std::string(record.module),
                                                     std::string(record.message),
                                                     std::string(record.text)});
            times.push_back(record.time_ns);
        }));
    const SushiDSP::LogSink forward = SushiDSP::log_forward_sink();
    const SushiDSP::LogRecord sent{SushiDSP::LogLevel::Trace, "sushiruntime", "scheduler",
                                   "graph.cpp", 12, 99, 1'000'000'000, "ran", "their own line"};
    forward(sent);
    SushiDSP::set_log_sink(SushiDSP::LogSink());

    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0].level, SushiDSP::LogLevel::Trace);
    EXPECT_EQ(seen[0].message, "ran");
    EXPECT_EQ(times[0], 1'000'000'000);
    EXPECT_NE(seen[0].line.find("[TRACE] [sushiruntime.scheduler] [t"), std::string::npos)
        << seen[0].line;
    EXPECT_NE(seen[0].line.find("[graph.cpp:12] ran"), std::string::npos) << seen[0].line;
    EXPECT_EQ(seen[0].line.find("[t99]"), std::string::npos) << seen[0].line;
}