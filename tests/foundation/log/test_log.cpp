/**
 * @file test_log.cpp
 * @brief Tests the SD_LOG front over SushiLog.
 * @author Mustafa Garip
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log.hpp>
#include <SushiDSP/foundation/log/log_capture.hpp>
#include <SushiLog/log/log_calls.hpp>

namespace
{
    /// @brief Names the records this file logs.
    constexpr SushiDSP::LogCategory K_FRONT_TEST_LOG_CATEGORY =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "front_test");

    /// @brief Counts how often an argument is evaluated.
    int evaluated = 0;

    /// @brief Returns its argument and counts the call.
    int counted(int value)
    {
        ++evaluated;
        return value;
    }
} // namespace

TEST(Small_Log, CeilingIsDebugUnderNdebugAndTraceOtherwise)
{
#if defined(NDEBUG)
    EXPECT_EQ(SushiDSP::K_SUSHIDSP_LOG_CEILING, SushiDSP::LogLevel::Debug);
#else
    EXPECT_EQ(SushiDSP::K_SUSHIDSP_LOG_CEILING, SushiDSP::LogLevel::Trace);
#endif
}

TEST(Small_Log, CallBelowTheCategoryLevelEvaluatesNoArgumentAndOneAboveIsDelivered)
{
    SushiDSP::LogCapture capture;
    evaluated = 0;
    SD_LOG_INFO(K_FRONT_TEST_LOG_CATEGORY, "hidden {}", counted(1));
    SD_LOG_WARN(K_FRONT_TEST_LOG_CATEGORY, "shown {}", counted(2));

    EXPECT_EQ(evaluated, 1);
    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].message, "shown 2");
}

TEST(Small_Log, LevelSetForTheCategoryLetsItsDebugCallsThrough)
{
    SushiDSP::LogCapture capture;
    SushiLog::set_log_level(K_FRONT_TEST_LOG_CATEGORY, SushiDSP::LogLevel::Debug);
    SD_LOG_DEBUG(K_FRONT_TEST_LOG_CATEGORY, "state {}", 7);
    SushiLog::set_log_level(K_FRONT_TEST_LOG_CATEGORY, SushiDSP::LogLevel::Warn);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiDSP::LogLevel::Debug);
}