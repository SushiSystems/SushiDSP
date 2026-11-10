/**************************************************************************/
/* test_jcm_v1a_netlist.cpp                                               */
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
 * @file test_jcm_v1a_netlist.cpp
 * @brief Tests that the JCM800 2203 V1a netlist builds in both shapes and names the schematic values.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>

namespace
{
    using SushiDSP::dk::DkNetlist;
    using SushiDSP::test_circuits::build_jcm_v1a;

    /// @brief Describes one shape of the block: with or without its RC tail.
    struct BlockShape
    {
            const char* name;
            bool with_rc_tail;
            std::size_t reactive_count;
            std::size_t node_count;
            std::size_t output_node;
    };

    const BlockShape kShapes[] = {
        {"SixStateBlock", false, 6, SushiDSP::test_circuits::kNodeSupply, SushiDSP::test_circuits::kNodeOut},
        {"SevenStateBlockWithItsRcTail", true, 7, SushiDSP::test_circuits::kNodeTail,
         SushiDSP::test_circuits::kNodeTail},
    };

    /// @brief Builds the shape's netlist and returns the index build_jcm_v1a gave its potentiometer.
    std::size_t build(DkNetlist<7, 2, 2, 1>& netlist, const BlockShape& shape)
    {
        return build_jcm_v1a(netlist, shape.with_rc_tail);
    }
} // namespace

class Small_JcmV1aNetlist : public ::testing::TestWithParam<BlockShape>
{
};

TEST_P(Small_JcmV1aNetlist, NamesThePreampPotentiometerAsTheFirstOne)
{
    DkNetlist<7, 2, 2, 1> netlist;
    EXPECT_EQ(build(netlist, GetParam()), 0u);
    EXPECT_EQ(netlist.potentiometer_count_, 1u);
}

TEST_P(Small_JcmV1aNetlist, WiresThePotentiometerBetweenThePotTopAndGroundWithItsWiperAtTheOutput)
{
    DkNetlist<7, 2, 2, 1> netlist;
    const std::size_t pot = build(netlist, GetParam());
    const SushiDSP::dk::PotentiometerEntry& entry = netlist.potentiometers_[pot];
    const SushiDSP::dk::Branch& upper = netlist.resistors_[entry.first_section_index];
    const SushiDSP::dk::Branch& lower = netlist.resistors_[entry.second_section_index];

    EXPECT_EQ(entry.total_ohms, SushiDSP::test_circuits::K_PREAMP_POT_OHMS);
    EXPECT_EQ(upper.positive_node, static_cast<std::size_t>(SushiDSP::test_circuits::kNodePotTop));
    EXPECT_EQ(upper.negative_node, static_cast<std::size_t>(SushiDSP::test_circuits::kNodeOut));
    EXPECT_EQ(lower.positive_node, static_cast<std::size_t>(SushiDSP::test_circuits::kNodeOut));
    EXPECT_EQ(lower.negative_node, 0u);
}

TEST_P(Small_JcmV1aNetlist, CountsTheReactiveElementsOfItsShape)
{
    DkNetlist<7, 2, 2, 1> netlist;
    build(netlist, GetParam());
    EXPECT_EQ(netlist.reactive_count_, GetParam().reactive_count);
}

TEST_P(Small_JcmV1aNetlist, NumbersTheNodesAndProbesTheOutputOfItsShape)
{
    DkNetlist<7, 2, 2, 1> netlist;
    build(netlist, GetParam());
    EXPECT_EQ(netlist.node_count, GetParam().node_count);
    EXPECT_EQ(netlist.outputs_[0].positive_node, GetParam().output_node);
}

INSTANTIATE_TEST_SUITE_P(Shapes, Small_JcmV1aNetlist, ::testing::ValuesIn(kShapes),
                         [](const ::testing::TestParamInfo<BlockShape>& info) { return std::string(info.param.name); });
