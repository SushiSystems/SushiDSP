/**************************************************************************/
/* test_log_join.cpp                                                      */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_log_join.cpp
 * @brief Tests the C call a host makes to join the plug-in's logger to its own.
 * @author Mustafa Garip
 */

#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log.hpp>

#include "log_join.hpp"

namespace
{
    /// @brief Names the records this file logs.
    constexpr SushiLog::LogCategory K_JOIN_TEST_LOG_CATEGORY =
        SushiLog::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "plugin");

    /// @brief What the host function saw of one forwarded record.
    struct Seen
    {
            std::string message;
            std::thread::id thread;
    };

    /// @brief Keeps the message of a forwarded record and the thread it was handed over on, in
    /// the Seen the user pointer names.
    void keep_message(const SushiLogRecord* record, void* user)
    {
        Seen* seen = static_cast<Seen*>(user);
        seen->message.assign(record->message, record->message_size);
        seen->thread = std::this_thread::get_id();
    }

    /// @brief Joins the logger to keep_message, logs one error and leaves the join.
    /// @param seen Receives what the host function saw.
    void log_one_record_through_the_join(Seen& seen)
    {
        sushidsp_join_log(&keep_message, &seen);
        SD_LOG_ERROR(K_JOIN_TEST_LOG_CATEGORY, "joined {}", 1);
        sl_set_log_sink(nullptr, nullptr);
    }
} // namespace

TEST(Small_LogJoin, JoinedLoggerHandsEachRecordToTheHostFunction)
{
    Seen seen;
    log_one_record_through_the_join(seen);

    EXPECT_EQ(seen.message, "joined 1");
}

TEST(Small_LogJoin, JoinedLoggerHandsEachRecordOverOnTheCallingThread)
{
    Seen seen;
    log_one_record_through_the_join(seen);

    EXPECT_EQ(seen.thread, std::this_thread::get_id());
}
