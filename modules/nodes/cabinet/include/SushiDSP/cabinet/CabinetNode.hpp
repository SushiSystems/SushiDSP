/**************************************************************************/
/* CabinetNode.hpp                                                       */
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
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#pragma once

#include <cstdint>
#include <vector>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/ICabinetHost.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/core/RtSwapSlot.hpp"
#include "SushiDSP/math/StreamingConvolution.hpp"

namespace SushiDSP
{
    /// @brief A standalone cabinet impulse-response convolver. Mono in, mono out; a
    /// passthrough until set_cabinet() installs an impulse response.
    class CabinetNode : public IAudioNode, public ICabinetHost
    {
    public:
        void prepare(const PrepareInfo& info) override;
        void process(ProcessContext& context) override;
        PortLayout ports() const override;
        void reset() override;
        float get_parameter(const char* id) const override;
        void set_parameter_for_control_thread(const char* id, float value) override;
        std::uint32_t latency_samples() const override;

        /// @brief This node's registered type id.
        /// @return "sushidsp.cabinet".
        const char* type_id() const override;

        bool set_cabinet(Ir impulse_response) override;
        void reclaim_cabinets() override;
        bool has_cabinet() const override;

    private:
        double sample_rate_ = 48000.0;
        std::uint32_t cabinet_partition_ = 0;
        std::uint32_t max_block_ = 0;
        std::vector<float> cabinet_impulse_response_;
        RtSwapSlot<StreamingConvolution> cabinet_slot_;
    };

    /// @brief The cabinet's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.cabinet".
    const NodeDescriptor& cabinet_descriptor();
}
