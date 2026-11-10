/**************************************************************************/
/* test_node_catalogue.cpp                                                */
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
 * @file test_node_catalogue.cpp
 * @brief Tests that the registry catalogue and the in-memory catalogue resolve type strings.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/session/rig_preset/node_catalogue.hpp>

namespace
{
    /// A type id the registry lists.
    constexpr const char* K_LISTED_TYPE = "sushidsp.tubescreamer";

    /// A type id no catalogue lists.
    constexpr const char* K_UNLISTED_TYPE = "sushidsp.nonexistent";
} // namespace

TEST(Small_NodeCatalogue, RegistryCatalogueFindsAListedType)
{
    const SushiDSP::RegistryNodeCatalogue catalogue;

    const SushiDSP::NodeDescriptor* descriptor = catalogue.find(K_LISTED_TYPE);

    ASSERT_NE(descriptor, nullptr);
    EXPECT_STREQ(descriptor->id, K_LISTED_TYPE);
}

TEST(Small_NodeCatalogue, RegistryCatalogueFindsNothingForAnUnlistedType)
{
    const SushiDSP::RegistryNodeCatalogue catalogue;

    EXPECT_EQ(catalogue.find(K_UNLISTED_TYPE), nullptr);
}

TEST(Small_NodeCatalogue, TheDefaultCatalogueIsTheRegistry)
{
    const SushiDSP::RegistryNodeCatalogue registry;

    EXPECT_EQ(SushiDSP::registry_node_catalogue().find(K_LISTED_TYPE), registry.find(K_LISTED_TYPE));
}

TEST(Small_NodeCatalogue, AnEmptyInMemoryCatalogueFindsNothing)
{
    const SushiDSP::InMemoryNodeCatalogue catalogue;

    EXPECT_EQ(catalogue.find(K_LISTED_TYPE), nullptr);
}

TEST(Small_NodeCatalogue, AnInMemoryCatalogueFindsTheDescriptorItWasGiven)
{
    const SushiDSP::NodeDescriptor* listed = SushiDSP::RegistryNodeCatalogue().find(K_LISTED_TYPE);
    ASSERT_NE(listed, nullptr);
    SushiDSP::InMemoryNodeCatalogue catalogue;

    catalogue.add(*listed);

    EXPECT_EQ(catalogue.find(K_LISTED_TYPE), listed);
}
