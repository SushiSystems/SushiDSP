/**************************************************************************/
/* bus_node.cpp                                                           */
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
 * @file bus_node.cpp
 * @brief Implements the mixer bus that gates and sums its stereo inputs.
 * @author Mustafa Garip
 */

#include "SushiDSP/routing/bus_node.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>

namespace SushiDSP
{
    BusNode::BusNode()
    {
        gate_.fill(1.0f);
    }

    void BusNode::prepare(const PrepareInfo& info)
    {
        const double samples = static_cast<double>(K_GATE_SMOOTHING_MS) * info.sample_rate / 1000.0;
        gate_coefficient_ = static_cast<float>(1.0 - std::exp(-1.0 / std::max(samples, 1.0)));
    }

    void BusNode::process(ProcessContext& context)
    {
        float* left = context.outputs[0];
        float* right = context.outputs[1];
        std::fill(left, left + context.frames, 0.0f);
        std::fill(right, right + context.frames, 0.0f);

        if (context.inputs == nullptr)
        {
            return;
        }

        // Solo counts only connected inputs: a soloed slot with nothing wired must not silence the rest.
        bool any_solo = false;
        for (std::uint32_t slot = 0; slot < K_MAX_INPUTS; ++slot)
        {
            any_solo = any_solo || (solo_[slot] && context.inputs[2 * slot] != nullptr);
        }

        for (std::uint32_t slot = 0; slot < K_MAX_INPUTS; ++slot)
        {
            const float* in_left = context.inputs[2 * slot];
            const float* in_right = context.inputs[2 * slot + 1];
            const bool audible = in_left != nullptr && !mute_[slot] && (!any_solo || solo_[slot]);
            const float target = audible ? 1.0f : 0.0f;
            float gate = gate_[slot];

            if (gate == 0.0f && target == 0.0f)
            {
                continue;
            }

            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                gate += (target - gate) * gate_coefficient_;
                if (in_left != nullptr)
                {
                    left[frame] += in_left[frame] * gate;
                    right[frame] += in_right[frame] * gate;
                }
            }
            gate_[slot] = gate;
        }
    }

    PortLayout BusNode::ports() const
    {
        return {2 * K_MAX_INPUTS, 2};
    }

    void BusNode::reset()
    {
        gate_.fill(1.0f);
    }

    const ParameterTable<BusNode, 2 * BusNode::K_MAX_INPUTS>& BusNode::parameters()
    {
        static const ParameterTable<BusNode, 2 * K_MAX_INPUTS> table{{{
            {{"mute_0", "Mute 1", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.mute_[0] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.mute_[0] ? 1.0f : 0.0f;
             }},
            {{"mute_1", "Mute 2", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.mute_[1] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.mute_[1] ? 1.0f : 0.0f;
             }},
            {{"mute_2", "Mute 3", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.mute_[2] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.mute_[2] ? 1.0f : 0.0f;
             }},
            {{"mute_3", "Mute 4", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.mute_[3] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.mute_[3] ? 1.0f : 0.0f;
             }},
            {{"mute_4", "Mute 5", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.mute_[4] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.mute_[4] ? 1.0f : 0.0f;
             }},
            {{"mute_5", "Mute 6", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.mute_[5] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.mute_[5] ? 1.0f : 0.0f;
             }},
            {{"mute_6", "Mute 7", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.mute_[6] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.mute_[6] ? 1.0f : 0.0f;
             }},
            {{"mute_7", "Mute 8", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.mute_[7] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.mute_[7] ? 1.0f : 0.0f;
             }},
            {{"solo_0", "Solo 1", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.solo_[0] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.solo_[0] ? 1.0f : 0.0f;
             }},
            {{"solo_1", "Solo 2", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.solo_[1] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.solo_[1] ? 1.0f : 0.0f;
             }},
            {{"solo_2", "Solo 3", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.solo_[2] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.solo_[2] ? 1.0f : 0.0f;
             }},
            {{"solo_3", "Solo 4", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.solo_[3] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.solo_[3] ? 1.0f : 0.0f;
             }},
            {{"solo_4", "Solo 5", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.solo_[4] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.solo_[4] ? 1.0f : 0.0f;
             }},
            {{"solo_5", "Solo 6", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.solo_[5] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.solo_[5] ? 1.0f : 0.0f;
             }},
            {{"solo_6", "Solo 7", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.solo_[6] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.solo_[6] ? 1.0f : 0.0f;
             }},
            {{"solo_7", "Solo 8", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](BusNode& n, float v)
             {
                 n.solo_[7] = v >= 0.5f;
             },
             [](const BusNode& n)
             {
                 return n.solo_[7] ? 1.0f : 0.0f;
             }},
        }}};
        return table;
    }

    float BusNode::get_parameter(const char* id) const
    {
        return parameters().read(*this, id);
    }

    void BusNode::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
    }

    void BusNode::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    const char* BusNode::type_id() const
    {
        return "sushidsp.bus";
    }

    namespace
    {
        std::unique_ptr<AudioNode> make_bus(const PrepareInfo&)
        {
            return std::make_unique<BusNode>();
        }
    } // namespace

    const NodeDescriptor& bus_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.bus",
            "Bus",
            BusNode::parameters().descriptors(),
            BusNode::parameters().count(),
            PortLayout{2 * BusNode::K_MAX_INPUTS, 2},
            &make_bus,
        };
        return descriptor;
    }
} // namespace SushiDSP
