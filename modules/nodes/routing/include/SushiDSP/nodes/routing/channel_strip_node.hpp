/**************************************************************************/
/* channel_strip_node.hpp                                                 */
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
 * @file channel_strip_node.hpp
 * @brief Declares the mixer channel strip: level, pan, polarity and delay for one mono source.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/node_descriptor.hpp>
#include <SushiDSP/foundation/core/parameter_table.hpp>
#include <SushiDSP/foundation/math/smoothed_parameter.hpp>
#include <SushiDSP/nodes/routing/sample_delay_line.hpp>

namespace SushiDSP
{
    /// @brief One mono source's channel strip: polarity, delay, level and pan, out as stereo.
    class ChannelStripNode : public AudioNode
    {
        public:
            /// @brief The longest delay the strip's delay control accepts, in milliseconds.
            static constexpr float K_MAX_DELAY_MS = 50.0f;

            /// @brief Constructs the strip at unity level, centre pan, no delay and normal polarity.
            ChannelStripNode();

            /// @brief Sizes the delay line for K_MAX_DELAY_MS and derives the smoothers' coefficients.
            /// @param info The rate and block size the node will process at.
            void prepare(const PrepareInfo& info) override;

            /// @brief Applies polarity and delay to the input, then level and pan, into two channels.
            /// @param context The block's input/output buffers and frame count.
            void process(ProcessContext& context) override;

            /// @brief Returns this node's port layout.
            /// @return One input channel, two output channels.
            PortLayout ports() const override;

            /// @brief Clears the delay line. Control thread only.
            void reset() override;

            /// @brief Reads a parameter by string id.
            /// @param id One of "level", "pan", "polarity" or "delay".
            /// @return The parameter's current value, or 0.0f for an unknown id.
            float get_parameter(const char* id) const override;

            /// @brief Sets a parameter by string id.
            /// @param id One of "level", "pan", "polarity" or "delay".
            /// @param value The new value, clamped to the parameter's range.
            void set_parameter_for_control_thread(const char* id, float value) override;

            /// @brief Sets a parameter by descriptor index.
            /// @param index The parameter's index in the descriptor table.
            /// @param value The new value, clamped to the parameter's range.
            void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

            /// @brief This strip's registered type id.
            /// @return "sushidsp.channel_strip".
            const char* type_id() const override;

            /// @brief This node's parameters, one row each. Declared on the class so a row's lambdas
            /// can reach the private fields the knobs live in.
            /// @return The table, built once on first use.
            static const ParameterTable<ChannelStripNode, 4>& parameters();

        private:
            /// @brief Recomputes the pan gains from a pan value and records the value they match.
            /// @param pan The pan value, clamped to [-1, 1].
            void apply_pan(float pan);

            /// @brief Stores a delay in milliseconds and selects the matching sample count.
            /// @param milliseconds The delay, clamped to [0, K_MAX_DELAY_MS].
            void apply_delay(float milliseconds);

            /// @brief The smoothed linear level.
            SmoothedParameter level_{1.0f};
            /// @brief The smoothed pan control, -1 (left) to 1 (right).
            SmoothedParameter pan_{0.0f};
            /// @brief The level in decibels, as the control thread set it.
            float level_db_ = 0.0f;
            /// @brief The delay in milliseconds, as the control thread set it.
            float delay_ms_ = 0.0f;
            /// @brief True when the input is inverted.
            bool polarity_inverted_ = false;
            /// @brief The sample rate prepare() was given, used to convert milliseconds to samples.
            double sample_rate_ = 48000.0;
            /// @brief The whole-sample delay the input passes through.
            SampleDelayLine delay_;
            /// @brief The pan value the gains were last computed from.
            float applied_pan_ = -2.0f;
            /// @brief The current left-channel gain.
            float gain_left_ = 0.0f;
            /// @brief The current right-channel gain.
            float gain_right_ = 0.0f;
    };

    /// @brief The channel strip's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.channel_strip".
    const NodeDescriptor& channel_strip_descriptor();
} // namespace SushiDSP
