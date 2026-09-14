/**************************************************************************/
/* test_rig_topology.cpp                                                  */
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

#include <gtest/gtest.h>

#include "RigTopology.hpp"

TEST(Unit_Dsp, RigTopologySeriesEdgesJoinNeighboursInOrder)
{
    RigTopology topology = RigTopology::from_series({ 4, 7, 9 });
    const auto edges = topology.edges(std::nullopt, std::nullopt);
    ASSERT_EQ(edges.size(), 2u);
    EXPECT_EQ(edges[0], std::make_pair(RigTopology::SlotId{ 4 }, RigTopology::SlotId{ 7 }));
    EXPECT_EQ(edges[1], std::make_pair(RigTopology::SlotId{ 7 }, RigTopology::SlotId{ 9 }));
}

TEST(Unit_Dsp, RigTopologySwitchingToSplitAndBackKeepsTheSeriesChain)
{
    RigTopology topology = RigTopology::from_series({ 1, 2 });
    EXPECT_TRUE(topology.set_chain_type(ChainType::TwoWay).empty());
    EXPECT_EQ(topology.parked_slots(), (std::vector<RigTopology::SlotId>{ 1, 2 }));
    topology.set_chain_type(ChainType::Series);
    EXPECT_EQ(topology.active_slots(), (std::vector<RigTopology::SlotId>{ 1, 2 }));
}

TEST(Unit_Dsp, RigTopologyChangingSplitTypeDiscardsBranchesAndKeepsPrePost)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({ StageKind::Pre }, 0, 10);
    topology.insert({ StageKind::Branch, 1 }, 0, 11);
    topology.insert({ StageKind::Post }, 0, 12);
    EXPECT_EQ(topology.set_chain_type(ChainType::ThreeWay),
              (std::vector<RigTopology::SlotId>{ 11 }));
    EXPECT_EQ(topology.stage({ StageKind::Pre }), (std::vector<RigTopology::SlotId>{ 10 }));
    EXPECT_EQ(topology.stage({ StageKind::Post }), (std::vector<RigTopology::SlotId>{ 12 }));
    EXPECT_TRUE(topology.stage({ StageKind::Branch, 2 }).empty());
}

TEST(Unit_Dsp, RigTopologySplitEdgesRouteThroughSplitterAndMixer)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({ StageKind::Pre }, 0, 1);
    topology.insert({ StageKind::Branch, 0 }, 0, 2);
    topology.insert({ StageKind::Post }, 0, 3);
    const std::vector<std::pair<RigTopology::SlotId, RigTopology::SlotId>> expected{
        { 1, 100 }, { 100, 2 }, { 2, 101 }, { 100, 101 }, { 101, 3 } };
    EXPECT_EQ(topology.edges(100, 101), expected);
}

TEST(Unit_Dsp, RigTopologyTwoWayAltIsADistinctTypeWithTwoBranches)
{
    EXPECT_EQ(RigTopology::branch_count(ChainType::TwoWayAlt), 2u);

    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWayAlt);
    EXPECT_EQ(topology.chain_type(), ChainType::TwoWayAlt);
    EXPECT_TRUE(topology.is_visible({ StageKind::Branch, 0 }));
    EXPECT_TRUE(topology.is_visible({ StageKind::Branch, 1 }));
    EXPECT_FALSE(topology.is_visible({ StageKind::Branch, 2 }));

    const std::vector<RigTopology::SlotId> discarded = topology.set_chain_type(ChainType::TwoWay);
    EXPECT_TRUE(discarded.empty());
}

TEST(Unit_Dsp, RigTopologyInsertIntoAnInvisibleStageIsRefused)
{
    RigTopology topology;
    EXPECT_FALSE(topology.is_visible({ StageKind::Pre }));
    EXPECT_FALSE(topology.insert({ StageKind::Pre }, 0, 1));

    topology.set_chain_type(ChainType::TwoWay);
    EXPECT_FALSE(topology.is_visible({ StageKind::Series }));
    EXPECT_FALSE(topology.insert({ StageKind::Series }, 0, 1));
    EXPECT_FALSE(topology.insert({ StageKind::Branch, 2 }, 0, 1));
}

TEST(Unit_Dsp, RigTopologyMoveWithinAStageAdjustsTheIndexAfterRemoval)
{
    RigTopology topology = RigTopology::from_series({ 1, 2, 3 });
    EXPECT_TRUE(topology.move(1, { StageKind::Series }, 2));
    EXPECT_EQ(topology.stage({ StageKind::Series }), (std::vector<RigTopology::SlotId>{ 2, 3, 1 }));
}

TEST(Unit_Dsp, RigTopologyMoveAcrossBranchesKeepsTheSlot)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({ StageKind::Branch, 0 }, 0, 5);
    EXPECT_TRUE(topology.move(5, { StageKind::Branch, 1 }, 0));
    EXPECT_TRUE(topology.stage({ StageKind::Branch, 0 }).empty());
    EXPECT_EQ(topology.stage({ StageKind::Branch, 1 }), (std::vector<RigTopology::SlotId>{ 5 }));
    const std::optional<StageAddress> address = topology.find(5);
    ASSERT_TRUE(address.has_value());
    EXPECT_EQ(address->kind, StageKind::Branch);
    EXPECT_EQ(address->branch, 1u);
}

TEST(Unit_Dsp, RigTopologyRemoveFindsTheSlotInAnyVisibleStage)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({ StageKind::Pre }, 0, 1);
    topology.insert({ StageKind::Branch, 1 }, 0, 2);
    topology.insert({ StageKind::Post }, 0, 3);

    EXPECT_TRUE(topology.remove(1));
    EXPECT_TRUE(topology.stage({ StageKind::Pre }).empty());
    EXPECT_TRUE(topology.remove(2));
    EXPECT_TRUE(topology.stage({ StageKind::Branch, 1 }).empty());
    EXPECT_TRUE(topology.remove(3));
    EXPECT_TRUE(topology.stage({ StageKind::Post }).empty());
    EXPECT_FALSE(topology.remove(99));
}

TEST(Unit_Dsp, RigTopologyTwoEmptyBranchesYieldOneSplitterToMixerEdge)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    const auto edges = topology.edges(100, 101);
    ASSERT_EQ(edges.size(), 1u);
    EXPECT_EQ(edges[0], std::make_pair(RigTopology::SlotId{ 100 }, RigTopology::SlotId{ 101 }));
}

TEST(Unit_Dsp, RigTopologyReplaceRenamesASlotInPlace)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({ StageKind::Branch, 0 }, 0, 5);
    EXPECT_TRUE(topology.replace(5, 55));
    EXPECT_EQ(topology.stage({ StageKind::Branch, 0 }), (std::vector<RigTopology::SlotId>{ 55 }));
    EXPECT_EQ(topology.find(5), std::nullopt);
    EXPECT_FALSE(topology.replace(5, 66));
}
