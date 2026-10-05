/****************************************************************/
/* NodeRegistry.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
