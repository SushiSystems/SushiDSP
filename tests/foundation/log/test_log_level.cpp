/**************************************************************************/
/* test_log_level.cpp                                                     */
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
 * @file test_log_level.cpp
 * @brief Tests the severity order and the names of the log levels.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log_level.hpp>

#include <cstdint>

namespace
{
    using SushiDSP::LogLevel;
} // namespace

TEST(Small_LogLevel, LevelsRiseFromTraceToFatal)
{
    EXPECT_LT(LogLevel::Trace, LogLevel::Debug);
    EXPECT_LT(LogLevel::Debug, LogLevel::Info);
    EXPECT_LT(LogLevel::Info, LogLevel::Warn);
    EXPECT_LT(LogLevel::Warn, LogLevel::Error);
    EXPECT_LT(LogLevel::Error, LogLevel::Fatal);
}

TEST(Small_LogLevel, ValuesAreTheFixedZeroToFiveSequence)
{
    EXPECT_EQ(static_cast<std::uint8_t>(LogLevel::Trace), 0);
    EXPECT_EQ(static_cast<std::uint8_t>(LogLevel::Fatal), 5);
}

TEST(Small_LogLevel, NameIsTheUpperCaseWordOfEachLevel)
{
    EXPECT_STREQ(SushiDSP::log_level_name(LogLevel::Trace), "TRACE");
    EXPECT_STREQ(SushiDSP::log_level_name(LogLevel::Warn), "WARN");
    EXPECT_STREQ(SushiDSP::log_level_name(LogLevel::Fatal), "FATAL");
}

TEST(Small_LogLevel, OffSitsAboveFatalAndIsNamedOff)
{
    EXPECT_LT(LogLevel::Fatal, LogLevel::Off);
    EXPECT_EQ(static_cast<std::uint8_t>(LogLevel::Off), 6);
    EXPECT_STREQ(SushiDSP::log_level_name(LogLevel::Off), "OFF");
}