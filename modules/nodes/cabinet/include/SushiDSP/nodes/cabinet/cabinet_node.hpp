/**************************************************************************/
/* cabinet_node.hpp                                                       */
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
 * @file cabinet_node.hpp
 * @brief Declares the standalone cabinet impulse-response convolver node.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <vector>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/cabinet_host.hpp>
#include <SushiDSP/foundation/core/node_descriptor.hpp>
#include <SushiDSP/foundation/core/rt_swap_slot.hpp>
#include <SushiDSP/foundation/math/streaming_convolution.hpp>

namespace SushiDSP
{
    /// @brief A standalone cabinet impulse-response convolver. Mono in, mono out; a
    /// passthrough until set_cabinet() installs an impulse response.
    class CabinetNode : public AudioNode, public CabinetHost
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
} // namespace SushiDSP
