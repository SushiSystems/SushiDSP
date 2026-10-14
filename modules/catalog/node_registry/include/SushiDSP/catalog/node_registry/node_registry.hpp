/**************************************************************************/
/* node_registry.hpp                                                      */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file node_registry.hpp
 * @brief Declares the catalog of constructible nodes and the factory that builds one by id.
 * @author Mustafa Garip
 */

#pragma once

#include <memory>
#include <vector>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/node_descriptor.hpp>

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
            NodeCategory category;
    };

    /// @brief Every product applications/host_gui can offer in its gear panel. Adding a new product
    /// means adding one entry here — nothing else in the GUI app changes.
    /// @return The static catalog of registered products.
    const std::vector<NodeEntry>& node_registry();

    /// @brief Constructs a node by its NodeDescriptor::id.
    /// @param id The product id, e.g. "sushidsp.tubescreamer".
    /// @param info Sample rate, max block size, and channel count for the new node.
    /// @return The constructed node, or nullptr if id is not registered.
    std::unique_ptr<AudioNode> make_node(const char* id, const PrepareInfo& info);
} // namespace SushiDSP
