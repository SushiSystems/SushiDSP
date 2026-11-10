/**************************************************************************/
/* test_sushilog_cabinet_log.cpp                                          */
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
 * @file test_sushilog_cabinet_log.cpp
 * @brief Tests the CabinetLog that writes each refusal as a SushiLog record.
 * @author Mustafa Garip
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/nodes/cabinet/sushilog_cabinet_log.hpp>

namespace
{
    /// @brief Writes @p refusal through a SushiLogCabinetLog and returns the lines it wrote.
    std::vector<SushiDSP::CapturedLogLine> lines_written_for(const SushiDSP::CabinetRefusal& refusal)
    {
        SushiDSP::LogCapture capture;
        SushiDSP::SushiLogCabinetLog log;

        log.report_refusal(refusal);

        return capture.lines();
    }
} // namespace

TEST(Small_SushiLogCabinetLog, AnEmptyImpulseResponseWritesOneWarnInTheCabinetCategory)
{
    const std::vector<SushiDSP::CapturedLogLine> lines = lines_written_for(
        {SushiDSP::CabinetRefusalCause::EmptyImpulseResponse, 0, 48000.0, 48000.0});

    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "cabinet");
    EXPECT_NE(lines[0].message.find("no samples"), std::string::npos) << lines[0].message;
}

TEST(Small_SushiLogCabinetLog, AnUnpreparedNodeWritesOneWarnNamingTheCause)
{
    const std::vector<SushiDSP::CapturedLogLine> lines =
        lines_written_for({SushiDSP::CabinetRefusalCause::NotPrepared, 1, 48000.0, 48000.0});

    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].message.find("not prepared"), std::string::npos) << lines[0].message;
}

TEST(Small_SushiLogCabinetLog, ARateMismatchWritesOneWarnNamingBothRates)
{
    const std::vector<SushiDSP::CapturedLogLine> lines =
        lines_written_for({SushiDSP::CabinetRefusalCause::SampleRateMismatch, 1, 44100.0, 48000.0});

    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].message.find("44100"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("48000"), std::string::npos) << lines[0].message;
}
