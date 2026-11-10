/**************************************************************************/
/* test_log_level_setting_reader.cpp                                      */
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
 * @file test_log_level_setting_reader.cpp
 * @brief Tests the reading of a project's level variable through an EnvironmentReader and its hand-over to SushiLog.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiLog/log/log_calls.hpp>
#include <SushiLog/log/log_category.hpp>
#include <SushiLog/log/log_project.hpp>

#include <SushiDSP/foundation/log/in_memory_environment.hpp>
#include <SushiDSP/foundation/log/log_level_setting_reader.hpp>

namespace
{
    /// @brief A project of this file's own, so applying a level leaves every product category alone.
    constexpr SushiLog::LogProject K_READER_TEST_PROJECT{"level_reader_test", "READER_TEST_LOG_LEVEL"};
} // namespace

TEST(Small_LogLevelSettingReader, ReadsTheVariableTheProjectNames)
{
    const SushiDSP::InMemoryEnvironment environment{{"READER_TEST_LOG_LEVEL", "info,room=debug"}};

    const auto setting = SushiDSP::read_log_level_setting(K_READER_TEST_PROJECT, environment);

    ASSERT_TRUE(setting.has_value());
    EXPECT_EQ(setting->level, SushiLog::LogLevel::Info);
    EXPECT_EQ(setting->level_of("room"), SushiLog::LogLevel::Debug);
}

TEST(Small_LogLevelSettingReader, FindsNothingWhenTheProjectsVariableIsNotSet)
{
    const SushiDSP::InMemoryEnvironment environment{{"OTHER_LOG_LEVEL", "debug"}};

    const auto setting = SushiDSP::read_log_level_setting(K_READER_TEST_PROJECT, environment);

    EXPECT_FALSE(setting.has_value());
}

TEST(Small_LogLevelSettingReader, FindsNothingWhenTheValueIsNotALevel)
{
    const SushiDSP::InMemoryEnvironment environment{{"READER_TEST_LOG_LEVEL", "loud"}};

    const auto setting = SushiDSP::read_log_level_setting(K_READER_TEST_PROJECT, environment);

    EXPECT_FALSE(setting.has_value());
}

TEST(Small_LogLevelSettingReader, AppliedSettingSetsTheProjectLevelAndEachNamedModule)
{
    const SushiDSP::InMemoryEnvironment environment{
        {"READER_TEST_LOG_LEVEL", "error,named=trace"}};

    SushiDSP::apply_log_level_setting(K_READER_TEST_PROJECT, environment);

    EXPECT_EQ(SushiLog::log_level(K_READER_TEST_PROJECT), SushiLog::LogLevel::Error);
    EXPECT_EQ(SushiLog::log_level(SushiLog::make_log_category(K_READER_TEST_PROJECT, "named")),
              SushiLog::LogLevel::Trace);
}
