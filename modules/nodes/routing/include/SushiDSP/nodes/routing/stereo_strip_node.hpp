/**************************************************************************/
/* stereo_strip_node.hpp                                                  */
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
 * @file stereo_strip_node.hpp
 * @brief Declares the stereo strip: level, balance and width for one stereo source.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/node_descriptor.hpp>
#include <SushiDSP/foundation/core/parameter_table.hpp>
#include <SushiDSP/foundation/math/smoothed_parameter.hpp>

namespace SushiDSP
{
    /// @brief One stereo source's strip: level, balance and width. Stereo in, stereo out.
    ///
    /// Balance leaves the centre at unity on both sides and turns the far side down: at -1 the
    /// right channel is silent, at +1 the left. Width scales the side signal: 0 is mono, 1 is the
    /// input. All three are smoothed, so a move does not click.
    class StereoStripNode : public AudioNode
    {
        public:
            /// @brief Constructs the strip at 0 dB, centred, at full width.
            StereoStripNode();

            /// @brief Derives the smoothing coefficients from the sample rate.
            /// @param info The rate and block size the node will process at.
            void prepare(const PrepareInfo& info) override;

            /// @brief Applies width, balance and level to the stereo input.
            /// @param context The block's input/output buffers and frame count.
            void process(ProcessContext& context) override;

            /// @brief Returns two channels in and two channels out.
            /// @return The port layout.
            PortLayout ports() const override;

            /// @brief Snaps every smoother to its target.
            void reset() override;

            /// @brief Reads a parameter by string id.
            /// @param id One of "level", "balance" or "width".
            /// @return The value, or 0.0f for an unknown id.
            float get_parameter(const char* id) const override;

            /// @brief Sets a parameter by string id.
            /// @param id One of "level", "balance" or "width".
            /// @param value The value, clamped to the parameter's range.
            void set_parameter_for_control_thread(const char* id, float value) override;

            /// @brief Sets a parameter by descriptor index.
            /// @param index The parameter's index in the descriptor table.
            /// @param value The value, clamped to the parameter's range.
            void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

            /// @brief This strip's registered type id.
            /// @return "sushidsp.stereo_strip".
            const char* type_id() const override;

            /// @brief This node's parameters, one row each.
            /// @return The table, built once on first use.
            static const ParameterTable<StereoStripNode, 3>& parameters();

        private:
            /// @brief Smooths the level as a linear gain.
            SmoothedParameter level_{1.0f};

            /// @brief Smooths the balance, -1 left to +1 right.
            SmoothedParameter balance_{0.0f};

            /// @brief Smooths the width, 0 mono to 1 as the input.
            SmoothedParameter width_{1.0f};

            /// @brief Stores the level in decibels, what get_parameter reads back.
            float level_db_ = 0.0f;
    };

    /// @brief The stereo strip's registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.stereo_strip".
    const NodeDescriptor& stereo_strip_descriptor();
} // namespace SushiDSP
