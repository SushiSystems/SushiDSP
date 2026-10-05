/****************************************************************/
/* IAudioNode.hpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file IAudioNode.hpp
 * @brief Declares the DSP graph node interface and its prepare and process contexts.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>

#include "SushiDSP/core/PortLayout.hpp"

namespace SushiDSP
{
    /// @brief Parameters supplied to IAudioNode::prepare(), fixed for the node's lifetime.
    struct PrepareInfo
    {
        double        sample_rate;
        std::uint32_t max_block;
        std::uint32_t channels;
    };

    /// @brief Per-block processing context. process() must fill exactly [0, frames) of each output channel.
    struct ProcessContext
    {
        float* const*       outputs;
        const float* const* inputs;
        std::uint32_t       frames;
        double               time_seconds;
    };

    /// @brief One DSP graph node. Implementations must be real-time safe in process().
    struct IAudioNode
    {
        virtual ~IAudioNode() = default;

        /// @brief Acquires all memory the node will need. Never called on the processing thread.
        /// @param info Sample rate, max block size, and channel count for this node's lifetime.
        virtual void prepare(const PrepareInfo& info) = 0;

        /// @brief Fills context.outputs with exactly context.frames of audio.
        /// @param context The per-block input/output buffers and frame count.
        /// No allocation, no lock, no syscall, no exception.
        virtual void process(ProcessContext& context) = 0;

        /// @brief Returns this node's fixed input/output channel counts.
        /// @return The node's PortLayout.
        virtual PortLayout ports() const = 0;

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
        virtual void set_parameter_for_audio_thread(std::uint16_t index, float value) { (void)index; (void)value; }

        /// @brief Clears internal state for a click-free restart. Control thread only, and never
        /// concurrently with process(): an implementation may reach state it otherwise only
        /// touches from the publisher side, such as an RtSwapSlot's active_for_publisher().
        virtual void reset() = 0;

        /// @brief Samples of delay this node adds between its input and output (FIR group
        /// delay, convolution partition, look-ahead). Constant after prepare() unless the node
        /// documents a control-thread operation that changes it, such as CabinetNode::set_cabinet()
        /// swapping a latency-bearing stage; hosts should re-read it after any such call.
        /// @return Latency in base-rate samples; 0 for a memoryless or IIR-only node.
        virtual std::uint32_t latency_samples() const { return 0; }

        /// @brief This node's NodeDescriptor::id, for a node registered in a node_registry.
        /// @return The registered type id, or "" for a node with no registry entry (e.g. a
        /// test fixture).
        virtual const char* type_id() const { return ""; }
    };
}
