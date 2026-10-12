/**************************************************************************/
/* mixer_node.cpp                                                         */
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
 * @file mixer_node.cpp
 * @brief Implements the node that sums its inputs, each scaled by its own gain.
 * @author Mustafa Garip
 */

#include "SushiDSP/routing/mixer_node.hpp"

#include <algorithm>
#include <memory>

namespace SushiDSP
{
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

        for (std::size_t input_slot = 0; input_slot < K_MAX_FAN_IN; ++input_slot)
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

    PortLayout MixerNode::ports() const
    {
        return {static_cast<std::uint32_t>(K_MAX_FAN_IN), 1};
    }

    void MixerNode::reset() {}

    const ParameterTable<MixerNode, MixerNode::K_MAX_FAN_IN>& MixerNode::parameters()
    {
        static const ParameterTable<MixerNode, MixerNode::K_MAX_FAN_IN> table{{{
            {{"gain_0", "Gain 1", 0.0f, 2.0f, 1.0f, ParamKind::Linear, ""},
             [](MixerNode& n, float v)
             {
                 n.gains_[0] = v;
             },
             [](const MixerNode& n)
             {
                 return n.gains_[0];
             }},
            {{"gain_1", "Gain 2", 0.0f, 2.0f, 1.0f, ParamKind::Linear, ""},
             [](MixerNode& n, float v)
             {
                 n.gains_[1] = v;
             },
             [](const MixerNode& n)
             {
                 return n.gains_[1];
             }},
            {{"gain_2", "Gain 3", 0.0f, 2.0f, 1.0f, ParamKind::Linear, ""},
             [](MixerNode& n, float v)
             {
                 n.gains_[2] = v;
             },
             [](const MixerNode& n)
             {
                 return n.gains_[2];
             }},
            {{"gain_3", "Gain 4", 0.0f, 2.0f, 1.0f, ParamKind::Linear, ""},
             [](MixerNode& n, float v)
             {
                 n.gains_[3] = v;
             },
             [](const MixerNode& n)
             {
                 return n.gains_[3];
             }},
            {{"gain_4", "Gain 5", 0.0f, 2.0f, 1.0f, ParamKind::Linear, ""},
             [](MixerNode& n, float v)
             {
                 n.gains_[4] = v;
             },
             [](const MixerNode& n)
             {
                 return n.gains_[4];
             }},
            {{"gain_5", "Gain 6", 0.0f, 2.0f, 1.0f, ParamKind::Linear, ""},
             [](MixerNode& n, float v)
             {
                 n.gains_[5] = v;
             },
             [](const MixerNode& n)
             {
                 return n.gains_[5];
             }},
            {{"gain_6", "Gain 7", 0.0f, 2.0f, 1.0f, ParamKind::Linear, ""},
             [](MixerNode& n, float v)
             {
                 n.gains_[6] = v;
             },
             [](const MixerNode& n)
             {
                 return n.gains_[6];
             }},
            {{"gain_7", "Gain 8", 0.0f, 2.0f, 1.0f, ParamKind::Linear, ""},
             [](MixerNode& n, float v)
             {
                 n.gains_[7] = v;
             },
             [](const MixerNode& n)
             {
                 return n.gains_[7];
             }},
        }}};
        return table;
    }

    float MixerNode::get_parameter(const char* id) const
    {
        return parameters().read(*this, id);
    }

    void MixerNode::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
    }

    void MixerNode::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    const char* MixerNode::type_id() const
    {
        return "sushidsp.mixer";
    }

    namespace
    {
        std::unique_ptr<AudioNode> make_mixer(const PrepareInfo&)
        {
            return std::make_unique<MixerNode>();
        }
    } // namespace

    const NodeDescriptor& mixer_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.mixer",
            "Mixer",
            MixerNode::parameters().descriptors(),
            MixerNode::parameters().count(),
            PortLayout{static_cast<std::uint32_t>(MixerNode::K_MAX_FAN_IN), 1},
            &make_mixer,
        };
        return descriptor;
    }
} // namespace SushiDSP
