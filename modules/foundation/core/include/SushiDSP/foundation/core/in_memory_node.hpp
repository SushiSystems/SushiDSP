/**************************************************************************/
/* in_memory_node.hpp                                                     */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file in_memory_node.hpp
 * @brief Declares the in-memory AudioNode: a node of any shape whose output is an affine mix of its input.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>

#include <SushiDSP/foundation/core/audio_node.hpp>

namespace SushiDSP
{
    /// @brief Holds the most output ports an InMemoryNode can declare.
    inline constexpr std::uint32_t K_IN_MEMORY_NODE_MAX_PORTS = 16;

    /// @brief Holds the most channels one port of an InMemoryNode can carry.
    inline constexpr std::uint32_t K_IN_MEMORY_NODE_MAX_CHANNELS = 4;

    /// @brief Holds the most input channels an InMemoryNode records.
    inline constexpr std::uint32_t K_IN_MEMORY_NODE_MAX_INPUTS = 16;

    /// @brief Holds the parameter index of the gain, also reachable by the id "gain".
    inline constexpr std::uint16_t K_IN_MEMORY_NODE_GAIN_INDEX = 0;

    /// @brief Holds the parameter index of port 0's level, also reachable by the id "level".
    inline constexpr std::uint16_t K_IN_MEMORY_NODE_LEVEL_INDEX = 1;

    /// @brief Describes what an InMemoryNode declares. An unset field answers with AudioNode's default.
    struct InMemoryNodeShape
    {
            /// @brief Holds the node's input and output channel counts.
            PortLayout ports{1, 1};
            /// @brief Holds the output port count; unset keeps AudioNode's one port.
            std::optional<std::uint32_t> output_port_count;
            /// @brief Holds the width of each output port; unset keeps AudioNode's widths.
            std::optional<std::array<std::uint32_t, K_IN_MEMORY_NODE_MAX_PORTS>> port_channels;
            /// @brief Holds the latency; unset keeps AudioNode's zero.
            std::optional<std::uint32_t> latency_samples;
            /// @brief Holds the type id; null keeps AudioNode's empty id.
            const char* type_id = nullptr;
    };

    /// @brief Describes the signal an InMemoryNode writes.
    ///
    /// Channel c of output port p, which is w channels wide, is gain times the sum of every input
    /// channel k with k mod w equal to c, plus levels[p][c]. An input narrower than w repeats: the
    /// channel c then reads input channel c mod the input's width. A null input reads as silence.
    struct InMemoryNodeSignal
    {
            /// @brief Holds the factor applied to the input.
            float gain = 1.0f;
            /// @brief Holds the level added to each channel of each output port.
            std::array<std::array<float, K_IN_MEMORY_NODE_MAX_CHANNELS>, K_IN_MEMORY_NODE_MAX_PORTS> levels{};
    };

    /// @brief Holds what an InMemoryNode saw, for a test to read after the node moved into a graph.
    struct InMemoryNodeRecord
    {
            /// @brief Counts the calls to prepare().
            std::uint32_t prepare_calls = 0;
            /// @brief Counts the calls to process().
            std::uint32_t process_calls = 0;
            /// @brief Counts the calls to reset().
            std::uint32_t reset_calls = 0;
            /// @brief Counts the nodes that wrote to this record and were destroyed.
            std::uint32_t destroyed = 0;
            /// @brief Holds the argument of the last prepare().
            PrepareInfo last_prepare{0.0, 0, 0};
            /// @brief Holds the frame count of the last process().
            std::uint32_t last_frames = 0;
            /// @brief Holds the time of the last process().
            double last_time_seconds = 0.0;
            /// @brief Holds whether the last process() was given an input array.
            bool inputs_present = false;
            /// @brief Holds whether the last process() was given a port table.
            bool port_table_present = false;
            /// @brief Holds whether the last port table's port 0 was the outputs pointer.
            bool port_zero_is_outputs = false;
            /// @brief Holds the first sample of each input channel of the last process(); NaN for a null one.
            std::array<float, K_IN_MEMORY_NODE_MAX_INPUTS> first_input_samples{};
            /// @brief Holds the index of the last audio-thread parameter write, or 0xFFFF before any.
            std::uint16_t last_audio_parameter_index = 0xFFFF;
            /// @brief Holds the value of the last audio-thread parameter write.
            float last_audio_parameter_value = 0.0f;
    };

    /// @brief An AudioNode held in memory, of any shape, whose output is an affine mix of its input.
    ///
    /// It keeps every promise of AudioNode: process() fills exactly [0, frames) of every channel of
    /// every declared port and allocates nothing. Its two parameters, the gain and port 0's level,
    /// are reachable by id and by index; any other id or index changes nothing.
    class InMemoryNode final : public AudioNode
    {
        public:
            /// @brief Makes a node that writes what it sees into its own record.
            /// @param shape What the node declares.
            /// @param signal What the node writes.
            InMemoryNode(InMemoryNodeShape shape, InMemoryNodeSignal signal)
                : shape_(shape), signal_(signal), record_(&own_record_)
            {
            }

            /// @brief Makes a node that writes what it sees into @p record.
            /// @param shape What the node declares.
            /// @param signal What the node writes.
            /// @param record The record the caller reads; it outlives the node.
            InMemoryNode(InMemoryNodeShape shape, InMemoryNodeSignal signal, InMemoryNodeRecord& record)
                : shape_(shape), signal_(signal), record_(&record)
            {
            }

            /// @brief Counts the destruction in the record.
            ~InMemoryNode() override
            {
                ++record_->destroyed;
            }

            InMemoryNode(const InMemoryNode&) = delete;
            InMemoryNode& operator=(const InMemoryNode&) = delete;

            /// @brief Records the call and its argument.
            void prepare(const PrepareInfo& info) override
            {
                ++record_->prepare_calls;
                record_->last_prepare = info;
            }

            /// @brief Records what it was given and writes the mix to every declared port.
            void process(ProcessContext& context) override
            {
                record_call(context);
                const std::uint32_t ports = output_port_count();
                for (std::uint32_t port = 0; port < ports && port < K_IN_MEMORY_NODE_MAX_PORTS; ++port)
                {
                    float* const* outputs = port_outputs(context, port);
                    if (outputs != nullptr)
                    {
                        write_port(context, port, outputs);
                    }
                }
            }

            /// @brief Returns the declared layout.
            PortLayout ports() const override
            {
                return shape_.ports;
            }

            /// @brief Returns the declared port count, or AudioNode's when unset.
            std::uint32_t output_port_count() const override
            {
                return shape_.output_port_count.has_value() ? *shape_.output_port_count
                                                            : AudioNode::output_port_count();
            }

            /// @brief Returns the declared width of @p port, or AudioNode's when unset.
            std::uint32_t output_port_channels(std::uint32_t port) const override
            {
                if (!shape_.port_channels.has_value())
                {
                    return AudioNode::output_port_channels(port);
                }
                return port < K_IN_MEMORY_NODE_MAX_PORTS ? (*shape_.port_channels)[port] : 0;
            }

            /// @brief Returns the gain for "gain", port 0's first level for "level", and 0 otherwise.
            float get_parameter(const char* id) const override
            {
                if (std::strcmp(id, "gain") == 0)
                {
                    return signal_.gain;
                }
                if (std::strcmp(id, "level") == 0)
                {
                    return signal_.levels[0][0];
                }
                return 0.0f;
            }

            /// @brief Writes the gain for "gain" and every level of port 0 for "level"; any other id is a no-op.
            void set_parameter_for_control_thread(const char* id, float value) override
            {
                if (std::strcmp(id, "gain") == 0)
                {
                    set_parameter_for_audio_thread(K_IN_MEMORY_NODE_GAIN_INDEX, value);
                }
                else if (std::strcmp(id, "level") == 0)
                {
                    set_parameter_for_audio_thread(K_IN_MEMORY_NODE_LEVEL_INDEX, value);
                }
            }

            /// @brief Records the write; index 0 sets the gain, index 1 every level of port 0, any other nothing.
            void set_parameter_for_audio_thread(std::uint16_t index, float value) override
            {
                record_->last_audio_parameter_index = index;
                record_->last_audio_parameter_value = value;
                if (index == K_IN_MEMORY_NODE_GAIN_INDEX)
                {
                    signal_.gain = value;
                }
                else if (index == K_IN_MEMORY_NODE_LEVEL_INDEX)
                {
                    signal_.levels[0].fill(value);
                }
            }

            /// @brief Counts the call.
            void reset() override
            {
                ++record_->reset_calls;
            }

            /// @brief Returns the declared latency, or AudioNode's when unset.
            std::uint32_t latency_samples() const override
            {
                return shape_.latency_samples.has_value() ? *shape_.latency_samples : AudioNode::latency_samples();
            }

            /// @brief Returns the declared type id, or AudioNode's when unset.
            const char* type_id() const override
            {
                return shape_.type_id != nullptr ? shape_.type_id : AudioNode::type_id();
            }

            /// @brief Returns the record this node writes.
            const InMemoryNodeRecord& record() const
            {
                return *record_;
            }

        private:
            /// @brief Records the counts, the context's tables and the first sample of each input.
            void record_call(const ProcessContext& context)
            {
                ++record_->process_calls;
                record_->last_frames = context.frames;
                record_->last_time_seconds = context.time_seconds;
                record_->inputs_present = context.inputs != nullptr;
                record_->port_table_present = context.port_outputs != nullptr;
                record_->port_zero_is_outputs =
                    context.port_outputs != nullptr && context.port_outputs[0] == context.outputs;
                record_->first_input_samples.fill(std::numeric_limits<float>::quiet_NaN());
                const std::uint32_t inputs = input_count();
                for (std::uint32_t input = 0; input < inputs; ++input)
                {
                    const float* channel = input_channel(context, input);
                    if (channel != nullptr && context.frames > 0)
                    {
                        record_->first_input_samples[input] = channel[0];
                    }
                }
            }

            /// @brief Returns the channel pointers of @p port, or null when the context does not carry the port.
            static float* const* port_outputs(const ProcessContext& context, std::uint32_t port)
            {
                if (context.port_outputs != nullptr)
                {
                    return context.port_outputs[port];
                }
                return port == 0 ? context.outputs : nullptr;
            }

            /// @brief Returns how many input channels the node reads, bounded by what it records.
            std::uint32_t input_count() const
            {
                return shape_.ports.input_channels < K_IN_MEMORY_NODE_MAX_INPUTS ? shape_.ports.input_channels
                                                                                 : K_IN_MEMORY_NODE_MAX_INPUTS;
            }

            /// @brief Returns input channel @p input, or null when the context carries none.
            static const float* input_channel(const ProcessContext& context, std::uint32_t input)
            {
                return context.inputs != nullptr ? context.inputs[input] : nullptr;
            }

            /// @brief Returns the input sum that feeds output channel @p channel of a port @p width wide.
            float input_sum(const ProcessContext& context, std::uint32_t channel, std::uint32_t width,
                            std::uint32_t frame) const
            {
                const std::uint32_t inputs = input_count();
                float sum = 0.0f;
                if (inputs == 0)
                {
                    return sum;
                }
                if (inputs < width)
                {
                    const float* source = input_channel(context, channel % inputs);
                    return source != nullptr ? source[frame] : sum;
                }
                for (std::uint32_t input = channel; input < inputs; input += width)
                {
                    const float* source = input_channel(context, input);
                    sum += source != nullptr ? source[frame] : 0.0f;
                }
                return sum;
            }

            /// @brief Writes the mix into every channel of @p port.
            void write_port(const ProcessContext& context, std::uint32_t port, float* const* outputs) const
            {
                const std::uint32_t declared = output_port_channels(port);
                const std::uint32_t width =
                    declared < K_IN_MEMORY_NODE_MAX_CHANNELS ? declared : K_IN_MEMORY_NODE_MAX_CHANNELS;
                for (std::uint32_t channel = 0; channel < width; ++channel)
                {
                    const float level = signal_.levels[port][channel];
                    for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                    {
                        outputs[channel][frame] = signal_.gain * input_sum(context, channel, width, frame) + level;
                    }
                }
            }

            InMemoryNodeShape shape_;
            InMemoryNodeSignal signal_;
            InMemoryNodeRecord own_record_;
            InMemoryNodeRecord* record_;
    };
} // namespace SushiDSP
