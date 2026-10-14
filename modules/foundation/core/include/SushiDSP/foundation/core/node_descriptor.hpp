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
