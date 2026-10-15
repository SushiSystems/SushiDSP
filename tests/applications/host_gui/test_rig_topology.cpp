/**************************************************************************/
/* test_rig_topology.cpp                                                  */
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

/**
 * @file test_rig_topology.cpp
 * @brief Tests the paths and edges of RigTopology.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "rig_topology.hpp"

TEST(Small_RigTopology, RigTopologySinglePathEdgesJoinNeighboursInOrder)
{
    RigTopology topology = RigTopology::from_linear_order({4, 7, 9});
    const auto edges = topology.edges(std::nullopt, std::nullopt);
    ASSERT_EQ(edges.size(), 2u);
    EXPECT_EQ(edges[0], std::make_pair(RigTopology::SlotId{4}, RigTopology::SlotId{7}));
    EXPECT_EQ(edges[1], std::make_pair(RigTopology::SlotId{7}, RigTopology::SlotId{9}));
}

TEST(Small_RigTopology, RigTopologyMainPathSurvivesEverySwitch)
{
    RigTopology topology = RigTopology::from_linear_order({1, 2});
    const std::vector<RigTopology::SlotId> main_path{1, 2};

    for (ChainType type : {ChainType::TwoWay, ChainType::ThreeWay, ChainType::Single})
    {
        const ChainTypeChange change = topology.set_chain_type(type);
        EXPECT_TRUE(change.discarded.empty());
        EXPECT_EQ(topology.stage({StageKind::Branch, 0}), main_path);
    }
}

TEST(Small_RigTopology, RigTopologyOpeningAPathReportsItForTheCallerToFill)
{
    RigTopology topology = RigTopology::from_linear_order({1});

    const ChainTypeChange to_two = topology.set_chain_type(ChainType::TwoWay);
    EXPECT_EQ(to_two.opened, (std::vector<std::size_t>{1}));
    EXPECT_TRUE(to_two.discarded.empty());

    // The path the model opens starts empty. See tests/applications/host_gui/README.md.
    EXPECT_TRUE(topology.stage({StageKind::Branch, 1}).empty());

    const ChainTypeChange to_three = topology.set_chain_type(ChainType::ThreeWay);
    EXPECT_EQ(to_three.opened, (std::vector<std::size_t>{2}));
}

TEST(Small_RigTopology, RigTopologyClosingAPathDiscardsOnlyItsOwnSlots)
{
    RigTopology topology = RigTopology::from_linear_order({10});
    topology.set_chain_type(ChainType::ThreeWay);
    topology.insert({StageKind::Branch, 1}, 0, 11);
    topology.insert({StageKind::Branch, 2}, 0, 12);
    topology.insert({StageKind::Post}, 0, 13);

    const ChainTypeChange change = topology.set_chain_type(ChainType::TwoWay);
    EXPECT_EQ(change.discarded, (std::vector<RigTopology::SlotId>{12}));
    EXPECT_TRUE(change.opened.empty());
    EXPECT_EQ(topology.stage({StageKind::Branch, 0}), (std::vector<RigTopology::SlotId>{10}));
    EXPECT_EQ(topology.stage({StageKind::Branch, 1}), (std::vector<RigTopology::SlotId>{11}));
    EXPECT_EQ(topology.stage({StageKind::Post}), (std::vector<RigTopology::SlotId>{13}));
    EXPECT_TRUE(topology.is_visible({StageKind::Post}));
}

TEST(Small_RigTopology, RigTopologySplitEdgesRouteThroughSplitterAndMixer)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({StageKind::Branch, 0}, 0, 2);
    topology.insert({StageKind::Post}, 0, 3);
    const std::vector<std::pair<RigTopology::SlotId, RigTopology::SlotId>> expected{
        {100, 2}, {2, 101}, {100, 101}, {101, 3}};
    EXPECT_EQ(topology.edges(100, 101), expected);
}

TEST(Small_RigTopology, RigTopologySinglePathIsTheWholeChain)
{
    RigTopology topology = RigTopology::from_linear_order({1, 2});

    // One path carries no master bus, so nothing can be put there and nothing is routed there.
    EXPECT_FALSE(topology.is_visible({StageKind::Post}));
    EXPECT_FALSE(topology.insert({StageKind::Post}, 0, 3));
    EXPECT_EQ(topology.active_slots(), (std::vector<RigTopology::SlotId>{1, 2}));

    const std::vector<std::pair<RigTopology::SlotId, RigTopology::SlotId>> expected{{1, 2}};
    EXPECT_EQ(topology.edges(std::nullopt, std::nullopt), expected);
}

TEST(Small_RigTopology, RigTopologyInsertIntoAPathThatIsNotOpenIsRefused)
{
    RigTopology topology;
    EXPECT_TRUE(topology.is_visible({StageKind::Branch, 0}));
    EXPECT_FALSE(topology.is_visible({StageKind::Branch, 1}));
    EXPECT_FALSE(topology.insert({StageKind::Branch, 1}, 0, 1));

    topology.set_chain_type(ChainType::TwoWay);
    EXPECT_FALSE(topology.insert({StageKind::Branch, 2}, 0, 1));
}

TEST(Small_RigTopology, RigTopologyTheMasterBusOpensWithTheSecondPath)
{
    RigTopology topology;
    EXPECT_FALSE(topology.is_visible({StageKind::Post}));

    for (ChainType type : {ChainType::TwoWay, ChainType::ThreeWay})
    {
        topology.set_chain_type(type);
        EXPECT_TRUE(topology.is_visible({StageKind::Post}));
    }

    topology.set_chain_type(ChainType::Single);
    EXPECT_FALSE(topology.is_visible({StageKind::Post}));
}

TEST(Small_RigTopology, RigTopologyMoveWithinAStageAdjustsTheIndexAfterRemoval)
{
    RigTopology topology = RigTopology::from_linear_order({1, 2, 3});
    EXPECT_TRUE(topology.move(1, {StageKind::Branch, 0}, 2));
    EXPECT_EQ(topology.stage({StageKind::Branch, 0}), (std::vector<RigTopology::SlotId>{2, 3, 1}));
}

TEST(Small_RigTopology, RigTopologyMoveAcrossBranchesKeepsTheSlot)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({StageKind::Branch, 0}, 0, 5);
    EXPECT_TRUE(topology.move(5, {StageKind::Branch, 1}, 0));
    EXPECT_TRUE(topology.stage({StageKind::Branch, 0}).empty());
    EXPECT_EQ(topology.stage({StageKind::Branch, 1}), (std::vector<RigTopology::SlotId>{5}));
    const std::optional<StageAddress> address = topology.find(5);
    ASSERT_TRUE(address.has_value());
    EXPECT_EQ(address->kind, StageKind::Branch);
    EXPECT_EQ(address->branch, 1u);
}

TEST(Small_RigTopology, RigTopologyRemoveFindsTheSlotInAnyVisibleStage)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({StageKind::Branch, 0}, 0, 1);
    topology.insert({StageKind::Branch, 1}, 0, 2);
    topology.insert({StageKind::Post}, 0, 3);

    EXPECT_TRUE(topology.remove(1));
    EXPECT_TRUE(topology.stage({StageKind::Branch, 0}).empty());
    EXPECT_TRUE(topology.remove(2));
    EXPECT_TRUE(topology.stage({StageKind::Branch, 1}).empty());
    EXPECT_TRUE(topology.remove(3));
    EXPECT_TRUE(topology.stage({StageKind::Post}).empty());
    EXPECT_FALSE(topology.remove(99));
}

TEST(Small_RigTopology, RigTopologyTwoEmptyBranchesYieldOneSplitterToMixerEdge)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    const auto edges = topology.edges(100, 101);
    ASSERT_EQ(edges.size(), 1u);
    EXPECT_EQ(edges[0], std::make_pair(RigTopology::SlotId{100}, RigTopology::SlotId{101}));
}

TEST(Small_RigTopology, RigTopologyReplaceRenamesASlotInPlace)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({StageKind::Branch, 0}, 0, 5);
    EXPECT_TRUE(topology.replace(5, 55));
    EXPECT_EQ(topology.stage({StageKind::Branch, 0}), (std::vector<RigTopology::SlotId>{55}));
    EXPECT_EQ(topology.find(5), std::nullopt);
    EXPECT_FALSE(topology.replace(5, 66));
}

TEST(Small_RigTopology, ParallelChainJoinsEachPathThroughItsStripAndTheBus)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::Parallel);
    topology.insert(StageAddress{StageKind::Branch, 0}, 0, 10);
    topology.insert(StageAddress{StageKind::Branch, 1}, 0, 11);

    const auto edges = topology.parallel_edges(900, 901, 902);

    const std::vector<std::pair<RigTopology::SlotId, RigTopology::SlotId>> expected{
        {10, 900}, {900, 902}, {11, 901}, {901, 902}};
    EXPECT_EQ(edges, expected);
}

TEST(Small_RigTopology, ParallelChainHasTwoPaths)
{
    EXPECT_EQ(RigTopology::branch_count(ChainType::Parallel), 2u);
}
