/**************************************************************************/
/* PanNode.cpp                                                            */
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

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and <cmath> may otherwise be pulled in transitively before this point.
#define _USE_MATH_DEFINES

#include "SushiDSP/routing/PanNode.hpp"

#include <cmath>
#include <cstring>
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
        // prepare() is a discontinuity anyway, so the smoother snaps rather than ramps from a
        // stale value, and applied_pan_ is invalidated so the first block builds gains that
        // match the snapped target (the JCM800's idiom for its own knobs).
        pan_.prepare(info.sample_rate, kSmoothingMs);
        pan_.snap(pan_.target());
        applied_pan_ = -2.0f;
        apply_pan(pan_.current());
    }

    void PanNode::apply_pan(float pan)
    {
        const float theta = (pan + 1.0f) * static_cast<float>(M_PI) / 4.0f;
        gain_left_ = std::cos(theta);
        gain_right_ = std::sin(theta);
        applied_pan_ = pan;
    }

    void PanNode::process(ProcessContext& context)
    {
        const float pan = pan_.skip(context.frames);
        if (std::fabs(pan - applied_pan_) > kCoefficientEpsilon)
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

    float PanNode::get_parameter(const char* id) const
    {
        return std::strcmp(id, "pan") == 0 ? pan_.target() : 0.0f;
    }

    void PanNode::set_parameter(const char* id, float value)
    {
        if (std::strcmp(id, "pan") == 0)
        {
            pan_.set_target(value);
        }
    }

    void PanNode::set_parameter(std::uint16_t index, float value)
    {
        if (index == 0)
        {
            pan_.set_target(value);
        }
    }

    const char* PanNode::type_id() const { return "sushidsp.pan"; }

    namespace
    {
        constexpr ParameterDescriptor kPanParameters[] = {
            { "pan", "Pan", -1.0f, 1.0f, 0.0f, ParamKind::Linear, "" },
        };

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
            kPanParameters,
            1,
            PortLayout{ 1, 2 },
            &make_pan,
        };
        return descriptor;
    }
}
