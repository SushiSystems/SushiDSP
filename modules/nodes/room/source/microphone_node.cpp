/**************************************************************************/
/* microphone_node.cpp                                                    */
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
 * @file microphone_node.cpp
 * @brief Implements the microphone rig element.
 * @author Mustafa Garip
 */

#include "SushiDSP/room/microphone_node.hpp"

#include <cstdint>
#include <memory>

namespace SushiDSP::Room
{
    MicrophoneNode::MicrophoneNode() {}

    void MicrophoneNode::prepare(const PrepareInfo&) {}

    void MicrophoneNode::process(ProcessContext& context)
    {
        const float sign = polarity_inverted_ ? -1.0f : 1.0f;
        const float* input = context.inputs != nullptr ? context.inputs[0] : nullptr;
        float* output = context.outputs[0];
        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            output[i] = input != nullptr ? input[i] * sign : 0.0f;
        }
    }

    PortLayout MicrophoneNode::ports() const
    {
        return {1, 1};
    }

    void MicrophoneNode::reset() {}

    const ParameterTable<MicrophoneNode, 2>& MicrophoneNode::parameters()
    {
        static const ParameterTable<MicrophoneNode, 2> table{{{
            {{"polarity", "Polarity", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](MicrophoneNode& n, float v)
             {
                 n.polarity_inverted_ = v >= 0.5f;
             },
             [](const MicrophoneNode& n)
             {
                 return n.polarity_inverted_ ? 1.0f : 0.0f;
             }},
            {{"room", "Room Mic", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](MicrophoneNode& n, float v)
             {
                 n.room_ = v >= 0.5f;
             },
             [](const MicrophoneNode& n)
             {
                 return n.room_ ? 1.0f : 0.0f;
             }},
        }}};
        return table;
    }

    float MicrophoneNode::get_parameter(const char* id) const
    {
        return parameters().read(*this, id);
    }

    void MicrophoneNode::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
    }

    void MicrophoneNode::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    const char* MicrophoneNode::type_id() const
    {
        return "sushidsp.microphone";
    }

    bool MicrophoneNode::is_room_microphone() const noexcept
    {
        return room_;
    }

    namespace
    {
        std::unique_ptr<AudioNode> make_microphone(const PrepareInfo&)
        {
            return std::make_unique<MicrophoneNode>();
        }
    } // namespace

    const NodeDescriptor& microphone_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.microphone",
            "Microphone",
            MicrophoneNode::parameters().descriptors(),
            MicrophoneNode::parameters().count(),
            PortLayout{1, 1},
            &make_microphone,
        };
        return descriptor;
    }
} // namespace SushiDSP::Room
