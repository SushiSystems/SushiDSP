/**************************************************************************/
/* IAudioNode.hpp                                                         */
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

        /// @brief Writes a parameter by its NodeDescriptor::parameters[].id string. Control
        /// thread only: the string lookup is not real-time safe.
        /// @param id The parameter's id. An unrecognized id is a no-op.
        /// @param value The value to set, in the parameter's declared [min, max] range.
        virtual void set_parameter(const char* id, float value) = 0;

        /// @brief Writes a parameter by its NodeDescriptor::parameters[] index. Called on the
        /// processing thread as RigGraph drains its ParameterChange ring, so an override must
        /// only store a target value; the default is a no-op for nodes with no parameters.
        /// @param index Index into the node's NodeDescriptor::parameters[]. Out of range is a no-op.
        /// @param value The value to set, in the parameter's declared [min, max] range.
        virtual void set_parameter(std::uint16_t index, float value) { (void)index; (void)value; }

        /// @brief Clears internal state for a click-free restart.
        virtual void reset() = 0;

        /// @brief Samples of delay this node adds between its input and output (FIR group
        /// delay, convolution partition, look-ahead). Constant after prepare() unless the node
        /// documents a control-thread operation that changes it (e.g. Jcm800::set_cabinet(),
        /// which swaps a latency-bearing stage); hosts should re-read it after any such call.
        /// @return Latency in base-rate samples; 0 for a memoryless or IIR-only node.
        virtual std::uint32_t latency_samples() const { return 0; }
    };
}
