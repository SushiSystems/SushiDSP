/**************************************************************************/
/* test_cabinet.cpp                                                       */
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
 * @file test_cabinet.cpp
 * @brief Tests how RigController installs a cabinet impulse response, keeps it across a preset and a
 * snapshot, and says why it refuses one.
 * @author Mustafa Garip
 */

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "rig_controller.hpp"
#include "support/rig_products.hpp"
#include "support/wav_fixture.hpp"
#include <SushiDSP/foundation/log_capture/log_capture.hpp>

namespace
{
    /// @brief Holds the rigs the cabinet tests build, on the catalogue of the fixture, and removes the
    /// impulse-response file the test wrote.
    class Medium_RigControllerCabinet : public TestSupport::RigProducts
    {
        protected:
            /// @brief Writes the impulse-response file the case installs.
            /// @return The file's path.
            const std::string& write_impulse()
            {
                ir_path_ = TestSupport::write_temp_impulse_wav();
                return ir_path_;
            }

            /// @brief Returns the path write_impulse() last wrote.
            const std::string& impulse_path() const noexcept
            {
                return ir_path_;
            }

            void TearDown() override
            {
                std::remove(ir_path_.c_str());
            }

        private:
            std::string ir_path_;
    };

    /// @brief Holds one product that hosts no cabinet.
    struct RefusalRow
    {
            const char* name;
            const char* descriptor_id;
    };

    /// @brief Holds the rigs the refusal rows build.
    class Medium_RigControllerCabinetRefusal : public Medium_RigControllerCabinet,
                                               public ::testing::WithParamInterface<RefusalRow>
    {
    };
} // namespace

TEST_F(Medium_RigControllerCabinet, SaveAndLoadJsonRoundTripsTheCabinetPath)
{
    RigController original = rig({48000.0, 256, 1});
    original.append("sushidsp.jcm800");
    const RigController::SlotId cabinet = original.append("sushidsp.cabinet");
    ASSERT_TRUE(original.set_cabinet(cabinet, write_impulse()));

    RigController reloaded = rig({48000.0, 256, 1});
    reloaded.load_from_json(original.save_to_json());

    const auto order = reloaded.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(reloaded.cabinet_path(order[1]), impulse_path());
}

TEST_P(Medium_RigControllerCabinetRefusal, RefusesACabinetOnASlotThatHostsNone)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId slot = controller.append(GetParam().descriptor_id);

    EXPECT_FALSE(controller.set_cabinet(slot, write_impulse()));

    EXPECT_EQ(controller.cabinet_path(slot), "");
    EXPECT_FALSE(controller.hosts_cabinet(slot));
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Medium_RigControllerCabinetRefusal,
    ::testing::Values(RefusalRow{"Amplifier", "sushidsp.peavey5150"},
                      RefusalRow{"Delay", "sushidsp.dd3"}),
    [](const ::testing::TestParamInfo<RefusalRow>& info) { return std::string(info.param.name); });

TEST_F(Medium_RigControllerCabinet, SnapshotRestoreKeepsTheParameters)
{
    RigController controller = rig({48000.0, 64, 1});
    const RigController::SlotId amp = controller.append("sushidsp.jcm800");
    controller.set_parameter(amp, "preamp", 0.9f);
    const std::optional<RigController::SlotSnapshot> saved = controller.snapshot(amp);
    ASSERT_TRUE(saved.has_value());
    controller.remove(amp, false);

    const RigController::SlotId restored = controller.restore(*saved, false);
    controller.publish();

    EXPECT_EQ(controller.descriptor_id(restored), "sushidsp.jcm800");
    EXPECT_EQ(controller.get_parameter(restored, "preamp"), 0.9f);
}

TEST_F(Medium_RigControllerCabinet, SnapshotRestoreKeepsTheCabinetPath)
{
    RigController controller = rig({48000.0, 64, 1});
    const RigController::SlotId cabinet = controller.append("sushidsp.cabinet");
    ASSERT_TRUE(controller.set_cabinet(cabinet, write_impulse()));
    const std::optional<RigController::SlotSnapshot> saved = controller.snapshot(cabinet);
    ASSERT_TRUE(saved.has_value());
    controller.remove(cabinet, false);

    const RigController::SlotId restored = controller.restore(*saved, false);
    controller.publish();

    EXPECT_EQ(controller.cabinet_path(restored), impulse_path());
}

TEST_F(Medium_RigControllerCabinet, ACabinetOnANodeThatHostsNoneIsRefusedWithItsCause)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId delay = controller.append("sushidsp.dd3");
    std::string reason;

    EXPECT_FALSE(controller.set_cabinet(delay, write_impulse(), reason));

    EXPECT_NE(reason.find("hosts no cabinet"), std::string::npos) << reason;
}

TEST_F(Medium_RigControllerCabinet, ACabinetOnANodeThatHostsNoneWritesOneWarnNamingTheFile)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId delay = controller.append("sushidsp.dd3");
    std::string reason;

    SushiDSP::LogCapture capture;
    EXPECT_FALSE(controller.set_cabinet(delay, write_impulse(), reason));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_NE(lines[0].message.find(impulse_path()), std::string::npos) << lines[0].message;
}

TEST_F(Medium_RigControllerCabinet, ACabinetTheNodeTakesLeavesNoReasonAndNoRecord)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId cabinet = controller.append("sushidsp.cabinet");
    std::string reason = "stale";

    SushiDSP::LogCapture capture;
    EXPECT_TRUE(controller.set_cabinet(cabinet, write_impulse(), reason));

    EXPECT_TRUE(reason.empty());
    EXPECT_TRUE(capture.lines().empty());
}
