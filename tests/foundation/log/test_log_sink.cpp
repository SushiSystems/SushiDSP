/**************************************************************************/
/* test_log_sink.cpp                                                      */
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
 * @file test_log_sink.cpp
 * @brief Tests that a logger hands its lines to the sink it was given.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log.hpp>
#include <SushiDSP/foundation/log/logger.hpp>

#include <memory>

namespace
{
    class CountingSink final : public SushiDSP::LogSink
    {
        public:
            void write(const SushiDSP::LogRecord&) noexcept override
            {
                ++writes;
            }

            void flush() noexcept override
            {
                ++flushes;
            }

            int writes = 0;
            int flushes = 0;
    };

    const SushiDSP::LogCategory K_CATEGORY = SushiDSP::make_log_category("sink_test");
} // namespace

TEST(Small_LogSink, LoggerWritesEachEnabledLineOnce)
{
    const auto sink = std::make_shared<CountingSink>();
    SushiDSP::Logger logger(sink, SushiDSP::LogLevel::Info);
    logger.log(SushiDSP::LogLevel::Info, K_CATEGORY, "one");
    logger.log(SushiDSP::LogLevel::Warn, K_CATEGORY, "two");
    EXPECT_EQ(sink->writes, 2);
}

TEST(Small_LogSink, LoggerDiscardsLinesBelowItsMinimumLevel)
{
    const auto sink = std::make_shared<CountingSink>();
    SushiDSP::Logger logger(sink, SushiDSP::LogLevel::Warn);
    logger.log(SushiDSP::LogLevel::Debug, K_CATEGORY, "quiet");
    EXPECT_EQ(sink->writes, 0);
}
