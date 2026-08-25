// RigGraph.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "SushiDSP/core/RigGraph.hpp"

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

    void RigGraph::compile(const PrepareInfo& info)
    {
        std::vector<std::uint32_t> in_degree(nodes_.size(), 0);
        for (const Edge& edge : edges_)
        {
            ++in_degree[edge.to];
        }

        std::queue<NodeHandle> ready;
        for (NodeHandle handle = 0; handle < nodes_.size(); ++handle)
        {
            if (in_degree[handle] == 0)
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
                    if (--in_degree[edge.to] == 0)
                    {
                        ready.push(edge.to);
                    }
                }
            }
        }

        for (auto& node : nodes_)
        {
            node->prepare(info);
        }
    }

    void RigGraph::process(ProcessContext& context)
    {
        std::vector<float*> node_output(nodes_.size(), nullptr);

        for (NodeHandle handle : topological_order_)
        {
            float* node_buffer = buffer_pool_.acquire(context.frames);

            const float* upstream_input = nullptr;
            for (const Edge& edge : edges_)
            {
                if (edge.to == handle)
                {
                    upstream_input = node_output[edge.from];
                    break;
                }
            }

            float* output_ptr[1] = { node_buffer };
            const float* input_ptr[1] = { upstream_input };

            ProcessContext node_context{ output_ptr, upstream_input != nullptr ? input_ptr : nullptr, context.frames, context.time_seconds };
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

        for (float* buffer : node_output)
        {
            if (buffer != nullptr)
            {
                buffer_pool_.release(buffer);
            }
        }
    }
}
