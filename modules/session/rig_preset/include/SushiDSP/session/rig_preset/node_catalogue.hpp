/**************************************************************************/
/* node_catalogue.hpp                                                     */
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
 * @file node_catalogue.hpp
 * @brief Declares the catalogue of node descriptors a preset resolves its type strings against.
 * @author Mustafa Garip
 * @see modules/session/rig_preset/README.md
 */

#pragma once

#include <cstring>
#include <vector>

#include <SushiDSP/catalog/node_registry/node_registry.hpp>

namespace SushiDSP
{
    /// @brief Resolves a node's type string to the descriptor that declares its parameters.
    class NodeCatalogue
    {
        public:
            virtual ~NodeCatalogue() = default;

            /// @brief Returns the descriptor whose id is @p type, or null when none has it.
            [[nodiscard]] virtual const NodeDescriptor* find(const char* type) const noexcept = 0;
    };

    /// @brief The production catalogue: every descriptor node_registry() lists.
    class RegistryNodeCatalogue final : public NodeCatalogue
    {
        public:
            [[nodiscard]] const NodeDescriptor* find(const char* type) const noexcept override
            {
                for (const NodeEntry& entry : node_registry())
                {
                    if (std::strcmp(entry.descriptor->id, type) == 0)
                    {
                        return entry.descriptor;
                    }
                }
                return nullptr;
            }
    };

    /// @brief A catalogue that holds the descriptors a test adds and no others.
    class InMemoryNodeCatalogue final : public NodeCatalogue
    {
        public:
            /// @brief Lists @p descriptor; it outlives the catalogue.
            void add(const NodeDescriptor& descriptor)
            {
                descriptors_.push_back(&descriptor);
            }

            [[nodiscard]] const NodeDescriptor* find(const char* type) const noexcept override
            {
                for (const NodeDescriptor* descriptor : descriptors_)
                {
                    if (std::strcmp(descriptor->id, type) == 0)
                    {
                        return descriptor;
                    }
                }
                return nullptr;
            }

        private:
            std::vector<const NodeDescriptor*> descriptors_;
    };

    /// @brief Returns the one RegistryNodeCatalogue, which the preset functions use by default.
    [[nodiscard]] inline const NodeCatalogue& registry_node_catalogue()
    {
        static const RegistryNodeCatalogue catalogue;
        return catalogue;
    }
} // namespace SushiDSP
