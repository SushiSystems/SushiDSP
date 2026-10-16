/**************************************************************************/
/* test_log_level_setting.cpp                                             */
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
 * @file test_log_level_setting.cpp
 * @brief Tests the reading of a log level setting such as `warn,room=debug`.
 * @author Mustafa Garip
 */

#include <optional>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log_category.hpp>
#include <SushiDSP/foundation/log/log_level_setting.hpp>

TEST(Small_LogLevelSetting, LevelNamesAreReadInAnyCase)
{
    EXPECT_EQ(SushiDSP::parse_log_level("trace"), SushiDSP::LogLevel::Trace);
    EXPECT_EQ(SushiDSP::parse_log_level("DEBUG"), SushiDSP::LogLevel::Debug);
    EXPECT_EQ(SushiDSP::parse_log_level("Info"), SushiDSP::LogLevel::Info);
    EXPECT_EQ(SushiDSP::parse_log_level("wArN"), SushiDSP::LogLevel::Warn);
    EXPECT_EQ(SushiDSP::parse_log_level("error"), SushiDSP::LogLevel::Error);
    EXPECT_EQ(SushiDSP::parse_log_level("fatal"), SushiDSP::LogLevel::Fatal);
    EXPECT_EQ(SushiDSP::parse_log_level("OFF"), SushiDSP::LogLevel::Off);
    EXPECT_EQ(SushiDSP::parse_log_level("warning"), std::nullopt);
}

TEST(Small_LogLevelSetting, SettingIsALevelForEveryCategoryThenPairs)
{
    const std::optional<SushiDSP::LogLevelSetting> setting =
        SushiDSP::parse_log_level_setting("warn, distributed=debug ,Memory = TRACE");

    ASSERT_TRUE(setting.has_value());
    EXPECT_EQ(setting->every, SushiDSP::LogLevel::Warn);
    ASSERT_EQ(setting->pair_count, 2u);
    EXPECT_EQ(setting->pairs[0].category_hash, SushiDSP::log_category_hash("distributed"));
    EXPECT_EQ(setting->pairs[0].level, SushiDSP::LogLevel::Debug);
    EXPECT_EQ(setting->pairs[1].category_hash, SushiDSP::log_category_hash("memory"));
    EXPECT_EQ(setting->pairs[1].level, SushiDSP::LogLevel::Trace);
}

TEST(Small_LogLevelSetting, SettingOfOneLevelHasNoPairs)
{
    const std::optional<SushiDSP::LogLevelSetting> setting =
        SushiDSP::parse_log_level_setting("error");
    ASSERT_TRUE(setting.has_value());
    EXPECT_EQ(setting->every, SushiDSP::LogLevel::Error);
    EXPECT_EQ(setting->pair_count, 0u);
}

TEST(Small_LogLevelSetting, SettingThatCannotBeReadIsRefusedWhole)
{
    EXPECT_FALSE(SushiDSP::parse_log_level_setting("").has_value());
    EXPECT_FALSE(SushiDSP::parse_log_level_setting("loud").has_value());
    EXPECT_FALSE(SushiDSP::parse_log_level_setting("room=debug").has_value());
    EXPECT_FALSE(SushiDSP::parse_log_level_setting("warn,room").has_value());
    EXPECT_FALSE(SushiDSP::parse_log_level_setting("warn,room=loud").has_value());
    EXPECT_FALSE(SushiDSP::parse_log_level_setting("warn,=debug").has_value());
    EXPECT_FALSE(SushiDSP::parse_log_level_setting("warn,").has_value());
}
