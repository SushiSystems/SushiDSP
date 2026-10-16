/**
 * @file test_log_join.cpp
 * @brief Tests the C call a host makes to join the plug-in's logger to its own.
 * @author Mustafa Garip
 */

#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log.hpp>

#include "log_join.hpp"

namespace
{
    /// @brief Names the records this file logs.
    constexpr SushiDSP::LogCategory K_JOIN_TEST_LOG_CATEGORY =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "plugin");

    /// @brief Copies the message of a forwarded record into the string the user pointer names.
    void keep_message(const SushiLogRecord* record, void* user)
    {
        static_cast<std::string*>(user)->assign(record->message, record->message_size);
    }
} // namespace

TEST(Small_LogJoin, JoinedLoggerHandsEachRecordToTheHostFunctionOnTheCallingThread)
{
    std::string seen;
    sushidsp_join_log(&keep_message, &seen);
    SD_LOG_ERROR(K_JOIN_TEST_LOG_CATEGORY, "joined {}", 1);
    sl_set_log_sink(nullptr, nullptr);

    EXPECT_EQ(seen, "joined 1");
}