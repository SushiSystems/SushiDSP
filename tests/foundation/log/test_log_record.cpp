/**************************************************************************/
/* test_log_record.cpp                                                    */
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
 * @file test_log_record.cpp
 * @brief Tests the fixed message length and the fields a log record carries.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log_record.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

#include <string_view>

TEST(Small_LogRecord, MessageLengthCeilingIsOneKibibyte)
{
    EXPECT_EQ(SushiDSP::K_MAX_LOG_MESSAGE_LENGTH, 1024u);
}

TEST(Small_LogRecord, HoldsTheLevelCategoryAndMessageItWasMadeWith)
{
    const SushiDSP::LogRecord record{SushiDSP::LogLevel::Error, "sushidsp", "room", "a.cpp", 9, 2, 5,
                                     "failed", "[ERROR] failed"};
    EXPECT_EQ(record.level, SushiDSP::LogLevel::Error);
    EXPECT_EQ(record.project, std::string_view("sushidsp"));
    EXPECT_EQ(record.module, std::string_view("room"));
    EXPECT_EQ(record.line, 9u);
    EXPECT_EQ(record.thread, 2u);
    EXPECT_EQ(record.time_ns, 5);
    EXPECT_EQ(record.message, std::string_view("failed"));
}

TEST(Small_LogRecord, LineCeilingIsThePrefixAndTheMessage)
{
    EXPECT_EQ(SushiDSP::K_MAX_LOG_PREFIX_LENGTH, 192u);
    EXPECT_EQ(SushiDSP::K_MAX_LOG_LINE_LENGTH, 1216u);
}

TEST(Small_LogRecord, NameBoundIsSixtyFourCharacters)
{
    EXPECT_EQ(SushiDSP::K_MAX_LOG_NAME_LENGTH, 64u);
}