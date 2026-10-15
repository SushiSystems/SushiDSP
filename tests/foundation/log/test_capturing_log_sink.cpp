/**************************************************************************/
/* test_capturing_log_sink.cpp                                            */
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
 * @file test_capturing_log_sink.cpp
 * @brief Tests that the capturing sink keeps each line and counts its flushes.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/capturing_log_sink.hpp>

TEST(Small_CapturingLogSink, StartsEmptyWithNoFlushes)
{
    const SushiDSP::CapturingLogSink sink;
    EXPECT_TRUE(sink.lines().empty());
    EXPECT_EQ(sink.flush_count(), 0u);
}

TEST(Small_CapturingLogSink, KeepsTheCategoryAndMessageOfEachLine)
{
    SushiDSP::CapturingLogSink sink;
    sink.write(SushiDSP::LogRecord{SushiDSP::LogLevel::Info, SushiDSP::make_log_category("room"),
                                   std::string_view("ready")});
    const std::vector<SushiDSP::CapturedLogLine> lines = sink.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].category, "room");
    EXPECT_EQ(lines[0].message, "ready");
}

TEST(Small_CapturingLogSink, CountsEachFlush)
{
    SushiDSP::CapturingLogSink sink;
    sink.flush();
    sink.flush();
    EXPECT_EQ(sink.flush_count(), 2u);
}
