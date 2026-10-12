/**************************************************************************/
/* pan_node.hpp                                                           */
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
 * @file pan_node.hpp
 * @brief Declares the node that turns one channel into two on a constant-power pan law.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/core/audio_node.hpp"
#include "SushiDSP/core/node_descriptor.hpp"
#include "SushiDSP/core/parameter_table.hpp"
#include "SushiDSP/math/smoothed_parameter.hpp"

namespace SushiDSP
{
    /// @brief Turns one channel into two on a constant-power pan law.
    class PanNode : public AudioNode
    {
        public:
            /// @brief Constructs the node with its pan smoother at centre.
            PanNode();

            /// @brief Derives the pan smoother's per-sample coefficient and snaps its gains.
            /// @param info The rate and block size the node will process at.
            void prepare(const PrepareInfo& info) override;

            /// @brief Splits the one input channel into two, weighted by the current pan gains.
            /// @param context The block's input/output buffers and frame count.
            void process(ProcessContext& context) override;

            /// @brief Returns this node's port layout.
            /// @return One input channel, two output channels.
            PortLayout ports() const override;

            /// @brief Present to satisfy AudioNode; this node holds no state reset() must clear.
            void reset() override;

            /// @brief Reads a parameter by string id.
            /// @param id The parameter's id ("pan").
            /// @return The pan target, or 0.0f for an unknown id.
            float get_parameter(const char* id) const override;

            /// @brief Sets a parameter by string id.
            /// @param id The parameter's id ("pan").
            /// @param value The new target, clamped to [-1, 1].
            void set_parameter_for_control_thread(const char* id, float value) override;

            /// @brief Sets a parameter by descriptor index.
            /// @param index The parameter's index (0 for "pan").
            /// @param value The new target, clamped to [-1, 1].
            void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

            /// @brief This pan node's registered type id.
            /// @return "sushidsp.pan".
            const char* type_id() const override;

            /// @brief This node's parameters, one row each, feeding the descriptor and both
            /// dispatches. Declared on the class rather than at namespace scope so a row's
            /// lambdas can reach the private field a knob actually lives in.
            /// @return The table, built once on first use.
            static const ParameterTable<PanNode, 1>& parameters();

        private:
            /// @brief Recomputes gain_left_/gain_right_ from a moved pan value and updates the
            /// applied marker.
            /// @param pan The pan value the gains should now match, clamped to [-1, 1].
            void apply_pan(float pan);

            /// @brief The smoothed pan control, -1 (left) to 1 (right).
            SmoothedParameter pan_{0.0f};
            /// @brief The pan value gain_left_/gain_right_ were last computed from.
            float applied_pan_ = -2.0f;
            /// @brief The current left-channel gain.
            float gain_left_ = 0.0f;
            /// @brief The current right-channel gain.
            float gain_right_ = 0.0f;
    };

    /// @brief The pan node's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.pan".
    const NodeDescriptor& pan_descriptor();
} // namespace SushiDSP
