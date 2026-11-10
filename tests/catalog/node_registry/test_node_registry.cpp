/**************************************************************************/
/* test_node_registry.cpp                                                 */
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
 * @file test_node_registry.cpp
 * @brief Tests that NodeRegistry lists and builds the registered nodes.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/room_host.hpp>
#include <SushiDSP/nodes/routing/mixer_node.hpp>
#include <SushiDSP/catalog/node_registry/node_registry.hpp>

namespace
{

const SushiDSP::PrepareInfo kPrepare{48000.0, 256, 1};

/** The category a product is listed under, found by its id. */
const SushiDSP::NodeEntry* find_entry(const char* id)
{
    for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
    {
        if (std::strcmp(entry.descriptor->id, id) == 0)
        {
            return &entry;
        }
    }
    return nullptr;
}

/** A product the registry lists, with the category it is listed under. */
struct ListedProduct
{
    const char* id;
    const char* category;
};

/** A product make_node builds by id, with the ports the built node declares. */
struct BuiltProduct
{
    const char* id;
    std::uint32_t input_channels;
    std::uint32_t output_channels;
};

const ListedProduct kListedProducts[] = {
    {"sushidsp.tubescreamer", "Distortion"},
    {"sushidsp.jcm800", "Amp"},
    {"sushidsp.peavey5150", "Amp"},
    {"sushidsp.mesa_mark2c", "Amp"},
    {"sushidsp.diezel_vh4", "Amp"},
    {"sushidsp.splitter", "Routing"},
    {"sushidsp.mixer", "Routing"},
    {"sushidsp.room", "Room"},
};

const BuiltProduct kBuiltProducts[] = {
    {"sushidsp.tubescreamer", 1u, 1u},
    {"sushidsp.jcm800", 1u, 1u},
    {"sushidsp.peavey5150", 1u, 1u},
    {"sushidsp.mesa_mark2c", 1u, 1u},
    {"sushidsp.diezel_vh4", 1u, 1u},
    {"sushidsp.splitter", 1u, 1u},
    {"sushidsp.mixer", static_cast<std::uint32_t>(SushiDSP::MixerNode::K_MAX_FAN_IN), 1u},
    {"sushidsp.cabinet", 1u, 1u},
    {"sushidsp.microphone", 1u, 1u},
    {"sushidsp.room", 6u, 2u},
};

std::string id_suffix(const char* id)
{
    std::string name(id);
    return name.substr(name.find('.') + 1);
}

} // namespace

class Small_NodeRegistryListing : public ::testing::TestWithParam<ListedProduct>
{
};

TEST_P(Small_NodeRegistryListing, ListsTheProductUnderItsCategory)
{
    const SushiDSP::NodeEntry* entry = find_entry(GetParam().id);
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->category.name, GetParam().category);
}

INSTANTIATE_TEST_SUITE_P(Products, Small_NodeRegistryListing, ::testing::ValuesIn(kListedProducts),
                         [](const ::testing::TestParamInfo<ListedProduct>& info) { return id_suffix(info.param.id); });

class Small_NodeRegistryMake : public ::testing::TestWithParam<BuiltProduct>
{
};

TEST_P(Small_NodeRegistryMake, BuildsTheProductRegisteredUnderItsId)
{
    const BuiltProduct& product = GetParam();
    std::unique_ptr<SushiDSP::AudioNode> node = SushiDSP::make_node(product.id, kPrepare);

    ASSERT_NE(node, nullptr);
    EXPECT_STREQ(node->type_id(), product.id);
    EXPECT_EQ(node->ports().input_channels, product.input_channels);
    EXPECT_EQ(node->ports().output_channels, product.output_channels);
}

INSTANTIATE_TEST_SUITE_P(Products, Small_NodeRegistryMake, ::testing::ValuesIn(kBuiltProducts),
                         [](const ::testing::TestParamInfo<BuiltProduct>& info) { return id_suffix(info.param.id); });

TEST(Small_NodeRegistry, MakeNodeReturnsNullForUnknownId)
{
    EXPECT_EQ(SushiDSP::make_node("does.not.exist", kPrepare), nullptr);
}

TEST(Small_NodeRegistry, RoomBuiltByIdIsARoomHost)
{
    std::unique_ptr<SushiDSP::AudioNode> node = SushiDSP::make_node("sushidsp.room", kPrepare);

    ASSERT_NE(node, nullptr);
    EXPECT_NE(dynamic_cast<SushiDSP::room::RoomHost*>(node.get()), nullptr);
}

TEST(Small_NodeRegistry, RoomBuiltByIdReadsBackTheDirectDelayItWasSet)
{
    std::unique_ptr<SushiDSP::AudioNode> node = SushiDSP::make_node("sushidsp.room", kPrepare);
    ASSERT_NE(node, nullptr);

    node->set_parameter_for_control_thread("direct_delay", 1.0f);

    EXPECT_FLOAT_EQ(node->get_parameter("direct_delay"), 1.0f);
}
