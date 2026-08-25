// RigGraph.hpp
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

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "SushiDSP/core/BufferPool.hpp"
#include "SushiDSP/core/IAudioNode.hpp"

namespace SushiDSP
{
    using NodeHandle = std::uint32_t;

    /// @brief A user-editable directed graph of IAudioNode instances, compiled to a topological order and replayed per block.
    class RigGraph
    {
    public:
        /// @brief Constructs an empty rig graph.
        /// @param block_size Frames per process() call.
        /// @param max_live_buffers Maximum transient buffers live at once during a block.
        RigGraph(std::uint32_t block_size, std::uint32_t max_live_buffers);

        /// @brief Adds a node to the graph, unconnected. Not real-time safe; call only off the processing thread.
        /// @param node The node to take ownership of.
        /// @return A handle identifying the node for connect() calls.
        NodeHandle add_node(std::unique_ptr<IAudioNode> node);

        /// @brief Connects one node's output to another's input. Not real-time safe.
        /// @param from The upstream node.
        /// @param to The downstream node.
        void connect(NodeHandle from, NodeHandle to);

        /// @brief Topologically sorts the graph and calls prepare() on every node. Not real-time safe; call after every add_node/connect.
        /// @param info Sample rate, block size, and channel count for every node.
        void compile(const PrepareInfo& info);

        /// @brief Processes one block through every node in topological order. Real-time safe.
        /// @param context The block's output buffers and frame count.
        void process(ProcessContext& context);

    private:
        struct Edge { NodeHandle from; NodeHandle to; };

        std::uint32_t                             block_size_;
        BufferPool                                buffer_pool_;
        std::vector<std::unique_ptr<IAudioNode>>  nodes_;
        std::vector<Edge>                         edges_;
        std::vector<NodeHandle>                   topological_order_;
        std::vector<float*>                       node_output_scratch_;
    };
}
