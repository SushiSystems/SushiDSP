/**************************************************************************/
/* MixerNode.cpp                                                          */
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

#include "SushiDSP/routing/MixerNode.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

namespace SushiDSP
{
    namespace
    {
        constexpr std::uint16_t kUnknownParameterIndex = 0xFFFF;

        std::uint16_t parameter_index_for(const char* id)
        {
            for (std::uint16_t i = 0; i < static_cast<std::uint16_t>(MixerNode::kMaxFanIn); ++i)
            {
                char expected[16];
                std::snprintf(expected, sizeof(expected), "gain_%u", static_cast<unsigned>(i));
                if (std::strcmp(id, expected) == 0)
                {
                    return i;
                }
            }
            return kUnknownParameterIndex;
        }
    }

    MixerNode::MixerNode()
    {
        gains_.fill(1.0f);
    }

    void MixerNode::prepare(const PrepareInfo&) {}

    void MixerNode::process(ProcessContext& context)
    {
        float* output = context.outputs[0];
        std::fill(output, output + context.frames, 0.0f);

        if (context.inputs == nullptr)
        {
            return;
        }

        for (std::size_t input_slot = 0; input_slot < kMaxFanIn; ++input_slot)
        {
            const float* input = context.inputs[input_slot];
            if (input == nullptr)
            {
                continue;
            }
            const float gain = gains_[input_slot];
            for (std::uint32_t i = 0; i < context.frames; ++i)
            {
                output[i] += input[i] * gain;
            }
        }
    }

    PortLayout MixerNode::ports() const { return { static_cast<std::uint32_t>(kMaxFanIn), 1 }; }

    void MixerNode::reset() {}

    float MixerNode::get_parameter(const char* id) const
    {
        const std::uint16_t index = parameter_index_for(id);
        return index == kUnknownParameterIndex ? 0.0f : gains_[index];
    }

    void MixerNode::set_parameter(const char* id, float value)
    {
        set_parameter(parameter_index_for(id), value);
    }

    void MixerNode::set_parameter(std::uint16_t index, float value)
    {
        if (index < kMaxFanIn)
        {
            gains_[index] = value;
        }
    }

    const char* MixerNode::type_id() const { return "sushidsp.mixer"; }

    namespace
    {
        constexpr ParameterDescriptor kMixerParameters[] = {
            { "gain_0", "Gain 1", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_1", "Gain 2", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_2", "Gain 3", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_3", "Gain 4", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_4", "Gain 5", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_5", "Gain 6", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_6", "Gain 7", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
            { "gain_7", "Gain 8", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
        };
        static_assert(sizeof(kMixerParameters) / sizeof(kMixerParameters[0]) == MixerNode::kMaxFanIn,
            "kMixerParameters must declare exactly MixerNode::kMaxFanIn entries");

        std::unique_ptr<IAudioNode> make_mixer(const PrepareInfo&)
        {
            return std::make_unique<MixerNode>();
        }
    }

    const NodeDescriptor& mixer_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.mixer",
            "Mixer",
            kMixerParameters,
            MixerNode::kMaxFanIn,
            PortLayout{ static_cast<std::uint32_t>(MixerNode::kMaxFanIn), 1 },
            &make_mixer,
        };
        return descriptor;
    }
}
