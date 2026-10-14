/**************************************************************************/
/* dk_block_node.hpp                                                      */
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
 * @file dk_block_node.hpp
 * @brief Defines the adapter that wraps a one-in, one-out DkCircuit as an AudioNode.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>

namespace SushiDSP::test_circuits
{
    /// @brief Wraps one single-input, single-output DK block as a node.
    /// @tparam Netlist The DkNetlist instantiation describing the block.
    /// @tparam Circuit The matching DkCircuit instantiation.
    /// @tparam Port The block's nonlinear-port glue.
    template <typename Netlist, typename Circuit, typename Port>
    class DkBlockNode final : public AudioNode
    {
        public:
            /// @brief The block's description; fill it before prepare().
            Netlist netlist{};

            /// @brief Wiper positions applied after prepare()'s derivation.
            std::array<double, dk::K_MAXIMUM_POTENTIOMETERS> pot_positions{{0.5, 0.5, 0.5, 0.5}};

            /// @brief Volts per unit sample at this node's boundary; 1.0 means the buffers are volts.
            double input_scale_volts = 1.0;

            /// @brief Derives the block's matrices at the run rate and applies the wiper positions.
            /// @param info Sample rate, block size and channel count.
            void prepare(const PrepareInfo& info) override
            {
                prepared_ = circuit_.prepare(netlist, info.sample_rate);
                for (std::size_t index = 0; index < dk::K_MAXIMUM_POTENTIOMETERS; ++index)
                {
                    circuit_.set_potentiometer(index, pot_positions[index]);
                }
                circuit_.update_matrices_if_dirty();
                circuit_.reset_statistics();
            }

            /// @brief Solves one sample per frame, scaling into volts and back.
            /// @param context The per-block buffers and frame count.
            /// @pre prepare() has returned with prepared() true.
            void process(ProcessContext& context) override
            {
                const float* input = context.inputs == nullptr ? nullptr : context.inputs[0];
                float* output = context.outputs[0];
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    const double drive =
                        input == nullptr ? 0.0
                                         : static_cast<double>(input[frame]) * input_scale_volts;
                    std::array<double, 1> outputs{};
                    circuit_.process_sample(port_, {drive}, outputs);
                    output[frame] = static_cast<float>(outputs[0] / input_scale_volts);
                }
            }

            /// @brief Reports the adapter's fixed channel counts.
            /// @return One input channel and one output channel.
            PortLayout ports() const override
            {
                return PortLayout{1u, 1u};
            }

            /// @brief Clears the block's state.
            void reset() override
            {
                circuit_.reset();
            }

            /// @brief Reads a parameter; this fixture exposes none.
            /// @param id Ignored.
            /// @return Always zero.
            float get_parameter(const char* id) const override
            {
                static_cast<void>(id);
                return 0.0f;
            }

            /// @brief Writes a parameter; this fixture exposes none.
            /// @param id Ignored.
            /// @param value Ignored.
            void set_parameter_for_control_thread(const char* id, float value) override
            {
                static_cast<void>(id);
                static_cast<void>(value);
            }

            /// @brief Whether prepare() derived a usable circuit.
            /// @return True when the derivation succeeded.
            bool prepared() const
            {
                return prepared_;
            }

            /// @brief The wrapped circuit, for reading its convergence counters.
            /// @return The circuit.
            const Circuit& circuit() const
            {
                return circuit_;
            }

        private:
            Circuit circuit_{};
            Port port_{};
            bool prepared_{false};
    };
} // namespace SushiDSP::test_circuits
