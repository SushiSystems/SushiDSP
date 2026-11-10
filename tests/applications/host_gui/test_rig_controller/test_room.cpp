/**************************************************************************/
/* test_room.cpp                                                          */
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
 * @file test_room.cpp
 * @brief Tests the room a RigController hosts: its scene and arrangement in a preset, the pacing of
 * its publishes and its HRTF set.
 * @author Mustafa Garip
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rig_controller.hpp"
#include "support/rig_products.hpp"
#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/nodes/room/room_host.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>
#include <SushiDSP/nodes/room/starter_scene.hpp>

namespace
{
    /// @brief Holds the rigs the room tests build, on the catalogue of the fixture.
    class Medium_RigControllerRoom : public TestSupport::RigProducts
    {
    };

    /// @brief Compares two points field by field.
    bool same_point(const SushiDSP::room::Vec3& left, const SushiDSP::room::Vec3& right)
    {
        return left.x == right.x && left.y == right.y && left.z == right.z;
    }

    /// @brief Compares two scenes field by field, every patch and the cabinet included.
    bool same_scene(const SushiDSP::room::RoomScene& left, const SushiDSP::room::RoomScene& right)
    {
        if (!same_point(left.dimensions, right.dimensions) || left.surfaces != right.surfaces ||
            left.scattering != right.scattering || left.patch_count != right.patch_count)
        {
            return false;
        }
        for (std::uint32_t index = 0; index < left.patch_count; ++index)
        {
            if (left.patches[index].surface != right.patches[index].surface ||
                left.patches[index].material != right.patches[index].material ||
                left.patches[index].area_m2 != right.patches[index].area_m2)
            {
                return false;
            }
        }
        return same_point(left.source_position, right.source_position) &&
               left.source_yaw_rad == right.source_yaw_rad &&
               left.cabinet.cone_radius_m == right.cabinet.cone_radius_m &&
               left.cabinet.driver_spacing_m == right.cabinet.driver_spacing_m &&
               left.cabinet.baffle_width_m == right.cabinet.baffle_width_m &&
               left.cabinet.breakup_frequency_hz == right.cabinet.breakup_frequency_hz &&
               same_point(left.listener_position, right.listener_position) &&
               left.listener_yaw_rad == right.listener_yaw_rad &&
               same_point(left.microphone_position, right.microphone_position) &&
               left.microphone_yaw_rad == right.microphone_yaw_rad &&
               left.microphone_pattern == right.microphone_pattern;
    }

    /// @brief Builds a scene that differs from RoomScene{} in every field a preset writes,
    /// every face's material and scattering word included.
    SushiDSP::room::RoomScene non_default_scene()
    {
        SushiDSP::room::RoomScene scene;
        scene.dimensions = {4.25, 3.5, 2.6};
        scene.surfaces = {SushiDSP::room::MaterialId::ConcreteBlockPainted,
                          SushiDSP::room::MaterialId::ConcreteUnpainted,
                          SushiDSP::room::MaterialId::ConcreteBlockPainted,
                          SushiDSP::room::MaterialId::ConcreteUnpainted,
                          SushiDSP::room::MaterialId::ConcreteBlockPainted,
                          SushiDSP::room::MaterialId::ConcreteUnpainted};
        scene.scattering = {
            SushiDSP::room::Scattering::Textured,  SushiDSP::room::Scattering::Cluttered,
            SushiDSP::room::Scattering::Cluttered, SushiDSP::room::Scattering::Textured,
            SushiDSP::room::Scattering::Cluttered, SushiDSP::room::Scattering::Textured};
        scene.patch_count = 2;
        scene.patches[0] = {SushiDSP::room::Surface::PlusX, SushiDSP::room::MaterialId::GlassWindow,
                            1.2};
        scene.patches[1] = {SushiDSP::room::Surface::MinusY,
                            SushiDSP::room::MaterialId::OpenDoorway, 1.6};
        scene.source_position = {0.6, 0.7, 0.45};
        scene.source_yaw_rad = 0.35;
        scene.cabinet = SushiDSP::room::K_CABINET1X12;
        scene.listener_position = {3.1, 2.2, 1.15};
        scene.listener_yaw_rad = -2.9;
        scene.microphone_position = {0.66, 0.7, 0.45};
        scene.microphone_yaw_rad = 3.0;
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Supercardioid;
        return scene;
    }

    /// @brief Returns the path of the six-direction HRTF fixture convert_hrtf.py's self-test
    /// writes.
    std::string hrtf_fixture_path()
    {
        return std::string(SUSHIDSP_FIXTURE_DIR) + "/hrtf_self_test.hrtf";
    }

    /// @brief Finds the first slot of a rig that carries one descriptor id.
    /// @return The slot, or std::nullopt when the rig has none.
    std::optional<RigController::SlotId> find_slot(const RigController& controller,
                                                   const std::string& descriptor_id)
    {
        for (const RigController::SlotId slot : controller.slot_order())
        {
            if (controller.descriptor_id(slot) == descriptor_id)
            {
                return slot;
            }
        }
        return std::nullopt;
    }

    /// @brief Edits a room's scene @p edits times, each moving the cabinet 1 cm, and advances the
    /// injected clock one millisecond after each edit.
    /// @return True when every edit was accepted.
    bool drag_cabinet(RigController& controller, RigController::SlotId room,
                      std::chrono::steady_clock::time_point& clock, int edits)
    {
        SushiDSP::room::RoomScene scene;
        std::string error;
        bool accepted = true;
        for (int edit = 0; edit < edits; ++edit)
        {
            scene.source_position.x = 0.5 + 0.01 * edit;
            accepted = controller.set_room_scene(room, scene, error) && accepted;
            clock += std::chrono::milliseconds(1);
        }
        return accepted;
    }

    /// @brief Fails unless the first @p count furnishings of two scenes agree in item and quantity.
    void expect_same_furnishings(const SushiDSP::room::RoomScene& actual,
                                 const SushiDSP::room::RoomScene& expected, std::uint32_t count)
    {
        ASSERT_EQ(actual.furnishing_count, count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            EXPECT_EQ(actual.furnishings[index].item, expected.furnishings[index].item);
            EXPECT_EQ(actual.furnishings[index].quantity, expected.furnishings[index].quantity);
        }
    }

    /// @brief Builds a format 2 preset of one room that lists @p rows furnishing rows.
    std::string preset_with_furnishing_rows(std::size_t rows)
    {
        nlohmann::json list = nlohmann::json::array();
        for (std::size_t index = 0; index < rows; ++index)
        {
            list.push_back({{"item", "sofa_two_seat"}, {"quantity", 1.0}});
        }
        const nlohmann::json preset = {
            {"format", 2},
            {"nodes", nlohmann::json::array({{{"id", 0},
                                              {"type", "sushidsp.room"},
                                              {"params", nlohmann::json::object()},
                                              {"inputs", nlohmann::json::array()},
                                              {"room", {{"furnishings", list}}}}})}};
        return preset.dump();
    }

    const char* const K_FORMAT_ONE_PRESET_WITH_A_ROOM = R"({"nodes":[
        {"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
        {"id":1,"type":"sushidsp.room","params":{"listener":1.0,"level":-4.0,"mix":0.5},
         "inputs":[0],"room":{"dimensions":[4.0,3.0,2.5]}},
        {"id":2,"type":"sushidsp.tubescreamer","params":{},"inputs":[1]}
    ]})";

    const char* const K_ROOM_WITH_DIMENSIONS_ONLY = R"({"format":2,"nodes":[
        {"id":0,"type":"sushidsp.room","params":{},"inputs":[],
         "room":{"dimensions":[4.0,3.0,2.5]}}
    ]})";
} // namespace

TEST_F(Medium_RigControllerRoom, RoundTripsARoomSceneThroughJson)
{
    RigController source = rig({48000.0, 64, 2});
    source.append("sushidsp.tubescreamer");
    const RigController::SlotId room = source.append("sushidsp.room");
    ASSERT_TRUE(source.hosts_room(room));
    const SushiDSP::room::RoomScene scene = non_default_scene();
    std::string error;
    ASSERT_TRUE(source.set_room_scene(room, scene, error)) << error;
    ASSERT_NE(source.room_host(room), nullptr);
    ASSERT_TRUE(same_scene(source.room_host(room)->scene(), scene));
    const std::string saved = source.save_to_json();

    RigController target = rig({48000.0, 64, 2});
    target.load_from_json(saved);

    EXPECT_EQ(target.save_to_json(), saved);
    const std::vector<RigController::SlotId> order = target.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_TRUE(same_scene(target.room_scene(order[1]), scene));
    ASSERT_NE(target.room_host(order[1]), nullptr);
    EXPECT_TRUE(same_scene(target.room_host(order[1])->scene(), scene));
}

TEST_F(Medium_RigControllerRoom, WritesTheRoomSceneUnderTheRoomNodeAsNamedKeys)
{
    RigController source = rig({48000.0, 64, 2});
    source.append("sushidsp.tubescreamer");
    const RigController::SlotId room = source.append("sushidsp.room");
    std::string error;
    ASSERT_TRUE(source.set_room_scene(room, non_default_scene(), error)) << error;

    const nlohmann::json parsed = nlohmann::json::parse(source.save_to_json());

    ASSERT_EQ(parsed.at("nodes").size(), 2u);
    EXPECT_FALSE(parsed.at("nodes").at(0).contains("room"));
    ASSERT_TRUE(parsed.at("nodes").at(1).contains("room"));
    const nlohmann::json& written = parsed.at("nodes").at(1).at("room");
    EXPECT_EQ(written.at("surfaces").at(4).get<std::string>(), "concrete_block_painted");
    EXPECT_EQ(written.at("scattering").at(2).get<std::string>(), "cluttered");
    EXPECT_EQ(written.at("patches").size(), 2u);
    EXPECT_EQ(written.at("microphone").at("pattern").get<std::string>(), "supercardioid");
}

TEST_F(Medium_RigControllerRoom, LoadsAPresetWithNoRoomAsTheStarterScene)
{
    const std::string preset = R"({"format":2,"nodes":[
        {"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
        {"id":1,"type":"sushidsp.room","params":{},"inputs":[0]}
    ]})";
    RigController controller = rig({48000.0, 64, 2});

    controller.load_from_json(preset);

    const std::vector<RigController::SlotId> order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    ASSERT_TRUE(controller.hosts_room(order[1]));
    const SushiDSP::room::RoomScene starter = SushiDSP::room::starter_scene();
    EXPECT_TRUE(same_scene(controller.room_scene(order[1]), starter));
    EXPECT_EQ(controller.room_scene(order[1]).furnishing_count, starter.furnishing_count);
    ASSERT_NE(controller.room_host(order[1]), nullptr);
    EXPECT_TRUE(same_scene(controller.room_host(order[1])->scene(), starter));
    EXPECT_EQ(controller.room_host(order[1])->scene().furnishing_count, starter.furnishing_count);
    // The fresh node already carries the starter scene, so no scene was handed to it.
    EXPECT_EQ(controller.room_publishes(order[1]), 0u);
}

TEST_F(Medium_RigControllerRoom, CoalescesADragToThirtyPublishesPerSecond)
{
    std::chrono::steady_clock::time_point clock{};
    RigController controller = rig({48000.0, 64, 2}, [&clock] { return clock; });
    const RigController::SlotId room = controller.append("sushidsp.room");
    ASSERT_NE(controller.room_host(room), nullptr);
    ASSERT_EQ(controller.room_publishes(room), 0u);
    constexpr int K_EDITS = 300;

    ASSERT_TRUE(drag_cabinet(controller, room, clock, K_EDITS));

    /* A publish needs 34 whole ms after the last (1 / 30 s = 33.3 ms), at one edit per ms. */
    const std::size_t published = controller.room_publishes(room);
    EXPECT_EQ(published, 1u + static_cast<std::size_t>((K_EDITS - 1) / 34));
    // The newest scene waits for a frame past the interval, then reaches the node whole.
    clock += std::chrono::milliseconds(40);
    controller.reclaim();
    EXPECT_EQ(controller.room_publishes(room), published + 1u);
    EXPECT_EQ(controller.room_host(room)->scene().source_position.x, 0.5 + 0.01 * (K_EDITS - 1));
}

TEST_F(Medium_RigControllerRoom, ReclaimPublishesNothingWhenNoSceneIsPending)
{
    std::chrono::steady_clock::time_point clock{};
    RigController controller = rig({48000.0, 64, 2}, [&clock] { return clock; });
    const RigController::SlotId room = controller.append("sushidsp.room");
    ASSERT_TRUE(drag_cabinet(controller, room, clock, 3));
    clock += std::chrono::milliseconds(40);
    controller.reclaim();
    const std::size_t published = controller.room_publishes(room);

    controller.reclaim();

    EXPECT_EQ(controller.room_publishes(room), published);
}

TEST_F(Medium_RigControllerRoom, RefusesASceneOutsideTheRoomWithoutPublishing)
{
    RigController controller = rig({48000.0, 64, 2});
    const RigController::SlotId room = controller.append("sushidsp.room");
    SushiDSP::room::RoomScene outside;
    outside.listener_position.x = outside.dimensions.x + 1.0;

    std::string error;
    EXPECT_FALSE(controller.set_room_scene(room, outside, error));

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(controller.room_publishes(room), 0u);
    EXPECT_TRUE(same_scene(controller.room_scene(room), SushiDSP::room::starter_scene()));
}

TEST_F(Medium_RigControllerRoom, InstallsAMeasuredHrtfIntoTheRoomsItHolds)
{
    RigController controller = rig({48000.0, 64, 2});
    const RigController::SlotId room = controller.append("sushidsp.room");
    ASSERT_NE(controller.room_host(room), nullptr);
    ASSERT_EQ(controller.room_host(room)->hrtf_source(), SushiDSP::room::HrtfSource::SphericalHead);

    std::string error;
    ASSERT_TRUE(controller.set_hrtf_path(hrtf_fixture_path(), error)) << error;

    EXPECT_EQ(error, "");
    EXPECT_EQ(controller.hrtf_path(), hrtf_fixture_path());
    EXPECT_EQ(controller.room_host(room)->hrtf_source(), SushiDSP::room::HrtfSource::Measured);
}

TEST_F(Medium_RigControllerRoom, KeepsTheMeasuredHrtfAcrossARebuild)
{
    RigController controller = rig({48000.0, 64, 2});
    const RigController::SlotId first = controller.append("sushidsp.room");
    std::string error;
    ASSERT_TRUE(controller.set_hrtf_path(hrtf_fixture_path(), error)) << error;

    const RigController::SlotId second = controller.append("sushidsp.room");

    ASSERT_NE(controller.room_host(first), nullptr);
    ASSERT_NE(controller.room_host(second), nullptr);
    EXPECT_EQ(controller.room_host(first)->hrtf_source(), SushiDSP::room::HrtfSource::Measured);
    EXPECT_EQ(controller.room_host(second)->hrtf_source(), SushiDSP::room::HrtfSource::Measured);
}

TEST_F(Medium_RigControllerRoom, ReportsAMissingHrtfByItsFileName)
{
    RigController controller = rig({48000.0, 64, 2});
    controller.append("sushidsp.room");
    const std::string missing = std::string(SUSHIDSP_FIXTURE_DIR) + "/no_such_set.hrtf";

    std::string error;
    EXPECT_FALSE(controller.set_hrtf_path(missing, error));

    EXPECT_NE(error.find(missing), std::string::npos) << error;
    EXPECT_EQ(controller.hrtf_path(), "");
}

TEST_F(Medium_RigControllerRoom, ALoadedHrtfFallsBackWhenTheNextOneIsMissing)
{
    RigController controller = rig({48000.0, 64, 2});
    const RigController::SlotId room = controller.append("sushidsp.room");
    std::string error;
    ASSERT_TRUE(controller.set_hrtf_path(hrtf_fixture_path(), error)) << error;
    ASSERT_EQ(controller.room_host(room)->hrtf_source(), SushiDSP::room::HrtfSource::Measured);

    EXPECT_FALSE(controller.set_hrtf_path(
        std::string(SUSHIDSP_FIXTURE_DIR) + "/no_such_set.hrtf", error));

    EXPECT_EQ(controller.room_host(room)->hrtf_source(), SushiDSP::room::HrtfSource::SphericalHead);
}

TEST_F(Medium_RigControllerRoom, ARebuildAfterAMissingHrtfLeavesTheFreshNodeOnItsOwnFallback)
{
    RigController controller = rig({48000.0, 64, 2});
    const RigController::SlotId room = controller.append("sushidsp.room");
    std::string error;
    ASSERT_TRUE(controller.set_hrtf_path(hrtf_fixture_path(), error)) << error;
    ASSERT_FALSE(controller.set_hrtf_path(
        std::string(SUSHIDSP_FIXTURE_DIR) + "/no_such_set.hrtf", error));

    controller.append("sushidsp.tubescreamer");

    ASSERT_NE(controller.room_host(room), nullptr);
    EXPECT_EQ(controller.room_host(room)->hrtf_source(), SushiDSP::room::HrtfSource::SphericalHead);
}

TEST_F(Medium_RigControllerRoom, AFailedHrtfLoadNamesWhatChangedAndTheLiveRoomsItReset)
{
    RigController controller = rig({48000.0, 64, 2});
    controller.append("sushidsp.room");
    std::string error;

    SushiDSP::LogCapture capture;
    EXPECT_FALSE(controller.set_hrtf_path("no_such_hrtf_set.hrtf", error));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0].category, "room");
    EXPECT_EQ(lines[1].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[1].category, "host_gui");
    EXPECT_NE(lines[1].message.find("1 live room(s)"), std::string::npos) << lines[1].message;
}

TEST_F(Medium_RigControllerRoom, RoundTripsThreeFurnishingsThroughJson)
{
    using SushiDSP::room::FurnitureId;
    RigController source = rig({48000.0, 64, 2});
    const RigController::SlotId room = source.append("sushidsp.room");
    ASSERT_TRUE(source.hosts_room(room));
    SushiDSP::room::RoomScene scene = non_default_scene();
    scene.furnishing_count = 3;
    scene.furnishings[0] = {FurnitureId::WardrobeModule, 4.5};
    scene.furnishings[1] = {FurnitureId::SofaTwoSeat, 2.0};
    scene.furnishings[2] = {FurnitureId::WardrobeModule, 1.25};
    std::string error;
    ASSERT_TRUE(source.set_room_scene(room, scene, error)) << error;
    const std::string saved = source.save_to_json();
    const nlohmann::json written =
        nlohmann::json::parse(saved).at("nodes").at(0).at("room").at("furnishings");
    ASSERT_EQ(written.size(), 3u);
    EXPECT_EQ(written.at(1).at("item").get<std::string>(), "sofa_two_seat");
    EXPECT_EQ(written.at(2).at("quantity").get<double>(), 1.25);

    RigController target = rig({48000.0, 64, 2});
    target.load_from_json(saved);

    EXPECT_EQ(target.save_to_json(), saved);
    ASSERT_EQ(target.slot_order().size(), 1u);
    expect_same_furnishings(target.room_scene(target.slot_order()[0]), scene, 3u);
}

TEST_F(Medium_RigControllerRoom, WritesNoFurnishingsFieldForABareRoom)
{
    RigController source = rig({48000.0, 64, 2});
    const RigController::SlotId room = source.append("sushidsp.room");
    std::string error;
    ASSERT_TRUE(source.set_room_scene(room, non_default_scene(), error)) << error;

    const nlohmann::json parsed = nlohmann::json::parse(source.save_to_json());

    EXPECT_FALSE(parsed.at("nodes").at(0).at("room").contains("furnishings"));
}

TEST_F(Medium_RigControllerRoom, LoadsARoomWithoutFurnishingsAsAnUnfurnishedScene)
{
    RigController controller = rig({48000.0, 64, 2});

    controller.load_from_json(K_ROOM_WITH_DIMENSIONS_ONLY);

    ASSERT_EQ(controller.slot_order().size(), 1u);
    const SushiDSP::room::RoomScene loaded = controller.room_scene(controller.slot_order()[0]);
    EXPECT_EQ(loaded.dimensions.x, 4.0);
    EXPECT_EQ(loaded.furnishing_count, 0u);
}

TEST_F(Medium_RigControllerRoom, RefusesAFurnishingNoTableRowNames)
{
    const std::string preset = R"({"format":2,"nodes":[
        {"id":0,"type":"sushidsp.room","params":{},"inputs":[],
         "room":{"furnishings":[{"item":"no_such_row","quantity":1.0}]}}
    ]})";
    RigController controller = rig({48000.0, 64, 2});

    EXPECT_THROW(controller.load_from_json(preset), std::runtime_error);
}

TEST_F(Medium_RigControllerRoom, RefusesMoreFurnishingRowsThanARoomCarries)
{
    RigController controller = rig({48000.0, 64, 2});

    EXPECT_THROW(controller.load_from_json(preset_with_furnishing_rows(
                     SushiDSP::room::RoomScene::K_MAX_FURNISHINGS + 1)),
                 std::runtime_error);
}

TEST_F(Medium_RigControllerRoom, RoundTripsTheBuiltRoomLossThroughJson)
{
    RigController source = rig({48000.0, 64, 2});
    const RigController::SlotId room = source.append("sushidsp.room");
    SushiDSP::room::RoomScene scene = non_default_scene();
    scene.built_room_loss = true;
    std::string error;
    ASSERT_TRUE(source.set_room_scene(room, scene, error)) << error;
    const std::string saved = source.save_to_json();
    EXPECT_TRUE(nlohmann::json::parse(saved)
                    .at("nodes")
                    .at(0)
                    .at("room")
                    .at("built_room_loss")
                    .get<bool>());

    RigController target = rig({48000.0, 64, 2});
    target.load_from_json(saved);

    EXPECT_EQ(target.save_to_json(), saved);
    EXPECT_TRUE(target.room_scene(target.slot_order().at(0)).built_room_loss);
}

TEST_F(Medium_RigControllerRoom, LoadsARoomWithoutTheBuiltRoomLossFieldWithTheLossOff)
{
    RigController controller = rig({48000.0, 64, 2});

    controller.load_from_json(K_ROOM_WITH_DIMENSIONS_ONLY);

    EXPECT_FALSE(controller.room_scene(controller.slot_order().at(0)).built_room_loss);
}

TEST_F(Medium_RigControllerRoom, RefusesABuiltRoomLossThatIsNotABoolean)
{
    const std::string preset = R"({"format":2,"nodes":[
        {"id":0,"type":"sushidsp.room","params":{},"inputs":[],
         "room":{"built_room_loss":1}}
    ]})";
    RigController controller = rig({48000.0, 64, 2});

    EXPECT_THROW(controller.load_from_json(preset), std::runtime_error);
}

TEST_F(Medium_RigControllerRoom, WritesFormatTwoWithItsEdgesAndTheirPorts)
{
    RigController controller = rig({48000.0, 64, 2});
    controller.append("sushidsp.tubescreamer");
    const RigController::SlotId room = controller.append("sushidsp.room");
    const RigController::SlotId microphone = controller.add_node("sushidsp.microphone", false);
    controller.connect(room, microphone, true, 1);

    const nlohmann::json parsed = nlohmann::json::parse(controller.save_to_json());

    EXPECT_EQ(parsed.at("format").get<int>(), 2);
    ASSERT_EQ(parsed.at("edges").size(), 2u);
    EXPECT_EQ(parsed.at("edges").at(1).at("port").get<int>(), 1);
}

TEST_F(Medium_RigControllerRoom, ALoadedFormatOnePresetLosesItsRoomSlotAndRejoinsTheChain)
{
    RigController controller = rig({48000.0, 64, 2});

    controller.load_from_json(K_FORMAT_ONE_PRESET_WITH_A_ROOM);

    const std::vector<RigController::SlotId> order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(controller.descriptor_id(order[0]), "sushidsp.tubescreamer");
    EXPECT_EQ(controller.descriptor_id(order[1]), "sushidsp.tubescreamer");
    ASSERT_EQ(controller.edges().size(), 1u);
    EXPECT_EQ(controller.edges().front(), std::make_pair(order[0], order[1]));
    EXPECT_FALSE(controller.shape().has_value());
}

TEST_F(Medium_RigControllerRoom, ALoadedFormatOnePresetKeepsItsRoomAsALegacyRecord)
{
    RigController controller = rig({48000.0, 64, 2});
    controller.load_from_json(K_FORMAT_ONE_PRESET_WITH_A_ROOM);

    const std::optional<LegacyRoom> legacy = controller.take_legacy_room();

    ASSERT_TRUE(legacy.has_value());
    EXPECT_FALSE(legacy->ears);
    EXPECT_EQ(legacy->level_db, -4.0f);
    EXPECT_EQ(legacy->mix, 0.5f);
    EXPECT_EQ(legacy->scene.dimensions.x, 4.0);
}

TEST_F(Medium_RigControllerRoom, TheLegacyRoomIsHandedOverOnce)
{
    RigController controller = rig({48000.0, 64, 2});
    controller.load_from_json(K_FORMAT_ONE_PRESET_WITH_A_ROOM);
    ASSERT_TRUE(controller.take_legacy_room().has_value());

    EXPECT_FALSE(controller.take_legacy_room().has_value());
}

TEST_F(Medium_RigControllerRoom, RoundTripsARoomsArrangementThroughJson)
{
    RigController source = rig({48000.0, 64, 2});
    const RigController::SlotId cabinet = source.append("sushidsp.cabinet");
    const RigController::SlotId room = source.append("sushidsp.room");
    const RigController::SlotId microphone = source.add_node("sushidsp.microphone", false);
    source.connect(room, microphone, true, 1);
    SushiDSP::Room::Arrangement arrangement;
    arrangement.set_listener({{4.0, 2.5, 1.3}, 0.4});
    std::string reason;
    ASSERT_TRUE(arrangement.add_source({cabinet, {{1.2, 1.5, 0.6}, 0.2}}, reason)) << reason;
    SushiDSP::Room::MicrophonePlacement placement;
    placement.id = microphone;
    placement.pose = {{1.6, 1.5, 0.7}, 3.0};
    placement.pattern = SushiDSP::room::PolarPattern::FigureEight;
    placement.proximity = false;
    ASSERT_TRUE(arrangement.add_microphone(placement, reason)) << reason;
    ASSERT_TRUE(arrangement.set_source_cabinet(cabinet, SushiDSP::room::K_CABINET2X12));
    SushiDSP::room::RoomScene shell = arrangement.shell();
    shell.furnishing_count = 1;
    shell.furnishings[0] = {SushiDSP::room::FurnitureId::SofaTwoSeat, 1.0};
    shell.furnishings[0].placed = true;
    shell.furnishings[0].x_m = 3.0;
    shell.furnishings[0].y_m = 1.0;
    shell.furnishings[0].yaw_rad = 0.5;
    arrangement.set_shell(shell);
    ASSERT_TRUE(source.set_room_arrangement(room, arrangement, reason)) << reason;
    const std::string saved = source.save_to_json();

    RigController target = rig({48000.0, 64, 2});
    target.load_from_json(saved);

    // The rows named their nodes, so the identities are the reloaded slots'.
    const std::optional<RigController::SlotId> loaded_room = find_slot(target, "sushidsp.room");
    const std::optional<RigController::SlotId> loaded_cabinet =
        find_slot(target, "sushidsp.cabinet");
    const std::optional<RigController::SlotId> loaded_microphone =
        find_slot(target, "sushidsp.microphone");
    ASSERT_TRUE(loaded_room.has_value());
    ASSERT_TRUE(loaded_cabinet.has_value());
    ASSERT_TRUE(loaded_microphone.has_value());
    const SushiDSP::Room::Arrangement loaded = target.room_arrangement(*loaded_room);
    ASSERT_EQ(loaded.sources().size(), 1u);
    EXPECT_EQ(loaded.sources()[0].id, *loaded_cabinet);
    EXPECT_EQ(loaded.sources()[0].pose.position.x, 1.2);
    EXPECT_EQ(loaded.sources()[0].pose.yaw_rad, 0.2);
    EXPECT_EQ(loaded.listener().position.y, 2.5);
    ASSERT_EQ(loaded.microphones().size(), 1u);
    EXPECT_EQ(loaded.microphones()[0].id, *loaded_microphone);
    EXPECT_EQ(loaded.microphones()[0].pose.position.z, 0.7);
    EXPECT_EQ(loaded.microphones()[0].pattern, SushiDSP::room::PolarPattern::FigureEight);
    EXPECT_FALSE(loaded.microphones()[0].proximity);
    EXPECT_EQ(loaded.microphones()[0].kind, SushiDSP::room::MicrophoneKind::FlatCondenser);
    EXPECT_NE(saved.find("flat_condenser"), std::string::npos);
    EXPECT_TRUE(SushiDSP::room::same_geometry(loaded.sources()[0].cabinet,
                                              SushiDSP::room::K_CABINET2X12));
    ASSERT_GE(loaded.shell().furnishing_count, 1u);
    EXPECT_TRUE(loaded.shell().furnishings[0].placed);
    EXPECT_EQ(loaded.shell().furnishings[0].x_m, 3.0);
    EXPECT_EQ(loaded.shell().furnishings[0].y_m, 1.0);
    EXPECT_EQ(loaded.shell().furnishings[0].yaw_rad, 0.5);
    EXPECT_EQ(target.save_to_json(), saved);
}
