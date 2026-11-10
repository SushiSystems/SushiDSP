/**************************************************************************/
/* test_sushidsp_log_project.cpp                                          */
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
 * @file test_sushidsp_log_project.cpp
 * @brief Tests the SushiLog project every SushiDSP category belongs to.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiLog/log/log_level.hpp>

#include <SushiDSP/foundation/log/in_memory_environment.hpp>
#include <SushiDSP/foundation/log/log_level_setting_reader.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

TEST(Small_SushidspLogProject, IsSushidspSetBySdLogLevel)
{
    const SushiDSP::InMemoryEnvironment environment{{"SD_LOG_LEVEL", "info,room=debug"}};

    const auto setting = SushiDSP::read_log_level_setting(SushiDSP::K_SUSHIDSP_LOG_PROJECT,
                                                          environment);

    ASSERT_TRUE(setting.has_value());
    EXPECT_EQ(setting->level, SushiLog::LogLevel::Info);
    EXPECT_EQ(setting->level_of("room"), SushiLog::LogLevel::Debug);
}
