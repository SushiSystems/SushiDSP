/**************************************************************************/
/* SplitterNode.cpp                                                       */
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
    void SplitterNode::set_parameter(const char*, float) {}

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
