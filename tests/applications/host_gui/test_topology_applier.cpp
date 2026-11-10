/**************************************************************************/
/* test_topology_applier.cpp                                              */
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

/** @file test_topology_applier.cpp
 *  @brief Tests that TopologyApplier compiles a RigTopology into RigController slots and edges.
 *  @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "rig_controller.hpp"
#include "rig_topology.hpp"
#include "support/rig_products.hpp"
#include "support/tolerance.hpp"
#include "topology_applier.hpp"

namespace
{
    using SlotId = RigController::SlotId;
    using EdgeList = std::vector<std::pair<SlotId, SlotId>>;

    /// @brief Builds the prepare block every case's controller runs at.
    /// @return A mono 48 kHz block of 64 frames.
    SushiDSP::PrepareInfo test_info()
    {
        return SushiDSP::PrepareInfo{48000.0, 64, 1};
    }

    /// @brief Holds the rigs the cases build, on the catalogue of the fixture.
    class Small_TopologyApplier : public TestSupport::RigProducts
    {
    };

    /// @brief Counts the rig's slots carrying a descriptor.
    /// @return How many live slots hold `descriptor_id`.
    std::size_t count_descriptor(const RigController& rig, const std::string& descriptor_id)
    {
        const std::vector<SlotId> order = rig.slot_order();
        return static_cast<std::size_t>(std::count_if(order.begin(), order.end(),
                                                      [&](SlotId slot)
                                                      {
                                                          return rig.descriptor_id(slot) ==
                                                                 descriptor_id;
                                                      }));
    }

    /// @brief Finds the first live slot carrying a descriptor.
    /// @return Its id; the case fails when no slot carries it.
    SlotId find_descriptor(const RigController& rig, const std::string& descriptor_id)
    {
        for (SlotId slot : rig.slot_order())
        {
            if (rig.descriptor_id(slot) == descriptor_id)
            {
                return slot;
            }
        }
        ADD_FAILURE() << "no slot carries " << descriptor_id;
        return 0;
    }

    /// @brief Lists the slots of the cabinet section and the DI path an applier owns.
    /// @return Every hidden slot but the chain's own splitter, mixer and Parallel strips.
    std::vector<SlotId> section_slots(const TopologyApplier& applier)
    {
        HiddenSlots hidden = applier.mixer_slots();
        hidden.splitter.reset();
        hidden.mixer.reset();
        hidden.left_strip.reset();
        hidden.right_strip.reset();
        hidden.join.reset();
        hidden.bus_feeders.clear();
        hidden.room_sum_feeders.clear();
        hidden.amp_bus_feeders.clear();
        hidden.master_bus_feeders.clear();
        std::vector<SlotId> slots;
        hidden.for_each_id(
            [&slots](SlotId& slot)
            {
                slots.push_back(slot);
            });
        return slots;
    }

    /// @brief Lists the rig's edges that touch no slot of the cabinet section: the chain's own.
    /// @return The edges, in stored order.
    EdgeList chain_edges(const RigController& rig, const TopologyApplier& applier)
    {
        const std::vector<SlotId> section = section_slots(applier);
        const auto in_section = [&section](SlotId slot)
        {
            return std::find(section.begin(), section.end(), slot) != section.end();
        };
        EdgeList edges;
        for (const auto& edge : rig.edges())
        {
            if (!in_section(edge.first) && !in_section(edge.second))
            {
                edges.push_back(edge);
            }
        }
        return edges;
    }

    /// @brief Lists the rig's slots that are not part of the cabinet section.
    /// @return The slots, in slot order.
    std::vector<SlotId> gear_slots(const RigController& rig, const TopologyApplier& applier)
    {
        const std::vector<SlotId> section = section_slots(applier);
        std::vector<SlotId> slots;
        for (SlotId slot : rig.slot_order())
        {
            if (std::find(section.begin(), section.end(), slot) == section.end())
            {
                slots.push_back(slot);
            }
        }
        return slots;
    }

    /// @brief Reports whether the rig holds an edge on a given upstream port.
    bool has_edge(const RigController& rig, SlotId from, SlotId to, std::uint32_t port = 0)
    {
        for (const SlotGraph::EdgeRecord& edge : rig.edge_records())
        {
            if (edge.from == from && edge.to == to && edge.from_port == port)
            {
                return true;
            }
        }
        return false;
    }

    /// @brief Builds a rig of one amplifier into one cabinet, laid out as a single path.
    struct CabinetRig
    {
            RigController rig;
            SlotId amp = 0;
            SlotId cabinet = 0;
            RigTopology topology;
            TopologyApplier applier;

            /// @param catalogue The catalogue the rig is built on.
            explicit CabinetRig(const SushiDSP::NodeCatalogue& catalogue)
                : rig(test_info(), {}, catalogue)
            {
                amp = rig.add_node("sushidsp.tubescreamer", false);
                cabinet = rig.add_node("sushidsp.cabinet", false);
                topology = RigTopology::from_linear_order({amp, cabinet});
                applier.apply(topology, rig);
            }
    };

    /// @brief Holds the slots of the two microphones a case adds.
    struct MicrophonePair
    {
            SlotId first;
            SlotId second;
    };

    /// @brief Adds two microphones to a cabinet rig.
    MicrophonePair add_two_microphones(CabinetRig& built)
    {
        const SlotId first = built.applier.add_microphone(built.topology, built.rig);
        const SlotId second = built.applier.add_microphone(built.topology, built.rig);
        return {first, second};
    }

    /// @brief Adds microphones to a cabinet rig until the room holds @p count of them.
    void add_microphones(CabinetRig& built, std::size_t count)
    {
        while (built.applier.mixer_slots().microphones.size() < count)
        {
            built.applier.add_microphone(built.topology, built.rig);
        }
    }

    /// @brief Adds a microphone to a full room and returns what the refusal says.
    /// @return The refusal's message; empty when the microphone was accepted.
    std::string refusal_of_a_microphone(CabinetRig& built)
    {
        try
        {
            built.applier.add_microphone(built.topology, built.rig);
        }
        catch (const std::runtime_error& refusal)
        {
            return refusal.what();
        }
        return std::string();
    }

    /// @brief Adds two microphones and marks the second one a Room microphone.
    /// @return The second microphone's slot.
    SlotId add_a_close_and_a_room_microphone(CabinetRig& built)
    {
        const MicrophonePair pair = add_two_microphones(built);
        built.rig.set_parameter(pair.second, "room", 1.0f);
        built.applier.apply(built.topology, built.rig);
        return pair.second;
    }

    /// @brief Holds the height the second microphone is raised to, which no default pose has.
    constexpr double K_RAISED_HEIGHT = 1.4;

    /// @brief Raises the second microphone of a pair to K_RAISED_HEIGHT; the case fails when the
    /// room refuses the arrangement.
    void assert_second_microphone_raised(CabinetRig& built, const MicrophonePair& pair)
    {
        const SlotId room = *built.applier.mixer_slots().room;
        SushiDSP::Room::Arrangement arrangement = built.rig.room_arrangement(room);
        SushiDSP::Room::MicrophonePlacement moved = *arrangement.find_microphone(pair.second);
        moved.pose.position.z = K_RAISED_HEIGHT;
        ASSERT_TRUE(arrangement.update_microphone(moved));
        std::string reason;
        ASSERT_TRUE(built.rig.set_room_arrangement(room, arrangement, reason)) << reason;
    }

    /// @brief Migrates an old room whose listener hears it into a cabinet rig.
    /// @return The legacy room the migration read.
    LegacyRoom migrate_a_room_heard_by_the_listener(CabinetRig& built)
    {
        LegacyRoom legacy;
        legacy.scene = built.rig.room_scene(*built.applier.mixer_slots().room);
        legacy.scene.dimensions = {5.0, 4.0, 2.6};
        legacy.scene.source_position = {1.0, 2.0, 0.6};
        legacy.scene.listener_position = {3.5, 2.0, 1.3};
        legacy.ears = true;
        legacy.level_db = -6.0f;
        built.applier.migrate_legacy_room(built.topology, built.rig, legacy);
        return legacy;
    }

    /// @brief Holds the level the old room's microphone had, in decibels.
    constexpr float K_LEGACY_MICROPHONE_LEVEL_DB = -3.0f;

    /// @brief Migrates an old room whose microphone hears it into a cabinet rig.
    LegacyRoom migrate_a_room_heard_by_a_microphone(CabinetRig& built)
    {
        LegacyRoom legacy;
        legacy.scene = built.rig.room_scene(*built.applier.mixer_slots().room);
        legacy.scene.microphone_position = {2.2, 2.1, 0.7};
        legacy.scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
        legacy.ears = false;
        legacy.level_db = K_LEGACY_MICROPHONE_LEVEL_DB;
        built.applier.migrate_legacy_room(built.topology, built.rig, legacy);
        return legacy;
    }
} // namespace

TEST_F(Small_TopologyApplier, TopologyApplierSinglePathWiresALinearChain)
{
    RigController controller = rig(test_info());
    const SlotId a = controller.add_node("sushidsp.tubescreamer", false);
    const SlotId b = controller.add_node("sushidsp.jcm800", false);
    const SlotId c = controller.add_node("sushidsp.tubescreamer", false);
    RigTopology topology = RigTopology::from_linear_order({c, a, b});
    TopologyApplier applier;

    applier.apply(topology, controller);

    EXPECT_EQ(chain_edges(controller, applier), (EdgeList{{c, a}, {a, b}}));
    EXPECT_EQ(gear_slots(controller, applier).size(), 3u);
}

TEST_F(Small_TopologyApplier, TopologyApplierTwoWayAddsOneSplitterAndOneMixer)
{
    RigController controller = rig(test_info());
    const SlotId drive = controller.add_node("sushidsp.tubescreamer", false);
    const SlotId amp = controller.add_node("sushidsp.jcm800", false);
    RigTopology topology = RigTopology::from_linear_order({drive, amp});
    TopologyApplier applier;

    applier.switch_chain_type(topology, controller, ChainType::TwoWay);
    applier.apply(topology, controller);

    EXPECT_EQ(count_descriptor(controller, "sushidsp.splitter"), 1u);
    EXPECT_EQ(count_descriptor(controller, "sushidsp.mixer"), 1u);
    const SlotId splitter = find_descriptor(controller, "sushidsp.splitter");
    const SlotId mixer = find_descriptor(controller, "sushidsp.mixer");
    const std::vector<SlotId> copied = topology.stage({StageKind::Branch, 1});
    ASSERT_EQ(copied.size(), 2u);
    EXPECT_EQ(chain_edges(controller, applier), (EdgeList{{splitter, drive},
                                                          {drive, amp},
                                                          {amp, mixer},
                                                          {splitter, copied[0]},
                                                          {copied[0], copied[1]},
                                                          {copied[1], mixer}}));
}

TEST_F(Small_TopologyApplier, TopologyApplierOpeningAPathCopiesTheMainPathWithItsParameters)
{
    RigController controller = rig(test_info());
    const SlotId amp = controller.append("sushidsp.jcm800");
    controller.set_parameter(amp, "preamp", 0.9f);
    RigTopology topology = RigTopology::from_linear_order({amp});
    TopologyApplier applier;

    applier.switch_chain_type(topology, controller, ChainType::TwoWay);

    // The opened path carries a copy of the main path. See tests/applications/host_gui/README.md.
    EXPECT_EQ(topology.stage({StageKind::Branch, 0}), (std::vector<SlotId>{amp}));
    const std::vector<SlotId> copied = topology.stage({StageKind::Branch, 1});
    ASSERT_EQ(copied.size(), 1u);
    EXPECT_NE(copied[0], amp);
    EXPECT_EQ(controller.descriptor_id(copied[0]), "sushidsp.jcm800");
    EXPECT_FLOAT_EQ(controller.get_parameter(copied[0], "preamp"), 0.9f);
    EXPECT_FLOAT_EQ(controller.get_parameter(amp, "preamp"), 0.9f);
}

TEST_F(Small_TopologyApplier, TopologyApplierClosingAPathKeepsTheMainPathAndDropsTheCopy)
{
    RigController controller = rig(test_info());
    const SlotId amp = controller.append("sushidsp.jcm800");
    RigTopology topology = RigTopology::from_linear_order({amp});
    TopologyApplier applier;
    applier.switch_chain_type(topology, controller, ChainType::TwoWay);
    const std::vector<SlotId> copied = topology.stage({StageKind::Branch, 1});
    ASSERT_EQ(copied.size(), 1u);

    applier.switch_chain_type(topology, controller, ChainType::Single);

    EXPECT_EQ(topology.stage({StageKind::Branch, 0}), (std::vector<SlotId>{amp}));
    EXPECT_TRUE(controller.descriptor_id(copied[0]).empty());
    EXPECT_EQ(gear_slots(controller, applier), (std::vector<SlotId>{amp}));
}

TEST_F(Small_TopologyApplier, TopologyApplierDiscardedBranchSlotsAreRemovedFromTheRig)
{
    RigController controller = rig(test_info());
    const SlotId branch_slot = controller.add_node("sushidsp.jcm800", false);
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({StageKind::Branch, 1}, 0, branch_slot);
    TopologyApplier applier;
    applier.apply(topology, controller);

    EXPECT_EQ(topology.set_chain_type(ChainType::Single).discarded,
              (std::vector<SlotId>{branch_slot}));
    applier.apply(topology, controller);

    EXPECT_TRUE(controller.descriptor_id(branch_slot).empty());
    EXPECT_TRUE(controller.slot_order().empty());
}

TEST_F(Small_TopologyApplier, AnEmptySingleTopologyAppliesToAnEmptyRig)
{
    RigController controller = rig(test_info());
    RigTopology topology;
    TopologyApplier applier;

    EXPECT_NO_THROW(applier.apply(topology, controller));

    EXPECT_TRUE(controller.slot_order().empty());
}

TEST_F(Small_TopologyApplier, AnEmptyTwoWayTopologyJoinsItsSplitterToItsMixerByOneEdge)
{
    RigController controller = rig(test_info());
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    TopologyApplier applier;

    EXPECT_NO_THROW(applier.apply(topology, controller));

    const SlotId splitter = find_descriptor(controller, "sushidsp.splitter");
    const SlotId mixer = find_descriptor(controller, "sushidsp.mixer");
    EXPECT_EQ(controller.edges(), (EdgeList{{splitter, mixer}}));
}

TEST_F(Small_TopologyApplier, TopologyApplierLeavesThePreviousGraphWhenApplyThrows)
{
    RigController controller = rig(test_info());
    const SlotId a = controller.append("sushidsp.tubescreamer");
    const SlotId b = controller.append("sushidsp.jcm800");
    const SushiDSP::RigGraph* graph_before = controller.active_graph();

    // A slot listed twice closes a cycle, which RigGraph::compile rejects.
    RigTopology topology = RigTopology::from_linear_order({a, b, a});
    TopologyApplier applier;

    EXPECT_THROW(applier.apply(topology, controller), std::runtime_error);

    EXPECT_EQ(topology.active_slots(), (std::vector<SlotId>{a, b, a}));
    EXPECT_EQ(controller.active_graph(), graph_before);
    EXPECT_EQ(controller.slot_order(), (std::vector<SlotId>{a, b}));
    EXPECT_EQ(controller.edges(), (EdgeList{{a, b}}));
}

TEST_F(Small_TopologyApplier, TopologyApplierRollbackRestoresRemovedSlotsUnderNewIds)
{
    RigController controller = rig(test_info());
    const SlotId a = controller.append("sushidsp.tubescreamer");
    const SlotId b = controller.append("sushidsp.jcm800");
    const SlotId stray = controller.append("sushidsp.tubescreamer");
    RigTopology topology = RigTopology::from_linear_order({a, b, a});
    TopologyApplier applier;

    EXPECT_THROW(applier.apply(topology, controller), std::runtime_error);

    const std::vector<SlotId> order = controller.slot_order();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], b);
    EXPECT_NE(order[2], stray);
    EXPECT_EQ(controller.descriptor_id(order[2]), "sushidsp.tubescreamer");
    EXPECT_EQ(controller.edges(), (EdgeList{{a, b}, {b, order[2]}}));
}

TEST_F(Small_TopologyApplier, TheCabinetSectionIsWiredFromTheCabinetToTheMasterStrip)
{
    CabinetRig built(catalogue());

    const HiddenSlots hidden = built.applier.mixer_slots();

    ASSERT_TRUE(hidden.room.has_value());
    ASSERT_TRUE(hidden.listener_strip.has_value());
    ASSERT_TRUE(hidden.amp_bus.has_value());
    ASSERT_TRUE(hidden.master_strip.has_value());
    EXPECT_FALSE(hidden.bus.has_value());
    EXPECT_EQ(count_descriptor(built.rig, "sushidsp.room"), 1u);
    // The cabinet feeds the room beside the amp bus; the listener's port joins the amp bus too.
    EXPECT_TRUE(has_edge(built.rig, built.cabinet, *hidden.room));
    EXPECT_TRUE(has_edge(built.rig, *hidden.room, *hidden.listener_strip, 0));
    EXPECT_TRUE(has_edge(built.rig, built.cabinet, *hidden.amp_bus));
    EXPECT_TRUE(has_edge(built.rig, *hidden.listener_strip, *hidden.amp_bus));
    EXPECT_TRUE(has_edge(built.rig, *hidden.amp_bus, *hidden.master_strip));
    EXPECT_EQ(hidden.amp_bus_feeders,
              (std::vector<SlotId>{built.cabinet, *hidden.listener_strip}));
}

TEST_F(Small_TopologyApplier, TheRoomHoldsTheCabinetUnderItsOwnSlotAndNoMicrophone)
{
    CabinetRig built(catalogue());

    const SushiDSP::Room::Arrangement arrangement =
        built.rig.room_arrangement(*built.applier.mixer_slots().room);

    ASSERT_EQ(arrangement.sources().size(), 1u);
    EXPECT_EQ(arrangement.sources()[0].id, built.cabinet);
    EXPECT_TRUE(arrangement.microphones().empty());
}

TEST_F(Small_TopologyApplier, ANewRigSoundsAsItsCabinetDoesWithTheListenerMuted)
{
    CabinetRig built(catalogue());

    const HiddenSlots hidden = built.applier.mixer_slots();

    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_0"), 0.0f);
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_1"), 1.0f);
}

TEST_F(Small_TopologyApplier, TopologyApplierGivesTheRoomNoSourceForAPathWithNoCabinet)
{
    RigController controller = rig(test_info());
    const SlotId amp = controller.add_node("sushidsp.tubescreamer", false);
    RigTopology topology = RigTopology::from_linear_order({amp});
    TopologyApplier applier;

    applier.apply(topology, controller);

    const HiddenSlots hidden = applier.mixer_slots();
    ASSERT_TRUE(hidden.room.has_value());
    EXPECT_TRUE(controller.room_arrangement(*hidden.room).sources().empty());
    EXPECT_FALSE(has_edge(controller, amp, *hidden.room));
    EXPECT_TRUE(has_edge(controller, amp, *hidden.amp_bus));
}

TEST_F(Small_TopologyApplier, AMicrophoneIsWiredFromItsRoomPortThroughItsStripToTheBus)
{
    CabinetRig built(catalogue());

    const SlotId microphone = built.applier.add_microphone(built.topology, built.rig);

    const HiddenSlots hidden = built.applier.mixer_slots();
    ASSERT_EQ(hidden.microphones.size(), 1u);
    EXPECT_EQ(hidden.microphones[0].microphone, microphone);
    ASSERT_TRUE(hidden.bus.has_value());
    EXPECT_EQ(built.rig.descriptor_id(microphone), "sushidsp.microphone");
    EXPECT_TRUE(has_edge(built.rig, *hidden.room, microphone, 1));
    EXPECT_TRUE(has_edge(built.rig, microphone, hidden.microphones[0].strip));
    EXPECT_TRUE(has_edge(built.rig, hidden.microphones[0].strip, *hidden.bus));
    EXPECT_TRUE(has_edge(built.rig, *hidden.bus, *hidden.amp_bus));
}

TEST_F(Small_TopologyApplier, AMicrophoneStandsInFrontOfTheCabinetInTheRoom)
{
    CabinetRig built(catalogue());

    const SlotId microphone = built.applier.add_microphone(built.topology, built.rig);

    const SushiDSP::Room::Arrangement arrangement =
        built.rig.room_arrangement(*built.applier.mixer_slots().room);
    ASSERT_EQ(arrangement.microphones().size(), 1u);
    EXPECT_EQ(arrangement.microphones()[0].id, microphone);
    EXPECT_GT(arrangement.microphones()[0].pose.position.x,
              arrangement.sources()[0].pose.position.x);
}

TEST_F(Small_TopologyApplier, AddingAMicrophoneMovesTheListenersMuteFromInputOneToInputTwo)
{
    CabinetRig built(catalogue());

    built.applier.add_microphone(built.topology, built.rig);

    const HiddenSlots hidden = built.applier.mixer_slots();
    EXPECT_EQ(hidden.amp_bus_feeders,
              (std::vector<SlotId>{built.cabinet, *hidden.bus, *hidden.listener_strip}));
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_1"), 0.0f);
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_2"), 1.0f);
}

TEST_F(Small_TopologyApplier, ANinthMicrophoneIsRefusedWithTheLimitInTheMessage)
{
    CabinetRig built(catalogue());
    add_microphones(built, SushiDSP::Room::Arrangement::K_MAX_MICROPHONES);

    const std::string refusal = refusal_of_a_microphone(built);

    const std::string limit =
        "at most " + std::to_string(SushiDSP::Room::Arrangement::K_MAX_MICROPHONES) + " microphones";
    EXPECT_NE(refusal.find(limit), std::string::npos) << refusal;
}

TEST_F(Small_TopologyApplier, ARefusedNinthMicrophoneLeavesTheRigAsItWas)
{
    CabinetRig built(catalogue());
    add_microphones(built, SushiDSP::Room::Arrangement::K_MAX_MICROPHONES);
    const std::vector<SlotId> before = built.rig.slot_order();

    EXPECT_THROW(built.applier.add_microphone(built.topology, built.rig), std::runtime_error);

    EXPECT_EQ(built.rig.slot_order(), before);
    EXPECT_EQ(built.applier.mixer_slots().microphones.size(),
              SushiDSP::Room::Arrangement::K_MAX_MICROPHONES);
}

TEST_F(Small_TopologyApplier, TopologyApplierRefusesAMicrophoneForARigWithNoGear)
{
    RigController controller = rig(test_info());
    RigTopology topology;
    TopologyApplier applier;
    applier.apply(topology, controller);

    EXPECT_THROW(applier.add_microphone(topology, controller), std::runtime_error);
    EXPECT_TRUE(controller.slot_order().empty());
}

TEST_F(Small_TopologyApplier, ARoomMicrophoneFeedsTheRoomStripInsteadOfTheBus)
{
    CabinetRig built(catalogue());

    const SlotId overhead = add_a_close_and_a_room_microphone(built);

    const HiddenSlots hidden = built.applier.mixer_slots();
    ASSERT_TRUE(hidden.room_sum.has_value());
    ASSERT_TRUE(hidden.room_strip.has_value());
    ASSERT_EQ(hidden.microphones.size(), 2u);
    ASSERT_EQ(hidden.microphones[1].microphone, overhead);
    const SlotId overhead_strip = hidden.microphones[1].strip;
    EXPECT_TRUE(has_edge(built.rig, overhead_strip, *hidden.room_sum));
    EXPECT_FALSE(has_edge(built.rig, overhead_strip, *hidden.bus));
    EXPECT_TRUE(has_edge(built.rig, *hidden.room_sum, *hidden.room_strip));
    EXPECT_TRUE(has_edge(built.rig, *hidden.room_strip, *hidden.bus));
    EXPECT_EQ(hidden.bus_feeders,
              (std::vector<SlotId>{hidden.microphones[0].strip, *hidden.room_strip}));
}

TEST_F(Small_TopologyApplier, ClearingTheRoomFlagTakesTheRoomSumAndItsStripAway)
{
    CabinetRig built(catalogue());
    const SlotId overhead = add_a_close_and_a_room_microphone(built);

    built.rig.set_parameter(overhead, "room", 0.0f);
    built.applier.apply(built.topology, built.rig);

    EXPECT_FALSE(built.applier.mixer_slots().room_strip.has_value());
    EXPECT_EQ(count_descriptor(built.rig, "sushidsp.stereo_strip"), 2u);
}

TEST_F(Small_TopologyApplier, RemovingAMicrophoneRemovesItsSlotAndKeepsTheOther)
{
    CabinetRig built(catalogue());
    const MicrophonePair pair = add_two_microphones(built);

    built.applier.remove_microphone(built.topology, built.rig, pair.first);

    const HiddenSlots hidden = built.applier.mixer_slots();
    ASSERT_EQ(hidden.microphones.size(), 1u);
    EXPECT_EQ(hidden.microphones[0].microphone, pair.second);
    EXPECT_TRUE(built.rig.descriptor_id(pair.first).empty());
}

TEST_F(Small_TopologyApplier, TheMicrophoneLeftAfterARemovalKeepsItsPlaceInTheRoom)
{
    CabinetRig built(catalogue());
    const MicrophonePair pair = add_two_microphones(built);
    assert_second_microphone_raised(built, pair);

    built.applier.remove_microphone(built.topology, built.rig, pair.first);

    const SushiDSP::Room::Arrangement after =
        built.rig.room_arrangement(*built.applier.mixer_slots().room);
    ASSERT_EQ(after.microphones().size(), 1u);
    EXPECT_EQ(after.microphones()[0].id, pair.second);
    EXPECT_EQ(after.microphones()[0].pose.position.z, K_RAISED_HEIGHT);
}

TEST_F(Small_TopologyApplier, TheMicrophoneLeftAfterARemovalMovesDownToTheFirstRoomPort)
{
    CabinetRig built(catalogue());
    const MicrophonePair pair = add_two_microphones(built);

    built.applier.remove_microphone(built.topology, built.rig, pair.first);

    EXPECT_TRUE(has_edge(built.rig, *built.applier.mixer_slots().room, pair.second, 1));
}

TEST_F(Small_TopologyApplier, RemovingAMicrophoneThatIsGoneIsRefused)
{
    CabinetRig built(catalogue());
    const MicrophonePair pair = add_two_microphones(built);
    built.applier.remove_microphone(built.topology, built.rig, pair.first);

    EXPECT_THROW(built.applier.remove_microphone(built.topology, built.rig, pair.first),
                 std::runtime_error);
}

TEST_F(Small_TopologyApplier, ThreeWayPutsOneRoomSourceOnTheCabinetOfEachPath)
{
    CabinetRig built(catalogue());

    built.applier.switch_chain_type(built.topology, built.rig, ChainType::ThreeWay);

    const SushiDSP::Room::Arrangement arrangement =
        built.rig.room_arrangement(*built.applier.mixer_slots().room);
    ASSERT_EQ(arrangement.sources().size(), 3u);
    EXPECT_EQ(arrangement.sources()[0].id, built.topology.stage({StageKind::Branch, 0})[1]);
    EXPECT_EQ(arrangement.sources()[1].id, built.topology.stage({StageKind::Branch, 1})[1]);
    EXPECT_EQ(arrangement.sources()[2].id, built.topology.stage({StageKind::Branch, 2})[1]);
}

TEST_F(Small_TopologyApplier, ThreeWayFeedsTheRoomFromTheCabinetOfEachPath)
{
    CabinetRig built(catalogue());

    built.applier.switch_chain_type(built.topology, built.rig, ChainType::ThreeWay);

    const SlotId room = *built.applier.mixer_slots().room;
    EXPECT_TRUE(has_edge(built.rig, built.topology.stage({StageKind::Branch, 0})[1], room));
    EXPECT_TRUE(has_edge(built.rig, built.topology.stage({StageKind::Branch, 1})[1], room));
    EXPECT_TRUE(has_edge(built.rig, built.topology.stage({StageKind::Branch, 2})[1], room));
}

TEST_F(Small_TopologyApplier, ThreeWayStandsTheCopiedCabinetsBesideTheFirstNotOnIt)
{
    CabinetRig built(catalogue());

    built.applier.switch_chain_type(built.topology, built.rig, ChainType::ThreeWay);

    const SushiDSP::Room::Arrangement arrangement =
        built.rig.room_arrangement(*built.applier.mixer_slots().room);
    ASSERT_EQ(arrangement.sources().size(), 3u);
    EXPECT_NE(arrangement.sources()[1].pose.position.y, arrangement.sources()[0].pose.position.y);
}

TEST_F(Small_TopologyApplier, ThreeWayMakesTheChainsMixerTheCabinetInput)
{
    CabinetRig built(catalogue());

    built.applier.switch_chain_type(built.topology, built.rig, ChainType::ThreeWay);

    const HiddenSlots hidden = built.applier.mixer_slots();
    EXPECT_EQ(hidden.amp_bus_feeders.front(), *hidden.mixer);
}

TEST_F(Small_TopologyApplier, ADiAfterThePostEffectsJoinsOnTheMasterBus)
{
    CabinetRig built(catalogue());

    built.applier.set_di(built.topology, built.rig, true);

    const HiddenSlots hidden = built.applier.mixer_slots();
    ASSERT_TRUE(hidden.di_strip.has_value());
    ASSERT_TRUE(hidden.master_bus.has_value());
    EXPECT_TRUE(has_edge(built.rig, *hidden.di_strip, *hidden.master_bus));
    EXPECT_TRUE(has_edge(built.rig, *hidden.amp_bus, *hidden.master_bus));
    EXPECT_TRUE(has_edge(built.rig, *hidden.master_bus, *hidden.master_strip));
}

TEST_F(Small_TopologyApplier, ADiBeforeThePostEffectsJoinsOnTheAmpBus)
{
    CabinetRig built(catalogue());
    built.applier.set_di(built.topology, built.rig, true);

    built.applier.set_di_position(built.topology, built.rig, DiPosition::BeforeEffects);

    const HiddenSlots hidden = built.applier.mixer_slots();
    ASSERT_TRUE(hidden.di_strip.has_value());
    EXPECT_FALSE(hidden.master_bus.has_value());
    EXPECT_TRUE(has_edge(built.rig, *hidden.di_strip, *hidden.amp_bus));
    EXPECT_TRUE(has_edge(built.rig, *hidden.amp_bus, *hidden.master_strip));
}

TEST_F(Small_TopologyApplier, ClosingTheDiTakesItsStripAway)
{
    CabinetRig built(catalogue());
    built.applier.set_di(built.topology, built.rig, true);

    built.applier.set_di(built.topology, built.rig, false);

    EXPECT_FALSE(built.applier.mixer_slots().di_strip.has_value());
}

TEST_F(Small_TopologyApplier, TopologyApplierTakesTheSectionAwayWithTheLastSlot)
{
    CabinetRig built(catalogue());
    built.applier.add_microphone(built.topology, built.rig);

    built.topology.remove(built.amp);
    built.topology.remove(built.cabinet);
    built.applier.apply(built.topology, built.rig);

    EXPECT_TRUE(built.rig.slot_order().empty());
    EXPECT_FALSE(built.applier.mixer_slots().room.has_value());
}

TEST_F(Small_TopologyApplier, TheRigRecordsItsShapeAfterAnApply)
{
    CabinetRig built(catalogue());

    built.applier.add_microphone(built.topology, built.rig);

    ASSERT_TRUE(built.rig.shape().has_value());
    const RigShape shape = *built.rig.shape();
    EXPECT_EQ(shape.chain_type, ChainType::Single);
    ASSERT_EQ(shape.branches.size(), 1u);
    EXPECT_EQ(shape.branches[0], (std::vector<SlotId>{built.amp, built.cabinet}));
    EXPECT_EQ(shape.hidden.microphones.size(), 1u);
}

TEST_F(Small_TopologyApplier, AFreshApplierThatAdoptsTheShapeKeepsEveryHiddenSlot)
{
    CabinetRig built(catalogue());
    built.applier.add_microphone(built.topology, built.rig);
    const RigShape shape = *built.rig.shape();
    const std::vector<SlotId> before = built.rig.slot_order();
    TopologyApplier fresh;
    RigTopology topology = RigTopology::from_stages(shape.chain_type, shape.branches, shape.post,
                                                    shape.di, shape.di_position);

    fresh.adopt(shape.hidden, built.rig);
    fresh.apply(topology, built.rig);

    EXPECT_EQ(built.rig.slot_order(), before);
    EXPECT_EQ(fresh.mixer_slots().microphones.size(), 1u);
}

TEST_F(Small_TopologyApplier, MigratingAnOldRoomCarriesItsGeometryIntoTheArrangement)
{
    CabinetRig built(catalogue());

    const LegacyRoom legacy = migrate_a_room_heard_by_the_listener(built);

    const SushiDSP::Room::Arrangement arrangement =
        built.rig.room_arrangement(*built.applier.mixer_slots().room);
    EXPECT_EQ(arrangement.shell().dimensions.x, legacy.scene.dimensions.x);
    EXPECT_EQ(arrangement.sources()[0].pose.position.x, legacy.scene.source_position.x);
    EXPECT_EQ(arrangement.listener().position.x, legacy.scene.listener_position.x);
    EXPECT_TRUE(arrangement.microphones().empty());
}

TEST_F(Small_TopologyApplier, MigratingAnOldRoomHeardByTheListenerPutsItsLevelOnTheListenersStrip)
{
    CabinetRig built(catalogue());

    const LegacyRoom legacy = migrate_a_room_heard_by_the_listener(built);

    EXPECT_EQ(built.rig.get_parameter(*built.applier.mixer_slots().listener_strip, "level"),
              legacy.level_db);
}

TEST_F(Small_TopologyApplier, MigratingAnOldRoomHeardByTheListenerMutesTheCabinetAndHearsTheListener)
{
    CabinetRig built(catalogue());

    migrate_a_room_heard_by_the_listener(built);

    const HiddenSlots hidden = built.applier.mixer_slots();
    EXPECT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_0"), 1.0f);
    EXPECT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_1"), 0.0f);
}

TEST_F(Small_TopologyApplier, MigratingAnOldRoomHeardByAMicrophonePlacesOneMicrophoneWhereItWas)
{
    CabinetRig built(catalogue());

    const LegacyRoom legacy = migrate_a_room_heard_by_a_microphone(built);

    const SushiDSP::Room::Arrangement arrangement =
        built.rig.room_arrangement(*built.applier.mixer_slots().room);
    ASSERT_EQ(arrangement.microphones().size(), 1u);
    EXPECT_EQ(arrangement.microphones()[0].pose.position.x, legacy.scene.microphone_position.x);
    EXPECT_EQ(arrangement.microphones()[0].pattern, legacy.scene.microphone_pattern);
}

TEST_F(Small_TopologyApplier, MigratingAnOldRoomHeardByAMicrophoneSetsTheMicrophonesPattern)
{
    CabinetRig built(catalogue());

    migrate_a_room_heard_by_a_microphone(built);

    const HiddenSlots hidden = built.applier.mixer_slots();
    ASSERT_EQ(hidden.microphones.size(), 1u);
    EXPECT_EQ(built.rig.get_parameter(hidden.microphones[0].microphone, "pattern"),
              static_cast<float>(static_cast<int>(SushiDSP::room::PolarPattern::Omni)));
}

// The bound of the level is derived in tests/applications/host_gui/README.md.
TEST_F(Small_TopologyApplier, MigratingAnOldRoomHeardByAMicrophoneRestoresItsLevelOnTheStrip)
{
    CabinetRig built(catalogue());

    migrate_a_room_heard_by_a_microphone(built);

    const HiddenSlots hidden = built.applier.mixer_slots();
    const double centring_db = 10.0 * std::log10(2.0);
    const double constant_error = std::fabs(3.0103 - centring_db) +
                                  TestSupport::unit_roundoff<float>() * 3.0103;
    const float level = built.rig.get_parameter(hidden.microphones[0].strip, "level");
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        level, static_cast<double>(K_LEGACY_MICROPHONE_LEVEL_DB) + centring_db, constant_error)));
}

TEST_F(Small_TopologyApplier, MigratingAnOldRoomHeardByAMicrophoneHearsTheBusAndMutesTheRest)
{
    CabinetRig built(catalogue());

    migrate_a_room_heard_by_a_microphone(built);

    const HiddenSlots hidden = built.applier.mixer_slots();
    ASSERT_EQ(hidden.amp_bus_feeders.size(), 3u);
    EXPECT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_0"), 1.0f);
    EXPECT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_1"), 0.0f);
    EXPECT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_2"), 1.0f);
}
