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

namespace
{
    class Small_RigTopologySwitch : public ::testing::TestWithParam<ChainType>
    {
    };

    class Small_RigTopologyMasterBus : public ::testing::TestWithParam<ChainType>
    {
    };

    /// @brief Holds a chain state, the branch an insert aims at and the reason it is refused.
    struct ClosedPathRow
    {
        ChainType type;
        std::size_t branch;
    };

    class Small_RigTopologyClosedPath : public ::testing::TestWithParam<ClosedPathRow>
    {
    };

    /// @brief Holds a stage and the slot a topology places in it.
    struct StageSlotRow
    {
        StageAddress address;
        RigTopology::SlotId slot;
    };

    class Small_RigTopologyRemove : public ::testing::TestWithParam<StageSlotRow>
    {
    };
} // namespace

TEST_P(Small_RigTopologySwitch, RigTopologyMainPathSurvivesEverySwitch)
{
    RigTopology topology = RigTopology::from_linear_order({1, 2});

    const ChainTypeChange change = topology.set_chain_type(GetParam());

    EXPECT_TRUE(change.discarded.empty());
    EXPECT_EQ(topology.stage({StageKind::Branch, 0}), (std::vector<RigTopology::SlotId>{1, 2}));
}

INSTANTIATE_TEST_SUITE_P(ChainTypes, Small_RigTopologySwitch,
                         ::testing::Values(ChainType::TwoWay, ChainType::ThreeWay, ChainType::Single));

TEST(Small_RigTopology, RigTopologyOpeningTheSecondPathReportsItAndLeavesItEmpty)
{
    RigTopology topology = RigTopology::from_linear_order({1});

    const ChainTypeChange change = topology.set_chain_type(ChainType::TwoWay);

    EXPECT_EQ(change.opened, (std::vector<std::size_t>{1}));
    EXPECT_TRUE(change.discarded.empty());
    // The path the model opens starts empty. See tests/applications/host_gui/README.md.
    EXPECT_TRUE(topology.stage({StageKind::Branch, 1}).empty());
}

TEST(Small_RigTopology, RigTopologyOpeningTheThirdPathReportsIt)
{
    RigTopology topology = RigTopology::from_linear_order({1});
    topology.set_chain_type(ChainType::TwoWay);

    const ChainTypeChange change = topology.set_chain_type(ChainType::ThreeWay);

    EXPECT_EQ(change.opened, (std::vector<std::size_t>{2}));
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

    const std::vector<std::pair<RigTopology::SlotId, RigTopology::SlotId>> expected{{1, 2}};
    EXPECT_FALSE(topology.is_visible({StageKind::Post}));
    EXPECT_EQ(topology.active_slots(), (std::vector<RigTopology::SlotId>{1, 2}));
    EXPECT_EQ(topology.edges(std::nullopt, std::nullopt), expected);
}

TEST(Small_RigTopology, RigTopologySinglePathRefusesAnInsertIntoThePostStage)
{
    RigTopology topology = RigTopology::from_linear_order({1, 2});

    // One path carries no master bus, so nothing can be put there.
    EXPECT_FALSE(topology.insert({StageKind::Post}, 0, 3));
}

TEST_P(Small_RigTopologyClosedPath, RigTopologyInsertIntoAPathThatIsNotOpenIsRefused)
{
    RigTopology topology;
    topology.set_chain_type(GetParam().type);

    EXPECT_FALSE(topology.insert({StageKind::Branch, GetParam().branch}, 0, 1));
}

INSTANTIATE_TEST_SUITE_P(States, Small_RigTopologyClosedPath,
                         ::testing::Values(ClosedPathRow{ChainType::Single, 1}, ClosedPathRow{ChainType::TwoWay, 2}));

TEST(Small_RigTopology, RigTopologyShowsOnlyThePathsItHasOpen)
{
    RigTopology topology;

    EXPECT_TRUE(topology.is_visible({StageKind::Branch, 0}));
    EXPECT_FALSE(topology.is_visible({StageKind::Branch, 1}));
}

TEST_P(Small_RigTopologyMasterBus, RigTopologyTheMasterBusOpensWithTheSecondPath)
{
    RigTopology topology;

    topology.set_chain_type(GetParam());

    EXPECT_EQ(topology.is_visible({StageKind::Post}), GetParam() != ChainType::Single);
}

INSTANTIATE_TEST_SUITE_P(ChainTypes, Small_RigTopologyMasterBus,
                         ::testing::Values(ChainType::Single, ChainType::TwoWay, ChainType::ThreeWay));

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

TEST_P(Small_RigTopologyRemove, RigTopologyRemoveFindsTheSlotInAnyVisibleStage)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({StageKind::Branch, 0}, 0, 1);
    topology.insert({StageKind::Branch, 1}, 0, 2);
    topology.insert({StageKind::Post}, 0, 3);

    EXPECT_TRUE(topology.remove(GetParam().slot));
    EXPECT_TRUE(topology.stage(GetParam().address).empty());
}

INSTANTIATE_TEST_SUITE_P(Stages, Small_RigTopologyRemove,
                         ::testing::Values(StageSlotRow{{StageKind::Branch, 0}, 1},
                                           StageSlotRow{{StageKind::Branch, 1}, 2},
                                           StageSlotRow{{StageKind::Post}, 3}));

TEST(Small_RigTopology, RigTopologyRemoveRefusesASlotItDoesNotHold)
{
    RigTopology topology = RigTopology::from_linear_order({1});

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
}

TEST(Small_RigTopology, RigTopologyReplaceRefusesASlotItNoLongerHolds)
{
    RigTopology topology;
    topology.set_chain_type(ChainType::TwoWay);
    topology.insert({StageKind::Branch, 0}, 0, 5);
    topology.replace(5, 55);

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
