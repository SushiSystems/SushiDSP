/**************************************************************************/
/* test_node_descriptor.cpp                                               */
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
 * @file test_node_descriptor.cpp
 * @brief Tests the plain rows a node descriptor carries for its parameters and ports.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/node_descriptor.hpp>

#include <cstdint>

TEST(Small_NodeDescriptor, ParameterKindsAreFiveDistinctValues)
{
    const std::uint8_t kinds[] = {
        static_cast<std::uint8_t>(SushiDSP::ParamKind::Linear),
        static_cast<std::uint8_t>(SushiDSP::ParamKind::Logarithmic),
        static_cast<std::uint8_t>(SushiDSP::ParamKind::Decibel),
        static_cast<std::uint8_t>(SushiDSP::ParamKind::Boolean),
        static_cast<std::uint8_t>(SushiDSP::ParamKind::Choice),
    };
    for (std::size_t i = 0; i < 5; ++i)
    {
        for (std::size_t j = i + 1; j < 5; ++j)
        {
            EXPECT_NE(kinds[i], kinds[j]);
        }
    }
}

TEST(Small_NodeDescriptor, ParameterRowKeepsItsRangeAndDefault)
{
    const SushiDSP::ParameterDescriptor gain{"gain", "Gain", 0.0f, 10.0f, 5.0f,
                                             SushiDSP::ParamKind::Linear, ""};
    EXPECT_STREQ(gain.id, "gain");
    EXPECT_LE(gain.min, gain.default_value);
    EXPECT_GE(gain.max, gain.default_value);
    EXPECT_EQ(gain.kind, SushiDSP::ParamKind::Linear);
}
