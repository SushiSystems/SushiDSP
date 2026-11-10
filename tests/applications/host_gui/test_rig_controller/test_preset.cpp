/**************************************************************************/
/* test_preset.cpp                                                        */
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
 * @file test_preset.cpp
 * @brief Tests what a RigController preset writes and reads back for the chain, the shape, the
 * impulse-response folder and the playback speed.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rig_controller.hpp"
#include "rig_shape.hpp"
#include "support/rig_products.hpp"

namespace
{
    /// @brief Holds the rigs the preset tests build, on the catalogue of the fixture.
    class Small_RigControllerPreset : public TestSupport::RigProducts
    {
    };

    /// @brief Holds one impulse-response folder and whether a save writes it.
    struct IrFolderRow
    {
            const char* name;
            const char* folder;
            bool written;
    };

    /// @brief Holds the rigs the folder-key rows build, on the catalogue of the fixture.
    class Small_RigControllerIrFolderKey
        : public TestSupport::RigProducts,
          public ::testing::WithParamInterface<IrFolderRow>
    {
    };

    /// @brief Counts the live slots carrying one descriptor id.
    std::size_t count_slots_of_type(const RigController& controller,
                                    const std::string& descriptor_id)
    {
        std::size_t total = 0;
        for (RigController::SlotId slot : controller.slot_order())
        {
            if (controller.descriptor_id(slot) == descriptor_id)
            {
                ++total;
            }
        }
        return total;
    }

    /// @brief Finds the rig's first mixer.
    /// @return The mixer's slot id, or std::nullopt when the rig has none.
    std::optional<RigController::SlotId> find_mixer(const RigController& controller)
    {
        for (RigController::SlotId slot : controller.slot_order())
        {
            if (controller.descriptor_id(slot) == "sushidsp.mixer")
            {
                return slot;
            }
        }
        return std::nullopt;
    }

    const char* const K_PRESET_WITHOUT_IR_FOLDER = R"({"nodes":[
        {"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
        {"id":1,"type":"sushidsp.jcm800","params":{},"inputs":[0]}
    ]})";

    const char* const K_IR_FOLDER = "D:/irs/marshall";
} // namespace

TEST_F(Small_RigControllerPreset, SaveAndLoadRoundTripsChainAndParameters)
{
    RigController original = rig({48000.0, 256, 1});
    const RigController::SlotId ts = original.append("sushidsp.tubescreamer");
    original.set_parameter(ts, "drive", 0.77f);
    original.append("sushidsp.jcm800");

    RigController reloaded = rig({48000.0, 256, 1});
    reloaded.load_from_json(original.save_to_json());

    const auto order = reloaded.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(reloaded.descriptor_id(order[0]), "sushidsp.tubescreamer");
    EXPECT_EQ(reloaded.descriptor_id(order[1]), "sushidsp.jcm800");
    EXPECT_EQ(reloaded.get_parameter(order[0], "drive"), 0.77f);
}

TEST_F(Small_RigControllerPreset, SaveToJsonUsesOwnStoredValueNotLiveNodeState)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");

    controller.set_parameter(ts, "drive", 0.9f);

    const nlohmann::json root = nlohmann::json::parse(controller.save_to_json());
    ASSERT_EQ(root.at("nodes").size(), 1u);
    EXPECT_EQ(root.at("nodes")[0].at("params").at("drive").get<float>(), 0.9f);
}

TEST_F(Small_RigControllerPreset, SaveAndLoadRoundTripsAParallelBranch)
{
    RigController original = rig({48000.0, 256, 1});
    original.append("sushidsp.tubescreamer");
    const RigController::SlotId splitter = original.append("sushidsp.splitter");
    original.append("sushidsp.jcm800");
    ASSERT_TRUE(original.add_branch(splitter, "sushidsp.tubescreamer").has_value());

    RigController reloaded = rig({48000.0, 256, 1});
    reloaded.load_from_json(original.save_to_json());

    EXPECT_EQ(reloaded.slot_order().size(), 5u);
    EXPECT_EQ(count_slots_of_type(reloaded, "sushidsp.mixer"), 1u);
    EXPECT_EQ(count_slots_of_type(reloaded, "sushidsp.splitter"), 1u);
    const std::optional<RigController::SlotId> mixer = find_mixer(reloaded);
    ASSERT_TRUE(mixer.has_value());
    EXPECT_EQ(reloaded.upstream_of(*mixer).size(), 2u);
}

TEST_P(Small_RigControllerIrFolderKey, SaveToJsonWritesTheIrFolderOnlyWhenOneIsSet)
{
    RigController controller = rig({48000.0, 64, 1});
    controller.append("sushidsp.tubescreamer");
    controller.set_ir_folder(GetParam().folder);

    const nlohmann::json parsed = nlohmann::json::parse(controller.save_to_json());

    EXPECT_EQ(parsed.contains("ir_folder"), GetParam().written);
    EXPECT_EQ(parsed.value("ir_folder", std::string()), GetParam().folder);
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_RigControllerIrFolderKey,
    ::testing::Values(IrFolderRow{"FolderSet", K_IR_FOLDER, true},
                      IrFolderRow{"NoFolder", "", false}),
    [](const ::testing::TestParamInfo<IrFolderRow>& info) { return std::string(info.param.name); });

TEST_F(Small_RigControllerPreset, LoadFromJsonRestoresTheIrFolder)
{
    RigController source = rig({48000.0, 64, 1});
    source.append("sushidsp.tubescreamer");
    source.set_ir_folder(K_IR_FOLDER);

    RigController target = rig({48000.0, 64, 1});
    target.load_from_json(source.save_to_json());

    EXPECT_EQ(target.ir_folder(), K_IR_FOLDER);
}

TEST_F(Small_RigControllerPreset, LoadsAPresetWithNoIrFolderKeyWithAnEmptyFolder)
{
    RigController controller = rig({48000.0, 64, 1});
    controller.set_ir_folder("D:/irs/left_over");

    controller.load_from_json(K_PRESET_WITHOUT_IR_FOLDER);

    EXPECT_EQ(controller.ir_folder(), "");
}

TEST_F(Small_RigControllerPreset, SavesThePlaybackSpeed)
{
    RigController source = rig({48000.0, 64, 2});
    source.append("sushidsp.tubescreamer");
    // 0.4 is below the slowest speed the transport takes, so it clamps to 0.5.
    source.transport().set_speed(0.4);
    const std::string saved = source.save_to_json();
    RigController target = rig({48000.0, 64, 2});
    target.transport().set_speed(2.0);

    target.load_from_json(saved);

    EXPECT_NE(saved.find("playback_speed"), std::string::npos) << saved;
    EXPECT_EQ(target.transport().speed(), 0.5);
}

TEST_F(Small_RigControllerPreset, LoadsAPresetWithoutASpeedAtUnity)
{
    RigController source = rig({48000.0, 64, 2});
    source.append("sushidsp.tubescreamer");
    nlohmann::json document = nlohmann::json::parse(source.save_to_json());
    document.at("tempo").erase("playback_speed");
    RigController target = rig({48000.0, 64, 2});
    target.transport().set_speed(2.0);

    target.load_from_json(document.dump());

    EXPECT_EQ(target.transport().speed(), 1.0);
}

TEST_F(Small_RigControllerPreset, RefusesAPresetOfANewerFormatBeforeTouchingTheRig)
{
    RigController controller = rig({48000.0, 64, 2});
    controller.append("sushidsp.tubescreamer");

    EXPECT_THROW(controller.load_from_json(R"({"format":3,"nodes":[]})"), std::runtime_error);

    EXPECT_EQ(controller.slot_order().size(), 1u);
}

TEST_F(Small_RigControllerPreset, RoundTripsTheRigsShapeThroughJson)
{
    RigController source = rig({48000.0, 64, 2});
    const RigController::SlotId amp = source.append("sushidsp.tubescreamer");
    const RigController::SlotId strip = source.append("sushidsp.channel_strip");
    RigShape shape;
    shape.chain_type = ChainType::Single;
    shape.branches = {{amp}};
    shape.di = true;
    shape.di_position = DiPosition::BeforeEffects;
    shape.hidden.di_strip = strip;
    shape.hidden.join = amp;
    shape.hidden.amp_bus_feeders = {amp, strip};
    source.set_shape(shape);

    RigController target = rig({48000.0, 64, 2});
    target.load_from_json(source.save_to_json());

    ASSERT_TRUE(target.shape().has_value());
    const RigShape loaded = *target.shape();
    const std::vector<RigController::SlotId> order = target.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(loaded.chain_type, ChainType::Single);
    EXPECT_EQ(loaded.branches, (std::vector<std::vector<RigController::SlotId>>{{order[0]}}));
    EXPECT_TRUE(loaded.di);
    EXPECT_EQ(loaded.di_position, DiPosition::BeforeEffects);
    EXPECT_EQ(loaded.hidden.di_strip, std::optional<RigController::SlotId>(order[1]));
    EXPECT_EQ(loaded.hidden.amp_bus_feeders,
              (std::vector<RigController::SlotId>{order[0], order[1]}));
}
