/**************************************************************************/
/* test_parameter_report.cpp                                              */
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
 * @file test_parameter_report.cpp
 * @brief Tests the record a by-id parameter write writes when no row carries the id.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/parameter_report.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>

#include <string>
#include <vector>

TEST(Small_ParameterReport, OneWarnNamesTheIdAndTheValueUnderTheCoreCategory)
{
    SushiDSP::LogCapture capture;

    SushiDSP::report_unknown_parameter("missing", 2.5f);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "core");
    EXPECT_NE(lines[0].message.find("'missing'"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("2.5"), std::string::npos) << lines[0].message;
}
