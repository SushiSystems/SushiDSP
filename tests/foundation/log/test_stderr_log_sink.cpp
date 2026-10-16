/**************************************************************************/
/* test_stderr_log_sink.cpp                                               */
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
 * @file test_stderr_log_sink.cpp
 * @brief Tests the line the stderr sink writes for one record.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/stderr_log_sink.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

#include <gtest/gtest-spi.h>

TEST(Small_StderrLogSink, WritesTheRecordLineAndANewline)
{
    testing::internal::CaptureStderr();
    SushiDSP::StderrLogSink sink;
    const SushiDSP::LogRecord record{SushiDSP::LogLevel::Warn, "sushidsp", "room", "a.cpp", 1, 1, 0,
                                     std::string_view("slow"),
                                     std::string_view("[12:00:00.000] [WARN] [room] [t1] [a.cpp:1] slow")};
    sink.write(record);
    EXPECT_EQ(testing::internal::GetCapturedStderr(),
              "[12:00:00.000] [WARN] [room] [t1] [a.cpp:1] slow\n");
}

TEST(Small_StderrLogSink, WritesTheMessageWhenTheRecordHasNoLine)
{
    testing::internal::CaptureStderr();
    SushiDSP::StderrLogSink sink;
    sink.write(SushiDSP::LogRecord{SushiDSP::LogLevel::Warn, "sushidsp", "room", "a.cpp", 1, 1, 0,
                                   std::string_view("slow"), std::string_view()});
    EXPECT_EQ(testing::internal::GetCapturedStderr(), "slow\n");
}