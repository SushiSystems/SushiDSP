/**************************************************************************/
/* test_in_memory_minimal_node.cpp                                        */
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
 * @file test_in_memory_minimal_node.cpp
 * @brief Tests that the minimal in-memory node keeps the promises of AudioNode and adds none.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/in_memory_minimal_node.hpp>

#include <vector>

TEST(Small_InMemoryMinimalNode, ProcessWritesSilenceToEveryChannelOfPortZero)
{
    SushiDSP::InMemoryMinimalNode node({0, 2});
    std::vector<float> left(8, 1.0f);
    std::vector<float> right(8, 1.0f);
    float* outputs[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{outputs, nullptr, 8, 0.0};

    node.process(context);

    EXPECT_EQ(left, std::vector<float>(8, 0.0f));
    EXPECT_EQ(right, std::vector<float>(8, 0.0f));
}

TEST(Small_InMemoryMinimalNode, PortsReportsTheDeclaredLayout)
{
    const SushiDSP::InMemoryMinimalNode node({1, 2});

    EXPECT_EQ(node.ports().input_channels, 1u);
    EXPECT_EQ(node.ports().output_channels, 2u);
}
