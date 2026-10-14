/**************************************************************************/
/* splitter_node.cpp                                                      */
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
 * @file splitter_node.cpp
 * @brief Implements the pass-through node that names a split point in a rig.
 * @author Mustafa Garip
 */

#include <SushiDSP/nodes/routing/splitter_node.hpp>

#include <memory>

namespace SushiDSP
{
    void SplitterNode::prepare(const PrepareInfo&) {}

    void SplitterNode::process(ProcessContext& context)
    {
        const float* input = context.inputs != nullptr ? context.inputs[0] : nullptr;
        float* output = context.outputs[0];
        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            output[i] = input != nullptr ? input[i] : 0.0f;
        }
    }

    PortLayout SplitterNode::ports() const
    {
        return {1, 1};
    }

    void SplitterNode::reset() {}

    float SplitterNode::get_parameter(const char*) const
    {
        return 0.0f;
    }
    void SplitterNode::set_parameter_for_control_thread(const char*, float) {}

    const char* SplitterNode::type_id() const
    {
        return "sushidsp.splitter";
    }

    namespace
    {
        std::unique_ptr<AudioNode> make_splitter(const PrepareInfo&)
        {
            return std::make_unique<SplitterNode>();
        }
    } // namespace

    const NodeDescriptor& splitter_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.splitter", "Splitter", nullptr, 0, PortLayout{1, 1}, &make_splitter,
        };
        return descriptor;
    }
} // namespace SushiDSP
