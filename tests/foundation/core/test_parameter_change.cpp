/**************************************************************************/
/* test_parameter_change.cpp                                              */
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
 * @file test_parameter_change.cpp
 * @brief Tests that a parameter change names the node, the parameter and the value.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/parameter_change.hpp>

#include <cstdint>

TEST(Small_ParameterChange, KeepsTheNodeParameterAndValueItWasMadeWith)
{
    const SushiDSP::ParameterChange change{7u, static_cast<std::uint16_t>(2), 0.25f};
    EXPECT_EQ(change.node, 7u);
    EXPECT_EQ(change.parameter_index, 2u);
    EXPECT_FLOAT_EQ(change.value, 0.25f);
}

TEST(Small_ParameterChange, NodeHandleIsThirtyTwoBits)
{
    EXPECT_EQ(sizeof(SushiDSP::NodeHandle), 4u);
}
