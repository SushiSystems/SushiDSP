/**
 * @file test_log_capture.cpp
 * @brief Tests the scope that collects every SushiLog record.
 * @author Mustafa Garip
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log.hpp>
#include <SushiDSP/foundation/log/log_capture.hpp>

namespace
{
    /// @brief Names the records this file logs.
    constexpr SushiDSP::LogCategory K_CAPTURE_TEST_LOG_CATEGORY =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "log_capture_test");
} // namespace

TEST(Small_LogCapture, CollectsEachRecordWithItsLevelModuleMessageAndLine)
{
    SushiDSP::LogCapture capture;
    SD_LOG_ERROR(K_CAPTURE_TEST_LOG_CATEGORY, "slot {} refused", 3);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiDSP::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "log_capture_test");
    EXPECT_EQ(lines[0].message, "slot 3 refused");
    EXPECT_NE(lines[0].line.find("[ERROR] [sushidsp.log_capture_test] [t"), std::string::npos)
        << lines[0].line;
}