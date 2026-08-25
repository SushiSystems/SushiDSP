/**************************************************************************/
/* test_node_registry.cpp                                                 */
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

#include <cstring>

#include <gtest/gtest.h>

#include "SushiDSP/node_registry/NodeRegistry.hpp"

TEST(Unit_Dsp, NodeRegistryListsTubeScreamerAndJcm800)
{
    const auto& entries = SushiDSP::node_registry();
    ASSERT_EQ(entries.size(), 2u);

    bool found_tube_screamer = false, found_jcm800 = false;
    for (const auto& entry : entries)
    {
        if (std::strcmp(entry.descriptor->id, "sushidsp.tubescreamer") == 0)
        {
            found_tube_screamer = true;
            EXPECT_STREQ(entry.category.name, "Distortion");
        }
        if (std::strcmp(entry.descriptor->id, "sushidsp.jcm800") == 0)
        {
            found_jcm800 = true;
            EXPECT_STREQ(entry.category.name, "Amp");
        }
    }
    EXPECT_TRUE(found_tube_screamer);
    EXPECT_TRUE(found_jcm800);
}

TEST(Unit_Dsp, MakeNodeConstructsTubeScreamerById)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    auto node = SushiDSP::make_node("sushidsp.tubescreamer", info);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->ports().input_channels, 1u);
    EXPECT_EQ(node->ports().output_channels, 1u);
    node->set_parameter("drive", 0.3f);
    EXPECT_FLOAT_EQ(node->get_parameter("drive"), 0.3f);
}

TEST(Unit_Dsp, MakeNodeConstructsJcm800ById)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    auto node = SushiDSP::make_node("sushidsp.jcm800", info);
    ASSERT_NE(node, nullptr);
    node->set_parameter("preamp", 0.4f);
    EXPECT_FLOAT_EQ(node->get_parameter("preamp"), 0.4f);
}

TEST(Unit_Dsp, MakeNodeReturnsNullForUnknownId)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    EXPECT_EQ(SushiDSP::make_node("does.not.exist", info), nullptr);
}
