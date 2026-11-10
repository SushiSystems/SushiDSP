/**************************************************************************/
/* test_logging.cpp                                                       */
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
 * @file test_logging.cpp
 * @brief Tests the records RigController writes for refused edits and for the gaps a preset leaves.
 * @author Mustafa Garip
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "host_gui_log_category.hpp"
#include "rig_controller.hpp"
#include "support/rig_products.hpp"
#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/foundation/log_capture/log_level_scope.hpp>

namespace
{
    /// @brief Holds the rigs the logging tests build, on the catalogue of the fixture.
    class Small_RigControllerLogging : public TestSupport::RigProducts
    {
    };

    /// @brief Holds one write the controller cannot place and the text its record must carry.
    struct UnknownWriteRow
    {
            const char* name;
            bool on_screamer;
            const char* parameter;
            const char* named;
    };

    /// @brief Holds the rigs the unknown-write rows build, on the catalogue of the fixture.
    class Small_RigControllerUnknownWrite : public TestSupport::RigProducts,
                                            public ::testing::WithParamInterface<UnknownWriteRow>
    {
    };

    const char* const K_PRESET_WITH_TRACK_ROW_GAPS = R"({
        "nodes": [],
        "track_count": 3,
        "tracks": [
            {"index": 9, "name": "far", "gain": 1.0, "pan": 0.0, "muted": false, "soloed": false},
            {"index": 1, "name": "near", "pan": 0.0, "muted": false, "soloed": false,
             "clips": [{"path": ""}]}
        ]})";

    /// @brief Loads a preset with the host_gui category at Info and returns what the load wrote.
    std::vector<SushiDSP::CapturedLogLine> load_recording_info(RigController& controller,
                                                               const std::string& text)
    {
        const SushiDSP::LogLevelScope level(K_HOST_GUI_LOG_CATEGORY, SushiLog::LogLevel::Info);
        SushiDSP::LogCapture capture;
        controller.load_from_json(text);
        return capture.lines();
    }
} // namespace

TEST_F(Small_RigControllerLogging, ACabinetOnAnUnknownSlotIsRefusedWithAReason)
{
    RigController controller = rig({48000.0, 256, 1});
    std::string reason;

    EXPECT_FALSE(controller.set_cabinet(999u, "no_such_file.wav", reason));

    EXPECT_NE(reason.find("no slot 999"), std::string::npos) << reason;
}

TEST_F(Small_RigControllerLogging, ACabinetOnAnUnknownSlotWritesOneWarnNamingTheSlot)
{
    RigController controller = rig({48000.0, 256, 1});
    std::string reason;

    SushiDSP::LogCapture capture;
    EXPECT_FALSE(controller.set_cabinet(999u, "no_such_file.wav", reason));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "host_gui");
    EXPECT_NE(lines[0].message.find("slot 999"), std::string::npos) << lines[0].message;
}

TEST_F(Small_RigControllerLogging, AnUnknownDescriptorIsWarnedOnceNamingItsId)
{
    RigController controller = rig({48000.0, 256, 1});

    SushiDSP::LogCapture capture;
    controller.append("sushidsp.does_not_exist");
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "host_gui");
    EXPECT_NE(lines[0].message.find("sushidsp.does_not_exist"), std::string::npos)
        << lines[0].message;
}

TEST_P(Small_RigControllerUnknownWrite, AnUnknownWriteIsLoggedOncePerStreak)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId screamer = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId slot = GetParam().on_screamer ? screamer : 777u;

    SushiDSP::LogCapture capture;
    controller.set_parameter(slot, GetParam().parameter, 1.0f);
    controller.set_parameter(slot, GetParam().parameter, 0.5f);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].message.find(GetParam().named), std::string::npos) << lines[0].message;
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_RigControllerUnknownWrite,
    ::testing::Values(UnknownWriteRow{"UnknownParameter", true, "no_such_knob", "no_such_knob"},
                      UnknownWriteRow{"UnknownSlot", false, "drive", "slot 777"}),
    [](const ::testing::TestParamInfo<UnknownWriteRow>& info) { return std::string(info.param.name); });

TEST_F(Small_RigControllerLogging, AnUnknownParameterRearmsItsRecordOnAGoodWrite)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId screamer = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(screamer, "no_such_knob", 1.0f);

    SushiDSP::LogCapture capture;
    controller.set_parameter(screamer, "drive", 0.4f);
    controller.set_parameter(screamer, "no_such_knob", 0.2f);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_NE(lines[0].message.find("no_such_knob"), std::string::npos) << lines[0].message;
}

TEST_F(Small_RigControllerLogging, ACompleteSessionPresetLoadsWithoutARecord)
{
    RigController writer = rig({48000.0, 64, 1});
    writer.append("sushidsp.tubescreamer");
    const std::string text = writer.save_to_json();
    RigController reader = rig({48000.0, 64, 1});

    SushiDSP::LogCapture capture;
    reader.load_from_json(text);

    EXPECT_TRUE(capture.lines().empty());
}

TEST_F(Small_RigControllerLogging, GapsInTheTrackRowsWriteOneWarnWithTheCounts)
{
    RigController controller = rig({48000.0, 64, 1});

    const std::vector<SushiDSP::CapturedLogLine> lines =
        load_recording_info(controller, K_PRESET_WITH_TRACK_ROW_GAPS);

    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "host_gui");
    EXPECT_NE(lines[0].message.find("1 track row(s)"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("1 track key(s)"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("1 clip(s)"), std::string::npos) << lines[0].message;
}

TEST_F(Small_RigControllerLogging, APresetWithNoFormatKeyWritesOneInfoNamingFormatOne)
{
    RigController controller = rig({48000.0, 64, 1});

    const std::vector<SushiDSP::CapturedLogLine> lines =
        load_recording_info(controller, K_PRESET_WITH_TRACK_ROW_GAPS);

    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[1].level, SushiLog::LogLevel::Info);
    EXPECT_NE(lines[1].message.find("format 1"), std::string::npos) << lines[1].message;
}

TEST_F(Small_RigControllerLogging, ATrackCountPastTheLimitIsHeldAndNamedInTheWarn)
{
    RigController controller = rig({48000.0, 64, 1});

    SushiDSP::LogCapture capture;
    controller.load_from_json(R"({"nodes": [], "track_count": 20})");

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_GE(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    const std::string expected =
        "track count of 20 was held to " + std::to_string(SessionTimeline::K_MAX_TRACKS);
    EXPECT_NE(lines[0].message.find(expected), std::string::npos) << lines[0].message;
    EXPECT_EQ(controller.session()->track_count(), SessionTimeline::K_MAX_TRACKS);
}
