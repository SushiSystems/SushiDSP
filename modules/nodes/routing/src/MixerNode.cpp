/****************************************************************/
/* MixerNode.cpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "SushiDSP/routing/MixerNode.hpp"

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

    const ParameterTable<MixerNode, MixerNode::kMaxFanIn>& MixerNode::parameters()
    {
        static const ParameterTable<MixerNode, MixerNode::kMaxFanIn> table{ {{
            { { "gain_0", "Gain 1", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
              [](MixerNode& n, float v) { n.gains_[0] = v; },
              [](const MixerNode& n) { return n.gains_[0]; } },
            { { "gain_1", "Gain 2", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
              [](MixerNode& n, float v) { n.gains_[1] = v; },
              [](const MixerNode& n) { return n.gains_[1]; } },
            { { "gain_2", "Gain 3", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
              [](MixerNode& n, float v) { n.gains_[2] = v; },
              [](const MixerNode& n) { return n.gains_[2]; } },
            { { "gain_3", "Gain 4", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
              [](MixerNode& n, float v) { n.gains_[3] = v; },
              [](const MixerNode& n) { return n.gains_[3]; } },
            { { "gain_4", "Gain 5", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
              [](MixerNode& n, float v) { n.gains_[4] = v; },
              [](const MixerNode& n) { return n.gains_[4]; } },
            { { "gain_5", "Gain 6", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
              [](MixerNode& n, float v) { n.gains_[5] = v; },
              [](const MixerNode& n) { return n.gains_[5]; } },
            { { "gain_6", "Gain 7", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
              [](MixerNode& n, float v) { n.gains_[6] = v; },
              [](const MixerNode& n) { return n.gains_[6]; } },
            { { "gain_7", "Gain 8", 0.0f, 2.0f, 1.0f, ParamKind::Linear, "" },
              [](MixerNode& n, float v) { n.gains_[7] = v; },
              [](const MixerNode& n) { return n.gains_[7]; } },
        }} };
        return table;
    }

    float MixerNode::get_parameter(const char* id) const { return parameters().read(*this, id); }

    void MixerNode::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
    }

    void MixerNode::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    const char* MixerNode::type_id() const { return "sushidsp.mixer"; }

    namespace
    {
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
            MixerNode::parameters().descriptors(),
            MixerNode::parameters().count(),
            PortLayout{ static_cast<std::uint32_t>(MixerNode::kMaxFanIn), 1 },
            &make_mixer,
        };
        return descriptor;
    }
}
