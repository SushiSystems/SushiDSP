/**************************************************************************/
/* RigGraph.cpp                                                           */
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

#include "SushiDSP/core/RigGraph.hpp"

#include <algorithm>
#include <cassert>
#include <queue>

namespace SushiDSP
{
    RigGraph::RigGraph(std::uint32_t block_size, std::uint32_t max_live_buffers)
        : block_size_(block_size)
        , buffer_pool_(block_size, max_live_buffers)
    {
    }

    NodeHandle RigGraph::add_node(std::unique_ptr<IAudioNode> node)
    {
        nodes_.push_back(std::move(node));
        return static_cast<NodeHandle>(nodes_.size() - 1);
    }

    void RigGraph::connect(NodeHandle from, NodeHandle to)
    {
        edges_.push_back({ from, to });
    }

    void RigGraph::remove_node(NodeHandle handle)
    {
        if (handle < nodes_.size())
        {
            nodes_[handle].reset();
        }
        edges_.erase(
            std::remove_if(edges_.begin(), edges_.end(),
                [handle](const Edge& edge) { return edge.from == handle || edge.to == handle; }),
            edges_.end());
    }

    void RigGraph::disconnect(NodeHandle from, NodeHandle to)
    {
        edges_.erase(
            std::remove_if(edges_.begin(), edges_.end(),
                [from, to](const Edge& edge) { return edge.from == from && edge.to == to; }),
            edges_.end());
    }

    IAudioNode* RigGraph::node(NodeHandle handle) const
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

    void RigGraph::compile(const PrepareInfo& info)
    {
        // process() holds one pool buffer per node for the whole block, so a graph larger than the
        // pool would hand out nullptr and crash on the RT thread.
        assert(nodes_.size() <= buffer_pool_.capacity() && "RigGraph: more nodes than BufferPool can hold live simultaneously");

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

        topological_order_.clear();
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

        node_output_scratch_.assign(nodes_.size(), nullptr);

        for (auto& node : nodes_)
        {
            if (node != nullptr)
            {
                node->prepare(info);
            }
        }
    }

    std::uint32_t RigGraph::latency_samples() const
    {
        std::uint32_t total = 0;
        for (NodeHandle handle : topological_order_)
        {
            if (nodes_[handle] != nullptr) { total += nodes_[handle]->latency_samples(); }
        }
        return total;
    }

    bool RigGraph::push_parameter_change(const ParameterChange& change)
    {
        return parameter_ring_.push(change);
    }

    void RigGraph::process(ProcessContext& context)
    {
        ParameterChange change;
        while (parameter_ring_.pop(change))
        {
            if (change.node < nodes_.size() && nodes_[change.node] != nullptr)
            {
                nodes_[change.node]->set_parameter(change.parameter_index, change.value);
            }
        }

        auto& node_output = node_output_scratch_;
        std::fill(node_output.begin(), node_output.end(), nullptr);

        for (NodeHandle handle : topological_order_)
        {
            if (nodes_[handle] == nullptr)
            {
                continue;
            }

            float* node_buffer = buffer_pool_.acquire(context.frames);

            bool has_upstream_edge = false;
            const float* upstream_input = nullptr;
            for (const Edge& edge : edges_)
            {
                if (edge.to == handle)
                {
                    upstream_input = node_output[edge.from];
                    has_upstream_edge = true;
                    break;
                }
            }

            float* output_ptr[1] = { node_buffer };
            const float* input_ptr[1] = { upstream_input };

            // A node with no upstream edge is a root node. Feed it the graph's own live
            // input (e.g. an ASIO device's captured audio) rather than always nullptr, so a
            // rig can start directly from live input without a synthetic source node.
            const float* const* node_inputs = nullptr;
            if (has_upstream_edge)
            {
                node_inputs = input_ptr;
            }
            else if (context.inputs != nullptr)
            {
                node_inputs = context.inputs;
            }

            ProcessContext node_context{ output_ptr, node_inputs, context.frames, context.time_seconds };
            nodes_[handle]->process(node_context);

            node_output[handle] = node_buffer;
        }

        if (!topological_order_.empty())
        {
            const NodeHandle last = topological_order_.back();
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = node_output[last][frame];
            }
        }
        else
        {
            // An empty graph must still fully define its output: leaving the caller's buffer
            // untouched would replay whatever was previously in it (e.g. SDL's raw stream
            // buffer), which is audible garbage looping at block rate.
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = 0.0f;
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
}
