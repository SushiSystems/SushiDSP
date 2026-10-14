/**************************************************************************/
/* bus_node.hpp                                                           */
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
 * @file bus_node.hpp
 * @brief Declares the mixer bus: mute and solo for each of its stereo inputs, summed to stereo.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <cstdint>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/node_descriptor.hpp>
#include <SushiDSP/foundation/core/parameter_table.hpp>

namespace SushiDSP
{
    /// @brief Sums up to eight stereo inputs, each gated by its own mute and solo.
    ///
    /// Solo is held here because only the bus sees every input. A muted input is silent whatever
    /// its solo; while any connected input is soloed, only soloed inputs sound.
    class BusNode : public AudioNode
    {
        public:
            /// @brief The number of stereo inputs, which is the number of input slots RigGraph routes.
            static constexpr std::uint32_t K_MAX_INPUTS = 8;

            /// @brief The time over which an input's gate moves between silent and sounding, in milliseconds.
            static constexpr float K_GATE_SMOOTHING_MS = 5.0f;

            /// @brief Constructs the bus with every input unmuted and unsoloed.
            BusNode();

            /// @brief Derives the gate smoothing coefficient from the sample rate.
            /// @param info The rate and block size the node will process at.
            void prepare(const PrepareInfo& info) override;

            /// @brief Sums the connected inputs whose gates are open into the stereo output.
            /// @param context The block's input/output buffers and frame count.
            void process(ProcessContext& context) override;

            /// @brief Returns this node's port layout.
            /// @return Two channels per input slot, for K_MAX_INPUTS slots; two output channels.
            PortLayout ports() const override;

            /// @brief Opens every gate again; the next block starts each gate at its target.
            /// Control thread only.
            void reset() override;

            /// @brief Reads a parameter by string id.
            /// @param id One of "mute_0".."mute_7" or "solo_0".."solo_7".
            /// @return The parameter's value, or 0.0f for an unknown id.
            float get_parameter(const char* id) const override;

            /// @brief Sets a parameter by string id.
            /// @param id One of "mute_0".."mute_7" or "solo_0".."solo_7".
            /// @param value Values of 0.5 or more set the flag; others clear it.
            void set_parameter_for_control_thread(const char* id, float value) override;

            /// @brief Sets a parameter by descriptor index.
            /// @param index The parameter's index in the descriptor table.
            /// @param value Values of 0.5 or more set the flag; others clear it.
            void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

            /// @brief This bus's registered type id.
            /// @return "sushidsp.bus".
            const char* type_id() const override;

            /// @brief This node's parameters, one row each. Declared on the class so a row's lambdas
            /// can reach the private flags.
            /// @return The table, built once on first use.
            static const ParameterTable<BusNode, 2 * K_MAX_INPUTS>& parameters();

        private:
            /// @brief Holds the mute flag of each input.
            std::array<bool, K_MAX_INPUTS> mute_{};

            /// @brief Holds the solo flag of each input.
            std::array<bool, K_MAX_INPUTS> solo_{};

            /// @brief Holds each input's current gate, from 0 (silent) to 1 (sounding).
            std::array<float, K_MAX_INPUTS> gate_{};

            /// @brief Holds the fraction of the remaining gate distance covered per sample.
            float gate_coefficient_ = 1.0f;

            /// @brief Records that a block has run since construction or the last reset, so the
            /// gates move from where they are and no longer start at their targets.
            bool started_ = false;
    };

    /// @brief The bus's product registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.bus".
    const NodeDescriptor& bus_descriptor();
} // namespace SushiDSP
