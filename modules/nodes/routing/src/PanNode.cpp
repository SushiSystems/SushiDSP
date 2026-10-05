/****************************************************************/
/* PanNode.cpp                                                  */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file PanNode.cpp
 * @brief Implements the constant-power pan node.
 * @author Mustafa Garip
 */

// M_PI requires _USE_MATH_DEFINES before any <cmath>.
#define _USE_MATH_DEFINES

#include "SushiDSP/routing/PanNode.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

namespace SushiDSP
{
    namespace
    {
        constexpr float kSmoothingMs = 20.0f;
        constexpr float kCoefficientEpsilon = 1e-4f;
    }

    PanNode::PanNode() {}

    void PanNode::prepare(const PrepareInfo& info)
    {
        // After prepare(), the smoother holds its target instead of gliding to it.
        pan_.prepare(info.sample_rate, kSmoothingMs);
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
        if (pan_.is_settled(kCoefficientEpsilon))
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

    PortLayout PanNode::ports() const { return { 1, 2 }; }

    void PanNode::reset() {}

    const ParameterTable<PanNode, 1>& PanNode::parameters()
    {
        static const ParameterTable<PanNode, 1> table{ {{
            { { "pan", "Pan", -1.0f, 1.0f, 0.0f, ParamKind::Linear, "" },
              [](PanNode& n, float v) { n.pan_.set_target(std::max(-1.0f, std::min(1.0f, v))); },
              [](const PanNode& n) { return n.pan_.target(); } },
        }} };
        return table;
    }

    float PanNode::get_parameter(const char* id) const { return parameters().read(*this, id); }

    void PanNode::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
    }

    void PanNode::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    const char* PanNode::type_id() const { return "sushidsp.pan"; }

    namespace
    {
        std::unique_ptr<IAudioNode> make_pan(const PrepareInfo&)
        {
            return std::make_unique<PanNode>();
        }
    }

    const NodeDescriptor& pan_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.pan",
            "Pan",
            PanNode::parameters().descriptors(),
            PanNode::parameters().count(),
            PortLayout{ 1, 2 },
            &make_pan,
        };
        return descriptor;
    }
}
