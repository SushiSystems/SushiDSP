/**************************************************************************/
/* stereo_strip_node.cpp                                                  */
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
 * @file stereo_strip_node.cpp
 * @brief Implements the stereo strip.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/routing/stereo_strip_node.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

namespace SushiDSP
{
    namespace
    {
        /// @brief The time constant of the three smoothers, in milliseconds.
        constexpr float K_SMOOTHING_MS = 20.0f;

        constexpr float K_MIN_LEVEL_DB = -60.0f;
        constexpr float K_MAX_LEVEL_DB = 12.0f;
    } // namespace

    StereoStripNode::StereoStripNode() {}

    void StereoStripNode::prepare(const PrepareInfo& info)
    {
        level_.prepare(info.sample_rate, K_SMOOTHING_MS);
        balance_.prepare(info.sample_rate, K_SMOOTHING_MS);
        width_.prepare(info.sample_rate, K_SMOOTHING_MS);
        reset();
    }

    void StereoStripNode::process(ProcessContext& context)
    {
        const float* in_left = context.inputs != nullptr ? context.inputs[0] : nullptr;
        const float* in_right = context.inputs != nullptr ? context.inputs[1] : nullptr;
        float* left = context.outputs[0];
        float* right = context.outputs[1];
        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            const float level = level_.next();
            const float balance = balance_.next();
            const float width = width_.next();
            const float l = in_left != nullptr ? in_left[i] : 0.0f;
            const float r = in_right != nullptr ? in_right[i] : 0.0f;
            const float mid = 0.5f * (l + r);
            const float side = 0.5f * (l - r) * width;
            left[i] = level * std::min(1.0f, 1.0f - balance) * (mid + side);
            right[i] = level * std::min(1.0f, 1.0f + balance) * (mid - side);
        }
    }

    PortLayout StereoStripNode::ports() const
    {
        return {2, 2};
    }

    void StereoStripNode::reset()
    {
        level_.snap(level_.target());
        balance_.snap(balance_.target());
        width_.snap(width_.target());
    }

    const ParameterTable<StereoStripNode, 3>& StereoStripNode::parameters()
    {
        static const ParameterTable<StereoStripNode, 3> table{{{
            {{"level", "Level", K_MIN_LEVEL_DB, K_MAX_LEVEL_DB, 0.0f, ParamKind::Decibel, "dB"},
             [](StereoStripNode& n, float v)
             {
                 n.level_db_ = std::clamp(v, K_MIN_LEVEL_DB, K_MAX_LEVEL_DB);
                 n.level_.set_target(std::pow(10.0f, n.level_db_ / 20.0f));
             },
             [](const StereoStripNode& n)
             {
                 return n.level_db_;
             }},
            {{"balance", "Balance", -1.0f, 1.0f, 0.0f, ParamKind::Linear, ""},
             [](StereoStripNode& n, float v)
             {
                 n.balance_.set_target(std::clamp(v, -1.0f, 1.0f));
             },
             [](const StereoStripNode& n)
             {
                 return n.balance_.target();
             }},
            {{"width", "Width", 0.0f, 1.0f, 1.0f, ParamKind::Linear, ""},
             [](StereoStripNode& n, float v)
             {
                 n.width_.set_target(std::clamp(v, 0.0f, 1.0f));
             },
             [](const StereoStripNode& n)
             {
                 return n.width_.target();
             }},
        }}};
        return table;
    }

    float StereoStripNode::get_parameter(const char* id) const
    {
        return parameters().read(*this, id);
    }

    void StereoStripNode::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
    }

    void StereoStripNode::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    const char* StereoStripNode::type_id() const
    {
        return "sushidsp.stereo_strip";
    }

    namespace
    {
        std::unique_ptr<AudioNode> make_stereo_strip(const PrepareInfo&)
        {
            return std::make_unique<StereoStripNode>();
        }
    } // namespace

    const NodeDescriptor& stereo_strip_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.stereo_strip",
            "Stereo Strip",
            StereoStripNode::parameters().descriptors(),
            StereoStripNode::parameters().count(),
            PortLayout{2, 2},
            &make_stereo_strip,
        };
        return descriptor;
    }
} // namespace SushiDSP
