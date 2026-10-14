/**************************************************************************/
/* channel_strip_node.cpp                                                 */
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
 * @file channel_strip_node.cpp
 * @brief Implements the mixer channel strip.
 * @author Mustafa Garip
 */

// M_PI requires _USE_MATH_DEFINES before any <cmath>.
#define _USE_MATH_DEFINES
#include <SushiDSP/nodes/routing/channel_strip_node.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>

namespace SushiDSP
{
    namespace
    {
        constexpr float K_SMOOTHING_MS = 20.0f;
        constexpr float K_COEFFICIENT_EPSILON = 1e-4f;
        constexpr float K_MIN_LEVEL_DB = -60.0f;
        constexpr float K_MAX_LEVEL_DB = 12.0f;
    } // namespace

    ChannelStripNode::ChannelStripNode() {}

    void ChannelStripNode::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        level_.prepare(info.sample_rate, K_SMOOTHING_MS);
        level_.snap(level_.target());
        pan_.prepare(info.sample_rate, K_SMOOTHING_MS);
        pan_.snap(pan_.target());

        delay_.prepare(static_cast<std::size_t>(
            std::lround(K_MAX_DELAY_MS * info.sample_rate / 1000.0)));
        apply_delay(delay_ms_);

        applied_pan_ = -2.0f;
        apply_pan(pan_.current());
    }

    void ChannelStripNode::apply_pan(float pan)
    {
        const float clamped = std::max(-1.0f, std::min(1.0f, pan));
        const float theta = (clamped + 1.0f) * static_cast<float>(M_PI) / 4.0f;
        gain_left_ = std::cos(theta);
        gain_right_ = std::sin(theta);
        applied_pan_ = clamped;
    }

    void ChannelStripNode::apply_delay(float milliseconds)
    {
        delay_ms_ = std::max(0.0f, std::min(K_MAX_DELAY_MS, milliseconds));
        const double samples = std::round(static_cast<double>(delay_ms_) * sample_rate_ / 1000.0);
        delay_.set_delay(static_cast<std::size_t>(samples));
    }

    void ChannelStripNode::process(ProcessContext& context)
    {
        float level = level_.skip(context.frames);
        if (level_.is_settled(K_COEFFICIENT_EPSILON))
        {
            level_.snap(level_.target());
            level = level_.target();
        }

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

        const float sign = polarity_inverted_ ? -1.0f : 1.0f;
        const float left_gain = level * gain_left_;
        const float right_gain = level * gain_right_;
        const float* input = context.inputs != nullptr ? context.inputs[0] : nullptr;
        float* left = context.outputs[0];
        float* right = context.outputs[1];

        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            const float sample = input != nullptr ? input[i] * sign : 0.0f;
            const float delayed = delay_.process(sample);
            left[i] = delayed * left_gain;
            right[i] = delayed * right_gain;
        }
    }

    PortLayout ChannelStripNode::ports() const
    {
        return {1, 2};
    }

    void ChannelStripNode::reset()
    {
        delay_.reset();
    }

    const ParameterTable<ChannelStripNode, 4>& ChannelStripNode::parameters()
    {
        static const ParameterTable<ChannelStripNode, 4> table{{{
            {{"level", "Level", K_MIN_LEVEL_DB, K_MAX_LEVEL_DB, 0.0f, ParamKind::Decibel, "dB"},
             [](ChannelStripNode& n, float v)
             {
                 n.level_db_ = std::max(K_MIN_LEVEL_DB, std::min(K_MAX_LEVEL_DB, v));
                 n.level_.set_target(std::pow(10.0f, n.level_db_ / 20.0f));
             },
             [](const ChannelStripNode& n)
             {
                 return n.level_db_;
             }},
            {{"pan", "Pan", -1.0f, 1.0f, 0.0f, ParamKind::Linear, ""},
             [](ChannelStripNode& n, float v)
             {
                 n.pan_.set_target(std::max(-1.0f, std::min(1.0f, v)));
             },
             [](const ChannelStripNode& n)
             {
                 return n.pan_.target();
             }},
            {{"polarity", "Polarity", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, ""},
             [](ChannelStripNode& n, float v)
             {
                 n.polarity_inverted_ = v >= 0.5f;
             },
             [](const ChannelStripNode& n)
             {
                 return n.polarity_inverted_ ? 1.0f : 0.0f;
             }},
            {{"delay", "Delay", 0.0f, K_MAX_DELAY_MS, 0.0f, ParamKind::Linear, "ms"},
             [](ChannelStripNode& n, float v)
             {
                 n.apply_delay(v);
             },
             [](const ChannelStripNode& n)
             {
                 return n.delay_ms_;
             }},
        }}};
        return table;
    }

    float ChannelStripNode::get_parameter(const char* id) const
    {
        return parameters().read(*this, id);
    }

    void ChannelStripNode::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
    }

    void ChannelStripNode::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    const char* ChannelStripNode::type_id() const
    {
        return "sushidsp.channel_strip";
    }

    namespace
    {
        std::unique_ptr<AudioNode> make_channel_strip(const PrepareInfo&)
        {
            return std::make_unique<ChannelStripNode>();
        }
    } // namespace

    const NodeDescriptor& channel_strip_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.channel_strip",
            "Channel Strip",
            ChannelStripNode::parameters().descriptors(),
            ChannelStripNode::parameters().count(),
            PortLayout{1, 2},
            &make_channel_strip,
        };
        return descriptor;
    }
} // namespace SushiDSP
