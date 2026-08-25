/**************************************************************************/
/* NodeRegistry.hpp                                                       */
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

#pragma once

#include <memory>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"

namespace SushiDSP
{
    /// @brief A gear-panel grouping label for a NodeRegistry entry (e.g. "Distortion", "Amp").
    struct NodeCategory
    {
        const char* name;
    };

    /// @brief One constructible product: its descriptor plus its gear-panel category.
    struct NodeEntry
    {
        const NodeDescriptor* descriptor;
        NodeCategory          category;
    };

    /// @brief Every product apps/host_gui can offer in its gear panel. Adding a new product
    /// means adding one entry here — nothing else in the GUI app changes.
    /// @return The static catalog of registered products.
    const std::vector<NodeEntry>& node_registry();

    /// @brief Constructs a node by its NodeDescriptor::id.
    /// @param id The product id, e.g. "sushidsp.tubescreamer".
    /// @param info Sample rate, max block size, and channel count for the new node.
    /// @return The constructed node, or nullptr if id is not registered.
    std::unique_ptr<IAudioNode> make_node(const char* id, const PrepareInfo& info);
}
