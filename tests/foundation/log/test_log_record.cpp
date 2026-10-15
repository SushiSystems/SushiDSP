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

#include <string_view>

TEST(LogRecord, MessageLengthCeilingIsOneKibibyte)
{
    EXPECT_EQ(SushiDSP::K_MAX_LOG_MESSAGE_LENGTH, 1024u);
}

TEST(LogRecord, HoldsTheLevelCategoryAndMessageItWasMadeWith)
{
    const SushiDSP::LogRecord record{SushiDSP::LogLevel::Error, SushiDSP::make_log_category("room"),
                                     std::string_view("failed")};
    EXPECT_EQ(record.level, SushiDSP::LogLevel::Error);
    EXPECT_EQ(record.category.name(), std::string_view("room"));
    EXPECT_EQ(record.message, std::string_view("failed"));
}
