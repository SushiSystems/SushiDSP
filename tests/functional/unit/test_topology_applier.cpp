/**************************************************************************/
/* test_topology_applier.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
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

#include "RigController.hpp"
#include "RigTopology.hpp"
#include "TopologyApplier.hpp"

namespace
{
    using SlotId = RigController::SlotId;
    using EdgeList = std::vector<std::pair<SlotId, SlotId>>;

    /// @brief Builds the prepare block every case's controller runs at.
    /// @return A mono 48 kHz block of 64 frames.
    SushiDSP::PrepareInfo test_info()
    {
        return SushiDSP::PrepareInfo{ 48000.0, 64, 1 };
    }

    /// @brief Counts the rig's slots carrying a descriptor.
    /// @return How many live slots hold `descriptor_id`.
    std::size_t count_descriptor(const RigController& rig, const std::string& descriptor_id)
    {
        const std::vector<SlotId> order = rig.slot_order();
        return static_cast<std::size_t>(std::count_if(order.begin(), order.end(),
            [&](SlotId slot) { return rig.descriptor_id(slot) == descriptor_id; }));
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
}

TEST(Unit_Dsp, TopologyApplierSeriesWiresALinearChain)
{
    RigController rig(test_info());
    const SlotId a = rig.add_node("sushidsp.tubescreamer", false);
    const SlotId b = rig.add_node("sushidsp.jcm800", false);
    const SlotId c = rig.add_node("sushidsp.tubescreamer", false);
    RigTopology topology = RigTopology::from_series({ c, a, b });
    TopologyApplier applier;

    applier.apply(topology, rig);

    EXPECT_EQ(rig.edges(), (EdgeList{ { c, a }, { a, b } }));
    EXPECT_EQ(rig.slot_order().size(), 3u);
}

TEST(Unit_Dsp, TopologyApplierTwoWayAddsOneSplitterAndOneMixer)
{
    RigController rig(test_info());
    const SlotId drive = rig.add_node("sushidsp.tubescreamer", false);
    const SlotId amp = rig.add_node("sushidsp.jcm800", false);
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({ StageKind::Pre }, 0, drive);
    topology.insert({ StageKind::Branch, 0 }, 0, amp);
    TopologyApplier applier;

    applier.apply(topology, rig);
    applier.apply(topology, rig);

    EXPECT_EQ(count_descriptor(rig, "sushidsp.splitter"), 1u);
    EXPECT_EQ(count_descriptor(rig, "sushidsp.mixer"), 1u);
    const SlotId splitter = find_descriptor(rig, "sushidsp.splitter");
    const SlotId mixer = find_descriptor(rig, "sushidsp.mixer");
    EXPECT_EQ(rig.edges(), (EdgeList{ { drive, splitter }, { splitter, amp }, { amp, mixer },
                                      { splitter, mixer } }));
}

TEST(Unit_Dsp, TopologyApplierBackToSeriesRestoresParkedParameters)
{
    RigController rig(test_info());
    const SlotId amp = rig.append("sushidsp.jcm800");
    rig.set_parameter(amp, "preamp", 0.9f);
    const SlotId drive = rig.append("sushidsp.tubescreamer");
    RigTopology topology = RigTopology::from_series({ amp, drive });
    TopologyApplier applier;

    topology.set_chain_type(ChainType::TwoWay);
    applier.apply(topology, rig);
    EXPECT_EQ(applier.parked_count(), 2u);
    EXPECT_EQ(rig.slot_order().size(), 2u);
    EXPECT_EQ(count_descriptor(rig, "sushidsp.jcm800"), 0u);

    topology.set_chain_type(ChainType::Series);
    applier.apply(topology, rig);

    EXPECT_EQ(applier.parked_count(), 0u);
    const std::vector<SlotId> active = topology.active_slots();
    ASSERT_EQ(active.size(), 2u);
    EXPECT_EQ(rig.descriptor_id(active[0]), "sushidsp.jcm800");
    EXPECT_FLOAT_EQ(rig.get_parameter(active[0], "preamp"), 0.9f);
    EXPECT_EQ(rig.slot_order().size(), 2u);
    EXPECT_EQ(rig.edges(), (EdgeList{ { active[0], active[1] } }));
}

TEST(Unit_Dsp, TopologyApplierDiscardedBranchSlotsAreRemovedFromTheRig)
{
    RigController rig(test_info());
    const SlotId branch_slot = rig.add_node("sushidsp.jcm800", false);
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({ StageKind::Branch, 1 }, 0, branch_slot);
    TopologyApplier applier;
    applier.apply(topology, rig);

    EXPECT_EQ(topology.set_chain_type(ChainType::ThreeWay), (std::vector<SlotId>{ branch_slot }));
    applier.apply(topology, rig);

    EXPECT_TRUE(rig.descriptor_id(branch_slot).empty());
    EXPECT_EQ(rig.slot_order().size(), 2u);
}

TEST(Unit_Dsp, TopologyApplierEmptyTopologiesApplyWithoutThrowing)
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
    EXPECT_EQ(rig.edges(), (EdgeList{ { splitter, mixer } }));
}

TEST(Unit_Dsp, TopologyApplierLeavesThePreviousGraphWhenApplyThrows)
{
    RigController rig(test_info());
    const SlotId a = rig.append("sushidsp.tubescreamer");
    const SlotId b = rig.append("sushidsp.jcm800");
    const SushiDSP::RigGraph* graph_before = rig.active_graph();

    // A slot listed twice closes a cycle, which RigGraph::compile rejects.
    RigTopology topology = RigTopology::from_series({ a, b, a });
    TopologyApplier applier;

    EXPECT_THROW(applier.apply(topology, rig), std::runtime_error);

    EXPECT_EQ(topology.active_slots(), (std::vector<SlotId>{ a, b, a }));
    EXPECT_EQ(rig.active_graph(), graph_before);
    EXPECT_EQ(rig.slot_order(), (std::vector<SlotId>{ a, b }));
    EXPECT_EQ(rig.edges(), (EdgeList{ { a, b } }));
}

TEST(Unit_Dsp, TopologyApplierRollbackRestoresRemovedSlotsUnderNewIds)
{
    RigController rig(test_info());
    const SlotId a = rig.append("sushidsp.tubescreamer");
    const SlotId b = rig.append("sushidsp.jcm800");
    const SlotId stray = rig.append("sushidsp.tubescreamer");
    RigTopology topology = RigTopology::from_series({ a, b, a });
    TopologyApplier applier;

    EXPECT_THROW(applier.apply(topology, rig), std::runtime_error);

    const std::vector<SlotId> order = rig.slot_order();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], b);
    EXPECT_NE(order[2], stray);
    EXPECT_EQ(rig.descriptor_id(order[2]), "sushidsp.tubescreamer");
    EXPECT_EQ(rig.edges(), (EdgeList{ { a, b }, { b, order[2] } }));
}
