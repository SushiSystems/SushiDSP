/**************************************************************************/
/* CabinetNode.cpp                                                        */
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
 * @file CabinetNode.cpp
 * @brief Implements the standalone cabinet impulse-response convolver node.
 * @author Mustafa Garip
 */

#include "SushiDSP/cabinet/CabinetNode.hpp"

#include <memory>

namespace SushiDSP
{
    void CabinetNode::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        max_block_ = info.max_block;
        cabinet_partition_ = StreamingConvolution::partition_size_for(sample_rate_);
        if (!cabinet_impulse_response_.empty())
        {
            cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
                cabinet_partition_, cabinet_impulse_response_.data(),
                static_cast<std::uint32_t>(cabinet_impulse_response_.size()), max_block_));
        }
    }

    void CabinetNode::process(ProcessContext& context)
    {
        const float* input = context.inputs != nullptr ? context.inputs[0] : nullptr;
        float* output = context.outputs[0];

        StreamingConvolution* const cabinet = cabinet_slot_.acquire_for_audio_thread();
        if (cabinet != nullptr && input != nullptr)
        {
            cabinet->process(input, output, context.frames);
            return;
        }
        for (std::uint32_t i = 0; i < context.frames; ++i)
        {
            output[i] = input != nullptr ? input[i] : 0.0f;
        }
    }

    PortLayout CabinetNode::ports() const { return { 1, 1 }; }

    void CabinetNode::reset()
    {
        if (StreamingConvolution* const cabinet = cabinet_slot_.active_for_publisher())
        {
            cabinet->reset();
        }
    }

    float CabinetNode::get_parameter(const char*) const { return 0.0f; }
    void CabinetNode::set_parameter_for_control_thread(const char*, float) {}

    std::uint32_t CabinetNode::latency_samples() const
    {
        return has_cabinet() ? cabinet_partition_ : 0;
    }

    const char* CabinetNode::type_id() const { return "sushidsp.cabinet"; }

    bool CabinetNode::set_cabinet(Ir impulse_response)
    {
        if (impulse_response.samples.empty() || cabinet_partition_ == 0
            || impulse_response.sample_rate != sample_rate_)
        {
            return false;
        }
        cabinet_impulse_response_ = std::move(impulse_response.samples);
        cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
            cabinet_partition_, cabinet_impulse_response_.data(),
            static_cast<std::uint32_t>(cabinet_impulse_response_.size()), max_block_));
        return true;
    }

    void CabinetNode::reclaim_cabinets()
    {
        cabinet_slot_.reclaim();
    }

    bool CabinetNode::has_cabinet() const
    {
        return cabinet_slot_.has_active();
    }

    namespace
    {
        std::unique_ptr<IAudioNode> make_cabinet(const PrepareInfo&)
        {
            return std::make_unique<CabinetNode>();
        }
    }

    const NodeDescriptor& cabinet_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.cabinet",
            "Cabinet",
            nullptr,
            0,
            PortLayout{ 1, 1 },
            &make_cabinet,
        };
        return descriptor;
    }
}
