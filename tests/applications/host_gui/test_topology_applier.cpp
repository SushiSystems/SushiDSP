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

/** @file test_topology_applier.cpp
 *  @brief Tests that TopologyApplier compiles a RigTopology into RigController slots and edges.
 *  @author Mustafa Garip
 */

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "rig_controller.hpp"
#include "rig_topology.hpp"
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
            RigController rig{test_info()};
            SlotId amp = 0;
            SlotId cabinet = 0;
            RigTopology topology;
            TopologyApplier applier;

            CabinetRig()
            {
                amp = rig.add_node("sushidsp.tubescreamer", false);
                cabinet = rig.add_node("sushidsp.cabinet", false);
                topology = RigTopology::from_linear_order({amp, cabinet});
                applier.apply(topology, rig);
            }
    };
} // namespace

TEST(TopologyApplier, TopologyApplierSinglePathWiresALinearChain)
{
    RigController rig(test_info());
    const SlotId a = rig.add_node("sushidsp.tubescreamer", false);
    const SlotId b = rig.add_node("sushidsp.jcm800", false);
    const SlotId c = rig.add_node("sushidsp.tubescreamer", false);
    RigTopology topology = RigTopology::from_linear_order({c, a, b});
    TopologyApplier applier;

    applier.apply(topology, rig);

    EXPECT_EQ(chain_edges(rig, applier), (EdgeList{{c, a}, {a, b}}));
    EXPECT_EQ(gear_slots(rig, applier).size(), 3u);
}

TEST(TopologyApplier, TopologyApplierTwoWayAddsOneSplitterAndOneMixer)
{
    RigController rig(test_info());
    const SlotId drive = rig.add_node("sushidsp.tubescreamer", false);
    const SlotId amp = rig.add_node("sushidsp.jcm800", false);
    RigTopology topology = RigTopology::from_linear_order({drive, amp});
    TopologyApplier applier;

    applier.switch_chain_type(topology, rig, ChainType::TwoWay);
    applier.apply(topology, rig);

    EXPECT_EQ(count_descriptor(rig, "sushidsp.splitter"), 1u);
    EXPECT_EQ(count_descriptor(rig, "sushidsp.mixer"), 1u);
    const SlotId splitter = find_descriptor(rig, "sushidsp.splitter");
    const SlotId mixer = find_descriptor(rig, "sushidsp.mixer");
    const std::vector<SlotId> copied = topology.stage({StageKind::Branch, 1});
    ASSERT_EQ(copied.size(), 2u);
    EXPECT_EQ(chain_edges(rig, applier), (EdgeList{{splitter, drive},
                                     {drive, amp},
                                     {amp, mixer},
                                     {splitter, copied[0]},
                                     {copied[0], copied[1]},
                                     {copied[1], mixer}}));
}

TEST(TopologyApplier, TopologyApplierOpeningAPathCopiesTheMainPathWithItsParameters)
{
    RigController rig(test_info());
    const SlotId amp = rig.append("sushidsp.jcm800");
    rig.set_parameter(amp, "preamp", 0.9f);
    RigTopology topology = RigTopology::from_linear_order({amp});
    TopologyApplier applier;

    applier.switch_chain_type(topology, rig, ChainType::TwoWay);

    // The opened path carries a copy of the main path. See tests/applications/host_gui/README.md.
    EXPECT_EQ(topology.stage({StageKind::Branch, 0}), (std::vector<SlotId>{amp}));
    const std::vector<SlotId> copied = topology.stage({StageKind::Branch, 1});
    ASSERT_EQ(copied.size(), 1u);
    EXPECT_NE(copied[0], amp);
    EXPECT_EQ(rig.descriptor_id(copied[0]), "sushidsp.jcm800");
    EXPECT_FLOAT_EQ(rig.get_parameter(copied[0], "preamp"), 0.9f);
    EXPECT_FLOAT_EQ(rig.get_parameter(amp, "preamp"), 0.9f);
}

TEST(TopologyApplier, TopologyApplierClosingAPathKeepsTheMainPathAndDropsTheCopy)
{
    RigController rig(test_info());
    const SlotId amp = rig.append("sushidsp.jcm800");
    RigTopology topology = RigTopology::from_linear_order({amp});
    TopologyApplier applier;

    applier.switch_chain_type(topology, rig, ChainType::TwoWay);
    const std::vector<SlotId> copied = topology.stage({StageKind::Branch, 1});
    ASSERT_EQ(copied.size(), 1u);

    applier.switch_chain_type(topology, rig, ChainType::Single);

    EXPECT_EQ(topology.stage({StageKind::Branch, 0}), (std::vector<SlotId>{amp}));
    EXPECT_TRUE(rig.descriptor_id(copied[0]).empty());
    EXPECT_EQ(gear_slots(rig, applier), (std::vector<SlotId>{amp}));
}

TEST(TopologyApplier, TopologyApplierDiscardedBranchSlotsAreRemovedFromTheRig)
{
    RigController rig(test_info());
    const SlotId branch_slot = rig.add_node("sushidsp.jcm800", false);
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({StageKind::Branch, 1}, 0, branch_slot);
    TopologyApplier applier;
    applier.apply(topology, rig);

    EXPECT_EQ(topology.set_chain_type(ChainType::Single).discarded,
              (std::vector<SlotId>{branch_slot}));
    applier.apply(topology, rig);

    EXPECT_TRUE(rig.descriptor_id(branch_slot).empty());
    EXPECT_TRUE(rig.slot_order().empty());
}

TEST(TopologyApplier, TopologyApplierEmptyTopologiesApplyWithoutThrowing)
{
    RigController rig(test_info());
    RigTopology topology;
    TopologyApplier applier;

    EXPECT_NO_THROW(applier.apply(topology, rig));
    EXPECT_TRUE(rig.slot_order().empty());

    topology.set_chain_type(ChainType::TwoWay);
    EXPECT_NO_THROW(applier.apply(topology, rig));
    const SlotId splitter = find_descriptor(rig, "sushidsp.splitter");
    const SlotId mixer = find_descriptor(rig, "sushidsp.mixer");
    EXPECT_EQ(rig.edges(), (EdgeList{{splitter, mixer}}));
}

TEST(TopologyApplier, TopologyApplierLeavesThePreviousGraphWhenApplyThrows)
{
    RigController rig(test_info());
    const SlotId a = rig.append("sushidsp.tubescreamer");
    const SlotId b = rig.append("sushidsp.jcm800");
    const SushiDSP::RigGraph* graph_before = rig.active_graph();

    // A slot listed twice closes a cycle, which RigGraph::compile rejects.
    RigTopology topology = RigTopology::from_linear_order({a, b, a});
    TopologyApplier applier;

    EXPECT_THROW(applier.apply(topology, rig), std::runtime_error);

    EXPECT_EQ(topology.active_slots(), (std::vector<SlotId>{a, b, a}));
    EXPECT_EQ(rig.active_graph(), graph_before);
    EXPECT_EQ(rig.slot_order(), (std::vector<SlotId>{a, b}));
    EXPECT_EQ(rig.edges(), (EdgeList{{a, b}}));
}

TEST(TopologyApplier, TopologyApplierRollbackRestoresRemovedSlotsUnderNewIds)
{
    RigController rig(test_info());
    const SlotId a = rig.append("sushidsp.tubescreamer");
    const SlotId b = rig.append("sushidsp.jcm800");
    const SlotId stray = rig.append("sushidsp.tubescreamer");
    RigTopology topology = RigTopology::from_linear_order({a, b, a});
    TopologyApplier applier;

    EXPECT_THROW(applier.apply(topology, rig), std::runtime_error);

    const std::vector<SlotId> order = rig.slot_order();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], b);
    EXPECT_NE(order[2], stray);
    EXPECT_EQ(rig.descriptor_id(order[2]), "sushidsp.tubescreamer");
    EXPECT_EQ(rig.edges(), (EdgeList{{a, b}, {b, order[2]}}));
}

TEST(TopologyApplier, TopologyApplierBuildsTheCabinetSectionAroundACabinet)
{
    CabinetRig built;
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

    // The room holds the cabinet under the cabinet's own slot, and no microphone.
    const SushiDSP::Room::Arrangement arrangement = built.rig.room_arrangement(*hidden.room);
    ASSERT_EQ(arrangement.sources().size(), 1u);
    EXPECT_EQ(arrangement.sources()[0].id, built.cabinet);
    EXPECT_TRUE(arrangement.microphones().empty());

    // A new rig sounds as its cabinet does: Cabinet is on and the listener is muted.
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_0"), 0.0f);
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_1"), 1.0f);
}

TEST(TopologyApplier, TopologyApplierGivesTheRoomNoSourceForAPathWithNoCabinet)
{
    RigController rig(test_info());
    const SlotId amp = rig.add_node("sushidsp.tubescreamer", false);
    RigTopology topology = RigTopology::from_linear_order({amp});
    TopologyApplier applier;

    applier.apply(topology, rig);

    const HiddenSlots hidden = applier.mixer_slots();
    ASSERT_TRUE(hidden.room.has_value());
    EXPECT_TRUE(rig.room_arrangement(*hidden.room).sources().empty());
    EXPECT_FALSE(has_edge(rig, amp, *hidden.room));
    EXPECT_TRUE(has_edge(rig, amp, *hidden.amp_bus));
}

TEST(TopologyApplier, TopologyApplierAddsAMicrophoneWithItsStripAndItsPort)
{
    CabinetRig built;

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

    // The room holds the microphone under its own slot, in front of the cabinet.
    const SushiDSP::Room::Arrangement arrangement = built.rig.room_arrangement(*hidden.room);
    ASSERT_EQ(arrangement.microphones().size(), 1u);
    EXPECT_EQ(arrangement.microphones()[0].id, microphone);
    EXPECT_GT(arrangement.microphones()[0].pose.position.x,
              arrangement.sources()[0].pose.position.x);

    // The listener's mute followed it from input 1 to input 2; the bus came in unmuted.
    EXPECT_EQ(hidden.amp_bus_feeders,
              (std::vector<SlotId>{built.cabinet, *hidden.bus, *hidden.listener_strip}));
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_1"), 0.0f);
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_2"), 1.0f);
}

TEST(TopologyApplier, TopologyApplierRefusesANinthMicrophoneAndLeavesTheRigAsItWas)
{
    CabinetRig built;
    for (int index = 0; index < 8; ++index)
    {
        built.applier.add_microphone(built.topology, built.rig);
    }
    const std::vector<SlotId> before = built.rig.slot_order();

    try
    {
        built.applier.add_microphone(built.topology, built.rig);
        FAIL() << "a ninth microphone was accepted";
    }
    catch (const std::runtime_error& refusal)
    {
        EXPECT_NE(std::string(refusal.what()).find("at most 8 microphones"), std::string::npos)
            << refusal.what();
    }

    EXPECT_EQ(built.rig.slot_order(), before);
    EXPECT_EQ(built.applier.mixer_slots().microphones.size(), 8u);
}

TEST(TopologyApplier, TopologyApplierRefusesAMicrophoneForARigWithNoGear)
{
    RigController rig(test_info());
    RigTopology topology;
    TopologyApplier applier;
    applier.apply(topology, rig);

    EXPECT_THROW(applier.add_microphone(topology, rig), std::runtime_error);
    EXPECT_TRUE(rig.slot_order().empty());
}

TEST(TopologyApplier, TopologyApplierMovesARoomMicrophoneToTheRoomStrip)
{
    CabinetRig built;
    const SlotId close = built.applier.add_microphone(built.topology, built.rig);
    const SlotId overhead = built.applier.add_microphone(built.topology, built.rig);
    (void)close;

    built.rig.set_parameter(overhead, "room", 1.0f);
    built.applier.apply(built.topology, built.rig);

    const HiddenSlots hidden = built.applier.mixer_slots();
    ASSERT_TRUE(hidden.room_sum.has_value());
    ASSERT_TRUE(hidden.room_strip.has_value());
    const SlotId overhead_strip = hidden.microphones[1].strip;
    EXPECT_TRUE(has_edge(built.rig, overhead_strip, *hidden.room_sum));
    EXPECT_FALSE(has_edge(built.rig, overhead_strip, *hidden.bus));
    EXPECT_TRUE(has_edge(built.rig, *hidden.room_sum, *hidden.room_strip));
    EXPECT_TRUE(has_edge(built.rig, *hidden.room_strip, *hidden.bus));
    EXPECT_EQ(hidden.bus_feeders,
              (std::vector<SlotId>{hidden.microphones[0].strip, *hidden.room_strip}));

    // Clearing the flag takes the Room sum and its strip away again.
    built.rig.set_parameter(overhead, "room", 0.0f);
    built.applier.apply(built.topology, built.rig);
    EXPECT_FALSE(built.applier.mixer_slots().room_strip.has_value());
    EXPECT_EQ(count_descriptor(built.rig, "sushidsp.stereo_strip"), 2u);
}

TEST(TopologyApplier, TopologyApplierRemovesAMicrophoneAndKeepsTheOthersPlaced)
{
    CabinetRig built;
    const SlotId first = built.applier.add_microphone(built.topology, built.rig);
    const SlotId second = built.applier.add_microphone(built.topology, built.rig);
    const SlotId room = *built.applier.mixer_slots().room;
    SushiDSP::Room::Arrangement arrangement = built.rig.room_arrangement(room);
    SushiDSP::Room::MicrophonePlacement moved = *arrangement.find_microphone(second);
    moved.pose.position.z = 1.4;
    ASSERT_TRUE(arrangement.update_microphone(moved));
    std::string reason;
    ASSERT_TRUE(built.rig.set_room_arrangement(room, arrangement, reason)) << reason;

    built.applier.remove_microphone(built.topology, built.rig, first);

    const HiddenSlots hidden = built.applier.mixer_slots();
    ASSERT_EQ(hidden.microphones.size(), 1u);
    EXPECT_EQ(hidden.microphones[0].microphone, second);
    EXPECT_TRUE(built.rig.descriptor_id(first).empty());
    const SushiDSP::Room::Arrangement after = built.rig.room_arrangement(*hidden.room);
    ASSERT_EQ(after.microphones().size(), 1u);
    EXPECT_EQ(after.microphones()[0].id, second);
    EXPECT_EQ(after.microphones()[0].pose.position.z, 1.4);
    // The remaining microphone moved down to the first port.
    EXPECT_TRUE(has_edge(built.rig, *hidden.room, second, 1));

    EXPECT_THROW(built.applier.remove_microphone(built.topology, built.rig, first),
                 std::runtime_error);
}

TEST(TopologyApplier, TopologyApplierPlacesOneSourcePerPathThatCarriesACabinet)
{
    CabinetRig built;

    built.applier.switch_chain_type(built.topology, built.rig, ChainType::ThreeWay);

    const HiddenSlots hidden = built.applier.mixer_slots();
    const SushiDSP::Room::Arrangement arrangement = built.rig.room_arrangement(*hidden.room);
    ASSERT_EQ(arrangement.sources().size(), 3u);
    for (std::size_t path = 0; path < 3; ++path)
    {
        const std::vector<SlotId> slots = built.topology.stage({StageKind::Branch, path});
        ASSERT_EQ(slots.size(), 2u);
        EXPECT_EQ(arrangement.sources()[path].id, slots[1]);
        EXPECT_TRUE(has_edge(built.rig, slots[1], *hidden.room));
    }
    // The first cabinet kept its place; the copies stand beside it, not on it.
    EXPECT_NE(arrangement.sources()[1].pose.position.y, arrangement.sources()[0].pose.position.y);
    // The chain's mixer is the Cabinet input now.
    EXPECT_EQ(hidden.amp_bus_feeders.front(), *hidden.mixer);
}

TEST(TopologyApplier, TopologyApplierJoinsTheDiBeforeOrAfterThePostEffects)
{
    CabinetRig built;

    built.applier.set_di(built.topology, built.rig, true);
    HiddenSlots hidden = built.applier.mixer_slots();
    ASSERT_TRUE(hidden.di_strip.has_value());
    ASSERT_TRUE(hidden.master_bus.has_value());
    EXPECT_TRUE(has_edge(built.rig, *hidden.di_strip, *hidden.master_bus));
    EXPECT_TRUE(has_edge(built.rig, *hidden.amp_bus, *hidden.master_bus));
    EXPECT_TRUE(has_edge(built.rig, *hidden.master_bus, *hidden.master_strip));

    built.applier.set_di_position(built.topology, built.rig, DiPosition::BeforeEffects);
    hidden = built.applier.mixer_slots();
    EXPECT_FALSE(hidden.master_bus.has_value());
    EXPECT_TRUE(has_edge(built.rig, *hidden.di_strip, *hidden.amp_bus));
    EXPECT_TRUE(has_edge(built.rig, *hidden.amp_bus, *hidden.master_strip));

    built.applier.set_di(built.topology, built.rig, false);
    EXPECT_FALSE(built.applier.mixer_slots().di_strip.has_value());
}

TEST(TopologyApplier, TopologyApplierTakesTheSectionAwayWithTheLastSlot)
{
    CabinetRig built;
    built.applier.add_microphone(built.topology, built.rig);

    built.topology.remove(built.amp);
    built.topology.remove(built.cabinet);
    built.applier.apply(built.topology, built.rig);

    EXPECT_TRUE(built.rig.slot_order().empty());
    EXPECT_FALSE(built.applier.mixer_slots().room.has_value());
}

TEST(TopologyApplier, TopologyApplierRecordsTheRigsShapeAndAdoptsItBack)
{
    CabinetRig built;
    built.applier.add_microphone(built.topology, built.rig);
    ASSERT_TRUE(built.rig.shape().has_value());
    const RigShape shape = *built.rig.shape();
    EXPECT_EQ(shape.chain_type, ChainType::Single);
    ASSERT_EQ(shape.branches.size(), 1u);
    EXPECT_EQ(shape.branches[0], (std::vector<SlotId>{built.amp, built.cabinet}));
    EXPECT_EQ(shape.hidden.microphones.size(), 1u);

    // A fresh applier that adopts the shape keeps every hidden slot it names.
    const std::vector<SlotId> before = built.rig.slot_order();
    TopologyApplier fresh;
    RigTopology topology = RigTopology::from_stages(shape.chain_type, shape.branches, shape.post,
                                                    shape.di, shape.di_position);
    fresh.adopt(shape.hidden, built.rig);
    fresh.apply(topology, built.rig);

    EXPECT_EQ(built.rig.slot_order(), before);
    EXPECT_EQ(fresh.mixer_slots().microphones.size(), 1u);
}

TEST(TopologyApplier, TopologyApplierMigratesAnOldRoomHeardByTheListener)
{
    CabinetRig built;
    LegacyRoom legacy;
    legacy.scene = built.rig.room_scene(*built.applier.mixer_slots().room);
    legacy.scene.dimensions = {5.0, 4.0, 2.6};
    legacy.scene.source_position = {1.0, 2.0, 0.6};
    legacy.scene.listener_position = {3.5, 2.0, 1.3};
    legacy.ears = true;
    legacy.level_db = -6.0f;

    built.applier.migrate_legacy_room(built.topology, built.rig, legacy);

    const HiddenSlots hidden = built.applier.mixer_slots();
    const SushiDSP::Room::Arrangement arrangement = built.rig.room_arrangement(*hidden.room);
    EXPECT_EQ(arrangement.shell().dimensions.x, 5.0);
    EXPECT_EQ(arrangement.sources()[0].pose.position.x, 1.0);
    EXPECT_EQ(arrangement.listener().position.x, 3.5);
    EXPECT_TRUE(arrangement.microphones().empty());
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.listener_strip, "level"), -6.0f);
    // Cabinet is muted and the listener is heard.
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_0"), 1.0f);
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_1"), 0.0f);
}

TEST(TopologyApplier, TopologyApplierMigratesAnOldRoomHeardByAMicrophone)
{
    CabinetRig built;
    LegacyRoom legacy;
    legacy.scene = built.rig.room_scene(*built.applier.mixer_slots().room);
    legacy.scene.microphone_position = {2.2, 2.1, 0.7};
    legacy.scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
    legacy.ears = false;
    legacy.level_db = -3.0f;

    built.applier.migrate_legacy_room(built.topology, built.rig, legacy);

    const HiddenSlots hidden = built.applier.mixer_slots();
    ASSERT_EQ(hidden.microphones.size(), 1u);
    const SushiDSP::Room::Arrangement arrangement = built.rig.room_arrangement(*hidden.room);
    ASSERT_EQ(arrangement.microphones().size(), 1u);
    EXPECT_EQ(arrangement.microphones()[0].pose.position.x, 2.2);
    EXPECT_EQ(arrangement.microphones()[0].pattern, SushiDSP::room::PolarPattern::Omni);
    EXPECT_FLOAT_EQ(built.rig.get_parameter(hidden.microphones[0].microphone, "pattern"), 0.0f);
    EXPECT_NEAR(built.rig.get_parameter(hidden.microphones[0].strip, "level"), 0.0103f, 1e-3f);
    // The bus is heard; Cabinet and the listener are muted.
    ASSERT_EQ(hidden.amp_bus_feeders.size(), 3u);
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_0"), 1.0f);
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_1"), 0.0f);
    EXPECT_FLOAT_EQ(built.rig.get_parameter(*hidden.amp_bus, "mute_2"), 1.0f);
}
