/****************************************************************/
/* SplitterNode.cpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file SplitterNode.cpp
 * @brief Implements the pass-through node that names a split point in a rig.
 * @author Mustafa Garip
 */

#include "SushiDSP/routing/SplitterNode.hpp"

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

    PortLayout SplitterNode::ports() const { return { 1, 1 }; }

    void SplitterNode::reset() {}

    float SplitterNode::get_parameter(const char*) const { return 0.0f; }
    void SplitterNode::set_parameter_for_control_thread(const char*, float) {}

    const char* SplitterNode::type_id() const { return "sushidsp.splitter"; }

    namespace
    {
        std::unique_ptr<IAudioNode> make_splitter(const PrepareInfo&)
        {
            return std::make_unique<SplitterNode>();
        }
    }

    const NodeDescriptor& splitter_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.splitter",
            "Splitter",
            nullptr,
            0,
            PortLayout{ 1, 1 },
            &make_splitter,
        };
        return descriptor;
    }
}
