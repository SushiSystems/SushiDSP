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
 * @brief Tests the callable that receives each record of the process logger.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <string>

#include <SushiDSP/foundation/log/log_sink.hpp>

TEST(Small_LogSink, ReceivesTheRecordItIsCalledWith)
{
    std::string seen;
    const SushiDSP::LogSink sink(
        [&seen](const SushiDSP::LogRecord& record)
        {
            seen = std::string(record.message);
        });
    sink(SushiDSP::LogRecord{SushiDSP::LogLevel::Info, "sushidsp", "room", "a.cpp", 1, 1, 0,
                             "ready", "ready"});
    EXPECT_EQ(seen, "ready");
}