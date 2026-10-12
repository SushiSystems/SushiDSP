/**************************************************************************/
/* audio_node.hpp                                                         */
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
 * @file audio_node.hpp
 * @brief Declares the DSP graph node interface and its prepare and process contexts.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>

#include "SushiDSP/core/port_layout.hpp"

namespace SushiDSP
{
    /// @brief Parameters supplied to AudioNode::prepare(), fixed for the node's lifetime.
    struct PrepareInfo
    {
            double sample_rate;
            std::uint32_t max_block;
            std::uint32_t channels;
    };

    /// @brief Per-block processing context. process() must fill exactly [0, frames) of each output channel.
    ///
    /// A node with one output port writes `outputs` and ignores `port_outputs`. A node that declares
    /// more output ports writes port p through `port_outputs[p]`. RigGraph always sets `port_outputs`
    /// and makes `port_outputs[0]` equal `outputs`; a caller that invokes process() directly leaves it
    /// null, and such a caller may then drive only port 0.
    struct ProcessContext
    {
            float* const* outputs;
            const float* const* inputs;
            std::uint32_t frames;
            double time_seconds;
            /// @brief Holds one channel-pointer array per output port; null outside RigGraph.
            float* const* const* port_outputs = nullptr;
    };

    /// @brief One DSP graph node. Implementations must be real-time safe in process().
    struct AudioNode
    {
            virtual ~AudioNode() = default;

            /// @brief Acquires all memory the node will need. Never called on the processing thread.
            /// @param info Sample rate, max block size, and channel count for this node's lifetime.
            virtual void prepare(const PrepareInfo& info) = 0;

            /// @brief Fills context.outputs with exactly context.frames of audio.
            /// @param context The per-block input/output buffers and frame count.
            /// No allocation, no lock, no syscall, no exception.
            virtual void process(ProcessContext& context) = 0;

            /// @brief Returns this node's fixed input/output channel counts.
            /// @return The node's PortLayout. Its output_channels is the width of output port 0.
            virtual PortLayout ports() const = 0;

            /// @brief Returns how many output ports this node declares. Port 0 always exists.
            /// @return 1 unless the node overrides it; RigGraph refuses a count above its K_MAX_OUTPUT_PORTS.
            virtual std::uint32_t output_port_count() const
            {
                return 1;
            }

            /// @brief Returns the channel count of one output port, the width RigGraph routes from it.
            /// @param port The port index, below output_port_count().
            /// @return The channel count; port 0 defaults to ports().output_channels, and any other
            /// port defaults to 0, which RigGraph refuses. A node with more ports overrides this too.
            virtual std::uint32_t output_port_channels(std::uint32_t port) const
            {
                return port == 0 ? ports().output_channels : 0;
            }

            /// @brief Reads a parameter by its NodeDescriptor::parameters[].id string.
            /// @param id The parameter's id. An unrecognized id returns 0.0f.
            /// @return The parameter's current normalized value.
            virtual float get_parameter(const char* id) const = 0;

            /// @brief Writes a parameter by its NodeDescriptor::parameters[].id string. The name
            /// carries the contract the signature cannot: the string lookup is not real-time safe.
            /// @param id The parameter's id. An unrecognized id is a no-op.
            /// @param value The value to set, in the parameter's declared [min, max] range.
            virtual void set_parameter_for_control_thread(const char* id, float value) = 0;

            /// @brief Writes a parameter by its NodeDescriptor::parameters[] index. RigGraph calls it
            /// as it drains its ParameterChange ring, so an override must only store a target value;
            /// the default is a no-op for nodes with no parameters.
            /// @param index Index into the node's NodeDescriptor::parameters[]. Out of range is a no-op.
            /// @param value The value to set, in the parameter's declared [min, max] range.
            virtual void set_parameter_for_audio_thread(std::uint16_t index, float value)
            {
                (void)index;
                (void)value;
            }

            /// @brief Clears internal state for a click-free restart. Control thread only, and never
            /// concurrently with process(): an implementation may reach state it otherwise only
            /// touches from the publisher side, such as an RtSwapSlot's active_for_publisher().
            virtual void reset() = 0;

            /// @brief Samples of delay this node adds between its input and output (FIR group
            /// delay, convolution partition, look-ahead). Constant after prepare() unless the node
            /// documents a control-thread operation that changes it, such as CabinetNode::set_cabinet()
            /// swapping a latency-bearing stage; hosts should re-read it after any such call.
            /// @return Latency in base-rate samples; 0 for a memoryless or IIR-only node.
            virtual std::uint32_t latency_samples() const
            {
                return 0;
            }

            /// @brief This node's NodeDescriptor::id, for a node registered in a node_registry.
            /// @return The registered type id, or "" for a node with no registry entry (e.g. a
            /// test fixture).
            virtual const char* type_id() const
            {
                return "";
            }
    };
} // namespace SushiDSP
