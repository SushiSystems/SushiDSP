/**************************************************************************/
/* pan_node.cpp                                                           */
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
 * @file pan_node.cpp
 * @brief Implements the constant-power pan node.
 * @author Mustafa Garip
 */

// M_PI requires _USE_MATH_DEFINES before any <cmath>.
#define _USE_MATH_DEFINES

#include <SushiDSP/nodes/routing/pan_node.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

namespace SushiDSP
{
    namespace
    {
        constexpr float K_SMOOTHING_MS = 20.0f;
        constexpr float K_COEFFICIENT_EPSILON = 1e-4f;
    } // namespace

    PanNode::PanNode() {}

    void PanNode::prepare(const PrepareInfo& info)
    {
        // After prepare(), the smoother holds its target instead of gliding to it.
        pan_.prepare(info.sample_rate, K_SMOOTHING_MS);
        pan_.snap(pan_.target());
        applied_pan_ = -2.0f;
        apply_pan(pan_.current());
    }

    void PanNode::apply_pan(float pan)
    {
        const float clamped = std::max(-1.0f, std::min(1.0f, pan));
        const float theta = (clamped + 1.0f) * static_cast<float>(M_PI) / 4.0f;
        gain_left_ = std::cos(theta);
        gain_right_ = std::sin(theta);
        applied_pan_ = clamped;
    }

    void PanNode::process(ProcessContext& context)
    {
        float pan = pan_.skip(context.frames);
        if (pan_.is_settled(K_COEFFICIENT_EPSILON))
        {
            pan_.snap(pan_.target());
            pan = pan_.target();
        }
        // Once the smoother has snapped, pan equals applied_pan_ exactly and the trig pair is skipped.
        if (pan != applied_pan_)
        {
            apply_pan(pan);
        }

        const float* input = context.inputs != nullptr ? context.inputs[0] : nullptr;
        float* left = context.outputs[0];
        float* right = context.outputs[1];
        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            const float sample = input != nullptr ? input[i] : 0.0f;
            left[i] = sample * gain_left_;
            right[i] = sample * gain_right_;
        }
    }

    PortLayout PanNode::ports() const
    {
        return {1, 2};
    }

    void PanNode::reset() {}

    const ParameterTable<PanNode, 1>& PanNode::parameters()
    {
        static const ParameterTable<PanNode, 1> table{{{
            {{"pan", "Pan", -1.0f, 1.0f, 0.0f, ParamKind::Linear, ""},
             [](PanNode& n, float v)
             {
                 n.pan_.set_target(std::max(-1.0f, std::min(1.0f, v)));
             },
             [](const PanNode& n)
             {
                 return n.pan_.target();
             }},
        }}};
        return table;
    }

    float PanNode::get_parameter(const char* id) const
    {
        return parameters().read(*this, id);
    }

    void PanNode::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
    }

    void PanNode::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    const char* PanNode::type_id() const
    {
        return "sushidsp.pan";
    }

    namespace
    {
        std::unique_ptr<AudioNode> make_pan(const PrepareInfo&)
        {
            return std::make_unique<PanNode>();
        }
    } // namespace

    const NodeDescriptor& pan_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.pan",
            "Pan",
            PanNode::parameters().descriptors(),
            PanNode::parameters().count(),
            PortLayout{1, 2},
            &make_pan,
        };
        return descriptor;
    }
} // namespace SushiDSP
