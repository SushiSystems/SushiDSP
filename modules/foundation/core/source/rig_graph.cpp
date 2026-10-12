/**************************************************************************/
/* rig_graph.cpp                                                          */
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
 * @file rig_graph.cpp
 * @brief Implements RigGraph's compilation to a topological order and its per-block replay.
 * @author Mustafa Garip
 */

#include "SushiDSP/core/rig_graph.hpp"

#include "SushiDSP/core/denormal_guard.hpp"

#include <algorithm>
#include <cassert>
#include <queue>
#include <string>

namespace SushiDSP
{
    RigGraph::RigGraph(std::uint32_t block_size, std::uint32_t max_live_buffers)
        : block_size_(block_size), initial_max_live_buffers_(max_live_buffers),
          buffer_pool_(block_size, max_live_buffers)
    {
    }

    bool RigGraph::channels_adapt(std::uint32_t from_channels, std::uint32_t to_channels)
    {
        if (from_channels == 0 || to_channels == 0)
        {
            return from_channels == to_channels;
        }
        return from_channels == to_channels || (from_channels == 1 && to_channels == 2) ||
               (from_channels == 2 && to_channels == 1);
    }

    RigGraph::NodeChannels RigGraph::channels_of(const PortLayout& layout)
    {
        const std::uint32_t input_width = std::min(layout.input_channels, layout.output_channels);
        const std::uint32_t input_slots =
            (input_width == 0) ? layout.input_channels : layout.input_channels / input_width;
        return NodeChannels{input_width, layout.output_channels, input_slots};
    }

    NodeHandle RigGraph::add_node(std::unique_ptr<AudioNode> node)
    {
        nodes_.push_back(std::move(node));
        return static_cast<NodeHandle>(nodes_.size() - 1);
    }

    void RigGraph::connect(NodeHandle from, NodeHandle to, std::uint32_t from_port)
    {
        std::size_t fan_out = 0;
        std::size_t fan_in = 0;
        for (const Edge& edge : edges_)
        {
            if (edge.from == from)
            {
                ++fan_out;
            }
            if (edge.to == to)
            {
                ++fan_in;
            }
        }
        if (fan_out >= K_MAX_FAN_IN_OUT)
        {
            throw std::runtime_error("RigGraph::connect: fan-out limit exceeded");
        }
        if (fan_in >= K_MAX_FAN_IN_OUT)
        {
            throw std::runtime_error("RigGraph::connect: fan-in limit exceeded");
        }
        edges_.push_back({from, to, from_port});
    }

    void RigGraph::remove_node(NodeHandle handle)
    {
        if (handle < nodes_.size())
        {
            nodes_[handle].reset();
        }
        edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
                                    [handle](const Edge& edge)
                                    {
                                        return edge.from == handle || edge.to == handle;
                                    }),
                     edges_.end());
    }

    void RigGraph::disconnect(NodeHandle from, NodeHandle to)
    {
        edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
                                    [from, to](const Edge& edge)
                                    {
                                        return edge.from == from && edge.to == to;
                                    }),
                     edges_.end());
    }

    AudioNode* RigGraph::node(NodeHandle handle) const
    {
        if (handle >= nodes_.size())
        {
            return nullptr;
        }
        return nodes_[handle].get();
    }

    std::vector<NodeHandle> RigGraph::nodes() const
    {
        std::vector<NodeHandle> result;
        for (NodeHandle handle = 0; handle < nodes_.size(); ++handle)
        {
            if (nodes_[handle] != nullptr)
            {
                result.push_back(handle);
            }
        }
        return result;
    }

    std::vector<NodeHandle> RigGraph::upstream_of(NodeHandle handle) const
    {
        std::vector<NodeHandle> result;
        for (const Edge& edge : edges_)
        {
            if (edge.to == handle)
            {
                result.push_back(edge.from);
            }
        }
        return result;
    }

    void RigGraph::compile(const PrepareInfo& info)
    {
        // Every cached compile() result is cleared first (modules/foundation/core/README.md).
        cached_latency_samples_ = 0;
        sink_handle_ = 0;
        topological_order_.clear();

        std::vector<std::uint32_t> in_degree(nodes_.size(), 0);
        for (const Edge& edge : edges_)
        {
            ++in_degree[edge.to];
        }

        std::queue<NodeHandle> ready;
        for (NodeHandle handle = 0; handle < nodes_.size(); ++handle)
        {
            if (nodes_[handle] != nullptr && in_degree[handle] == 0)
            {
                ready.push(handle);
            }
        }

        while (!ready.empty())
        {
            const NodeHandle current = ready.front();
            ready.pop();
            topological_order_.push_back(current);

            for (const Edge& edge : edges_)
            {
                if (edge.from == current)
                {
                    if (--in_degree[edge.to] == 0 && nodes_[edge.to] != nullptr)
                    {
                        ready.push(edge.to);
                    }
                }
            }
        }

        const std::size_t live_node_count =
            static_cast<std::size_t>(std::count_if(nodes_.begin(), nodes_.end(),
                                                   [](const std::unique_ptr<AudioNode>& node)
                                                   {
                                                       return node != nullptr;
                                                   }));
        if (topological_order_.size() != live_node_count)
        {
            throw std::runtime_error("RigGraph::compile: cycle detected");
        }

        std::vector<bool> has_outgoing_edge(nodes_.size(), false);
        std::vector<bool> has_incoming_edge(nodes_.size(), false);
        for (const Edge& edge : edges_)
        {
            has_outgoing_edge[edge.from] = true;
            has_incoming_edge[edge.to] = true;
        }
        std::size_t sink_count = 0;
        for (NodeHandle handle : topological_order_)
        {
            const bool is_sink_candidate =
                !has_outgoing_edge[handle] && (has_incoming_edge[handle] || live_node_count == 1);
            if (is_sink_candidate)
            {
                sink_handle_ = handle;
                ++sink_count;
            }
        }
        if (sink_count > 1)
        {
            throw std::runtime_error(
                "RigGraph::compile: multiple sink nodes (ambiguous graph output)");
        }
        if (sink_count == 0 && !topological_order_.empty())
        {
            throw std::runtime_error("RigGraph::compile: no sink node found");
        }

        node_channels_.assign(nodes_.size(), NodeChannels{0, 0, 0});
        for (NodeHandle handle : topological_order_)
        {
            node_channels_[handle] = channels_of(nodes_[handle]->ports());
        }

        port_count_.assign(nodes_.size(), 0);
        port_widths_.assign(nodes_.size() * K_MAX_OUTPUT_PORTS, 0);
        for (NodeHandle handle : topological_order_)
        {
            AudioNode& node = *nodes_[handle];
            const std::uint32_t count = node.output_port_count();
            if (count == 0 || count > K_MAX_OUTPUT_PORTS)
            {
                throw std::runtime_error("RigGraph::compile: node " + std::to_string(handle) +
                                         " declares " + std::to_string(count) +
                                         " output port(s), the limit is " +
                                         std::to_string(K_MAX_OUTPUT_PORTS));
            }
            port_count_[handle] = count;
            for (std::uint32_t port = 0; port < count; ++port)
            {
                const std::uint32_t width = node.output_port_channels(port);
                if (width == 0 || width > K_MAX_CHANNELS)
                {
                    throw std::runtime_error("RigGraph::compile: output port " +
                                             std::to_string(port) + " of node " +
                                             std::to_string(handle) + " has no channel width");
                }
                port_widths_[handle * K_MAX_OUTPUT_PORTS + port] = width;
            }
            if (port_widths_[handle * K_MAX_OUTPUT_PORTS] != node_channels_[handle].output_width)
            {
                throw std::runtime_error("RigGraph::compile: output port 0 of node " +
                                         std::to_string(handle) +
                                         " disagrees with its PortLayout");
            }
        }

        for (const Edge& edge : edges_)
        {
            if (edge.from_port >= port_count_[edge.from])
            {
                throw std::runtime_error("RigGraph::compile: node " + std::to_string(edge.from) +
                                         " has no output port " + std::to_string(edge.from_port));
            }
            const std::uint32_t from_channels =
                port_widths_[edge.from * K_MAX_OUTPUT_PORTS + edge.from_port];
            const std::uint32_t to_channels = node_channels_[edge.to].input_width;
            if (from_channels > K_MAX_CHANNELS || to_channels > K_MAX_CHANNELS ||
                !channels_adapt(from_channels, to_channels))
            {
                throw std::runtime_error(
                    "RigGraph::compile: no channel adaptation rule from node " +
                    std::to_string(edge.from) + " (" + std::to_string(from_channels) +
                    " channels) to node " + std::to_string(edge.to) + " (" +
                    std::to_string(to_channels) + " channels)");
            }
        }

        host_output_channels_ = std::max(1u, info.channels);
        if (!topological_order_.empty())
        {
            const std::uint32_t sink_channels = node_channels_[sink_handle_].output_width;
            if (sink_channels > K_MAX_CHANNELS ||
                !channels_adapt(sink_channels, host_output_channels_))
            {
                throw std::runtime_error(
                    "RigGraph::compile: no channel adaptation rule from sink node " +
                    std::to_string(sink_handle_) + " (" + std::to_string(sink_channels) +
                    " channels) to the host output (" + std::to_string(host_output_channels_) +
                    " channels)");
            }
        }

        max_output_channels_ = 1;
        std::uint32_t total_output_channels = 0;
        for (NodeHandle handle : topological_order_)
        {
            for (std::uint32_t port = 0; port < port_count_[handle]; ++port)
            {
                const std::uint32_t width = port_widths_[handle * K_MAX_OUTPUT_PORTS + port];
                max_output_channels_ = std::max(max_output_channels_, width);
                total_output_channels += width;
            }
        }

        // The gather reserve is the widest single gather, not one buffer per edge in the graph.
        live_buffer_count_ =
            total_output_channels + static_cast<std::uint32_t>(K_MAX_FAN_IN_OUT) * K_MAX_CHANNELS;

        const std::uint32_t pool_capacity = std::max(live_buffer_count_, initial_max_live_buffers_);
        if (buffer_pool_.capacity() != pool_capacity)
        {
            buffer_pool_ = BufferPool(block_size_, pool_capacity);
        }

        node_output_scratch_.assign(nodes_.size() * K_MAX_OUTPUT_PORTS * max_output_channels_,
                                    nullptr);

        for (auto& node : nodes_)
        {
            if (node != nullptr)
            {
                node->prepare(info);
            }
        }

        std::vector<std::uint32_t> latency_at(nodes_.size(), 0);
        for (NodeHandle handle : topological_order_)
        {
            std::uint32_t upstream_max = 0;
            for (const Edge& edge : edges_)
            {
                if (edge.to == handle)
                {
                    upstream_max = std::max(upstream_max, latency_at[edge.from]);
                }
            }
            latency_at[handle] = upstream_max + nodes_[handle]->latency_samples();
        }
        cached_latency_samples_ = topological_order_.empty() ? 0u : latency_at[sink_handle_];
    }

    bool RigGraph::is_empty() const noexcept
    {
        return topological_order_.empty();
    }

    std::uint32_t RigGraph::latency_samples() const
    {
        return cached_latency_samples_;
    }

    std::uint32_t RigGraph::live_buffer_count() const
    {
        return live_buffer_count_;
    }

    bool RigGraph::push_parameter_change(const ParameterChange& change)
    {
        return parameter_ring_.push(change);
    }

    void RigGraph::process(ProcessContext& context)
    {
        DenormalGuard guard;

        ParameterChange change;
        while (parameter_ring_.pop(change))
        {
            if (change.node < nodes_.size() && nodes_[change.node] != nullptr)
            {
                nodes_[change.node]->set_parameter_for_audio_thread(change.parameter_index,
                                                                    change.value);
            }
        }

        auto& node_output = node_output_scratch_;
        std::fill(node_output.begin(), node_output.end(), nullptr);

        for (NodeHandle handle : topological_order_)
        {
            // compile() accepts only an order covering exactly the live nodes; the handle is non-null.
            assert(nodes_[handle] != nullptr);

            const NodeChannels channels = node_channels_[handle];

            for (std::uint32_t port = 0; port < K_MAX_OUTPUT_PORTS; ++port)
            {
                const std::uint32_t width =
                    port < port_count_[handle] ? port_widths_[handle * K_MAX_OUTPUT_PORTS + port]
                                               : 0u;
                for (std::uint32_t channel = 0; channel < K_MAX_CHANNELS; ++channel)
                {
                    port_output_ptrs_[port][channel] = nullptr;
                }
                for (std::uint32_t channel = 0; channel < width; ++channel)
                {
                    // compile() sizes the pool at sum(output channels) + K_MAX_FAN_IN_OUT * K_MAX_CHANNELS, so acquire() cannot fail here.
                    float* node_buffer = buffer_pool_.acquire(context.frames);
                    assert(node_buffer != nullptr &&
                           "RigGraph: BufferPool exhausted; compile() sizes it from live_buffer_count()");
                    node_output[(handle * K_MAX_OUTPUT_PORTS + port) * max_output_channels_ +
                                channel] = node_buffer;
                    port_output_ptrs_[port][channel] = node_buffer;
                }
                port_output_table_[port] = width > 0 ? port_output_ptrs_[port] : nullptr;
            }

            // slots * width indexes gathered_inputs_, so both are clamped to the bounds that array was sized from.
            const std::uint32_t width =
                std::min(std::max(channels.input_width, 1u), K_MAX_CHANNELS);
            const std::uint32_t slots = std::min(std::max(channels.input_slots, 1u),
                                                 static_cast<std::uint32_t>(K_MAX_FAN_IN_OUT));
            for (std::uint32_t slot_channel = 0; slot_channel < slots * width; ++slot_channel)
            {
                gathered_inputs_[slot_channel] = nullptr;
                gather_scratch_[slot_channel] = nullptr;
            }

            std::uint32_t edge_index = 0;
            for (const Edge& edge : edges_)
            {
                if (edge.to != handle)
                {
                    continue;
                }

                // Edges past the consumer's last slot fold into that slot rather than being dropped.
                const std::uint32_t slot = std::min(edge_index, slots - 1);
                ++edge_index;

                const std::size_t producer_slot = edge.from * K_MAX_OUTPUT_PORTS + edge.from_port;
                const std::uint32_t producer_width = port_widths_[producer_slot];
                const float* producer[K_MAX_CHANNELS] = {};
                for (std::uint32_t channel = 0; channel < producer_width; ++channel)
                {
                    producer[channel] = node_output[producer_slot * max_output_channels_ + channel];
                }

                const std::uint32_t base = slot * width;
                if (gathered_inputs_[base] == nullptr)
                {
                    if (producer_width == width)
                    {
                        for (std::uint32_t channel = 0; channel < width; ++channel)
                        {
                            gathered_inputs_[base + channel] = producer[channel];
                        }
                    }
                    else if (producer_width == 1)
                    {
                        for (std::uint32_t channel = 0; channel < width; ++channel)
                        {
                            gathered_inputs_[base + channel] = producer[0];
                        }
                    }
                    else
                    {
                        float* summed = buffer_pool_.acquire(context.frames);
                        assert(summed != nullptr &&
                               "RigGraph: BufferPool exhausted during input gather");
                        for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                        {
                            summed[frame] = (producer[0][frame] + producer[1][frame]) * 0.5f;
                        }
                        gather_scratch_[base] = summed;
                        gathered_inputs_[base] = summed;
                    }
                }
                else
                {
                    for (std::uint32_t channel = 0; channel < width; ++channel)
                    {
                        if (gather_scratch_[base + channel] == nullptr)
                        {
                            float* accumulator = buffer_pool_.acquire(context.frames);
                            assert(accumulator != nullptr &&
                                   "RigGraph: BufferPool exhausted during fan-in sum");
                            const float* existing = gathered_inputs_[base + channel];
                            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                            {
                                accumulator[frame] = existing != nullptr ? existing[frame] : 0.0f;
                            }
                            gather_scratch_[base + channel] = accumulator;
                            gathered_inputs_[base + channel] = accumulator;
                        }
                    }

                    if (producer_width == width)
                    {
                        for (std::uint32_t channel = 0; channel < width; ++channel)
                        {
                            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                            {
                                gather_scratch_[base + channel][frame] += producer[channel][frame];
                            }
                        }
                    }
                    else if (producer_width == 1)
                    {
                        for (std::uint32_t channel = 0; channel < width; ++channel)
                        {
                            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                            {
                                gather_scratch_[base + channel][frame] += producer[0][frame];
                            }
                        }
                    }
                    else
                    {
                        for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                        {
                            gather_scratch_[base][frame] +=
                                (producer[0][frame] + producer[1][frame]) * 0.5f;
                        }
                    }
                }
            }

            // A root node reads the live input through slot 0 of the gather array (modules/foundation/core/README.md).
            if (edge_index == 0 && context.inputs != nullptr)
            {
                for (std::uint32_t channel = 0; channel < width; ++channel)
                {
                    gathered_inputs_[channel] = context.inputs[0];
                }
                edge_index = 1;
            }
            const float* const* node_inputs = (edge_index > 0) ? gathered_inputs_ : nullptr;

            ProcessContext node_context{port_output_ptrs_[0], node_inputs, context.frames,
                                        context.time_seconds, port_output_table_};
            nodes_[handle]->process(node_context);

            for (std::uint32_t slot_channel = 0; slot_channel < slots * width; ++slot_channel)
            {
                if (gather_scratch_[slot_channel] != nullptr)
                {
                    buffer_pool_.release(gather_scratch_[slot_channel]);
                    gather_scratch_[slot_channel] = nullptr;
                }
            }
        }

        if (!topological_order_.empty())
        {
            const std::uint32_t sink_width = node_channels_[sink_handle_].output_width;
            const float* sink_output[K_MAX_CHANNELS] = {};
            for (std::uint32_t channel = 0; channel < sink_width; ++channel)
            {
                sink_output[channel] =
                    node_output[(sink_handle_ * K_MAX_OUTPUT_PORTS) * max_output_channels_ +
                                channel];
            }

            if (sink_width == host_output_channels_)
            {
                for (std::uint32_t channel = 0; channel < host_output_channels_; ++channel)
                {
                    for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                    {
                        context.outputs[channel][frame] = sink_output[channel][frame];
                    }
                }
            }
            else if (sink_width == 1)
            {
                for (std::uint32_t channel = 0; channel < host_output_channels_; ++channel)
                {
                    for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                    {
                        context.outputs[channel][frame] = sink_output[0][frame];
                    }
                }
            }
            else
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] =
                        (sink_output[0][frame] + sink_output[1][frame]) * 0.5f;
                }
            }
        }
        else
        {
            // An empty graph still writes silence to every output channel (modules/foundation/core/README.md).
            for (std::uint32_t channel = 0; channel < host_output_channels_; ++channel)
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[channel][frame] = 0.0f;
                }
            }
        }

        for (float* buffer : node_output)
        {
            if (buffer != nullptr)
            {
                buffer_pool_.release(buffer);
            }
        }
    }
} // namespace SushiDSP
