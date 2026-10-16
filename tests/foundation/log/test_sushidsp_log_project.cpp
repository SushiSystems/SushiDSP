/**
 * @file test_sushidsp_log_project.cpp
 * @brief Tests the SushiLog project every SushiDSP category belongs to.
 * @author Mustafa Garip
 */

#include <string_view>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

TEST(Small_SushidspLogProject, IsSushidspSetBySdLogLevel)
{
    EXPECT_EQ(SushiDSP::K_SUSHIDSP_LOG_PROJECT.name(), std::string_view("sushidsp"));
    EXPECT_EQ(SushiDSP::K_SUSHIDSP_LOG_PROJECT.level_variable(), std::string_view("SD_LOG_LEVEL"));
}

TEST(Small_SushidspLogProject, CategoryCarriesTheProjectAndItsModule)
{
    constexpr SushiDSP::LogCategory category =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "room");
    EXPECT_EQ(category.project().name(), std::string_view("sushidsp"));
    EXPECT_EQ(category.module(), std::string_view("room"));
}