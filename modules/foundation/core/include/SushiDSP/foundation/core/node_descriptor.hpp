/**************************************************************************/
/* node_descriptor.hpp                                                    */
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
 * @file node_descriptor.hpp
 * @brief Declares the descriptors a host instantiates and drives a product through.
 * @author Mustafa Garip
 */

#pragma once

#include <memory>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/port_layout.hpp>

namespace SushiDSP
{
    /// @brief How a ParameterDescriptor's value maps to a host automation control.
    enum class ParamKind
    {
        Linear,
        Logarithmic,
        Decibel,
        Boolean,
        Choice
    };

    /// @brief One automatable parameter exposed by a product's NodeDescriptor.
    struct ParameterDescriptor
    {
            const char* id;
            const char* label;
            float min;
            float max;
            float default_value;
            ParamKind kind;
            const char* unit;
    };

    /// @brief Everything a host needs to instantiate and drive a product, with no knowledge of its internals.
    struct NodeDescriptor
    {
            const char* id;
            const char* name;
            const ParameterDescriptor* parameters;
            std::size_t parameter_count;
            PortLayout ports;
            std::unique_ptr<AudioNode> (*make)(const PrepareInfo&);
    };
} // namespace SushiDSP
