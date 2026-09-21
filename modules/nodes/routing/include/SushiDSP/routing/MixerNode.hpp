/**************************************************************************/
/* MixerNode.hpp                                                          */
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

#pragma once

#include <array>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/core/ParameterTable.hpp"
#include "SushiDSP/core/RigGraph.hpp"

namespace SushiDSP
{
    /// @brief Sums up to kMaxFanIn inputs, each scaled by its own gain_N parameter (default
    /// 1.0), into a single output. Plain scalar multiply-add — no crossfade curve, no panning;
    /// a more elaborate mixing law is future work if a real rig needs one.
    class MixerNode : public IAudioNode
    {
    public:
        /// @brief Matches RigGraph::kMaxFanInOut: a MixerNode can never be connect()-ed more
        /// inputs than the graph itself allows, so every possible input slot has a declared gain.
        static constexpr std::size_t kMaxFanIn = RigGraph::kMaxFanInOut;

        MixerNode();

        void prepare(const PrepareInfo& info) override;
        void process(ProcessContext& context) override;
        PortLayout ports() const override;
        void reset() override;
        float get_parameter(const char* id) const override;
        void set_parameter_for_control_thread(const char* id, float value) override;
        void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

        /// @brief This mixer's registered type id.
        /// @return "sushidsp.mixer".
        const char* type_id() const override;

        /// @brief This node's parameters, one row each, feeding the descriptor and both
        /// dispatches. Declared on the class rather than at namespace scope so a row's
        /// lambdas can reach the private field a knob actually lives in.
        /// @return The table, built once on first use.
        static const ParameterTable<MixerNode, kMaxFanIn>& parameters();

    private:
        std::array<float, kMaxFanIn> gains_;
    };

    /// @brief The mixer's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.mixer".
    const NodeDescriptor& mixer_descriptor();
}
