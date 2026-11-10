/**************************************************************************/
/* test_audio_node.cpp                                                    */
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
 * @file test_audio_node.cpp
 * @brief Tests the default AudioNode gives a parameter written by index to a node that does not override it.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/in_memory_minimal_node.hpp>

TEST(Small_AudioNode, IgnoresAParameterWrittenByIndexByDefault)
{
    SushiDSP::InMemoryMinimalNode node({1, 2});
    node.set_parameter_for_audio_thread(0, 0.75f);
    node.set_parameter_for_audio_thread(65535, 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("anything"), 0.0f);
}
