/**************************************************************************/
/* mixer_node.hpp                                                         */
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
 * @file mixer_node.hpp
 * @brief Declares the node that sums its inputs, each scaled by its own gain, into one output.
 * @author Mustafa Garip
 */

#pragma once

#include <array>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/node_descriptor.hpp>
#include <SushiDSP/foundation/core/parameter_table.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>

namespace SushiDSP
{
    /// @brief Sums up to K_MAX_FAN_IN inputs, each scaled by its own gain_N parameter (default
    /// 1.0), into a single output. Plain scalar multiply-add — no crossfade curve, no panning;
    /// a more elaborate mixing law is future work if a real rig needs one.
    class MixerNode : public AudioNode
    {
        public:
            /// @brief Matches RigGraph::K_MAX_FAN_IN_OUT: a MixerNode can never be connect()-ed more
            /// inputs than the graph itself allows, so every possible input slot has a declared gain.
            static constexpr std::size_t K_MAX_FAN_IN = RigGraph::K_MAX_FAN_IN_OUT;

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
            static const ParameterTable<MixerNode, K_MAX_FAN_IN>& parameters();

        private:
            std::array<float, K_MAX_FAN_IN> gains_;
    };

    /// @brief The mixer's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.mixer".
    const NodeDescriptor& mixer_descriptor();
} // namespace SushiDSP
