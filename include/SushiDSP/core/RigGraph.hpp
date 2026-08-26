/**************************************************************************/
/* RigGraph.hpp                                                           */
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
#include <memory>
#include <stdexcept>
#include <vector>

#include "SushiDSP/core/BufferPool.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/ParameterChange.hpp"
#include "SushiDSP/core/SpscRing.hpp"

namespace SushiDSP
{
    /// @brief A user-editable directed graph of IAudioNode instances, compiled to a topological order and replayed per block.
    class RigGraph
    {
    public:
        /// @brief Maximum parameter changes that may be pending between two process() calls.
        static constexpr std::size_t kParameterRingCapacity = 1024;

        /// @brief Maximum outgoing edges from, or incoming edges into, any single node.
        /// Bounds the fixed-size input-gather array RigGraph::process() uses per node.
        static constexpr std::size_t kMaxFanInOut = 8;

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

        /// @brief Removes a node and every edge touching it. Not real-time safe; call only off
        /// the processing thread. The handle becomes invalid; other handles are unaffected.
        /// @param handle The node to remove.
        void remove_node(NodeHandle handle);

        /// @brief Removes a single edge, if present. Not real-time safe.
        /// @param from The upstream node.
        /// @param to The downstream node.
        void disconnect(NodeHandle from, NodeHandle to);

        /// @brief Looks up a live node by handle.
        /// @param handle The node's handle.
        /// @return The node, or nullptr if handle is out of range or was removed.
        IAudioNode* node(NodeHandle handle) const;

        /// @brief Lists every live (non-removed) node handle, in add_node() order.
        /// @return The live handles.
        std::vector<NodeHandle> nodes() const;

        /// @brief Lists the handles of every node with an edge into handle, in connect() call
        /// order. Not real-time safe; call only off the processing thread.
        /// @param handle The downstream node to query.
        /// @return The upstream handles, or an empty vector if handle has no upstream edge.
        std::vector<NodeHandle> upstream_of(NodeHandle handle) const;

        /// @brief Queues a parameter write for the next process() call. Control thread only;
        /// lock-free and allocation-free.
        /// @param change The node, parameter index, and value to apply.
        /// @return true if queued, false if the ring is full and the change was dropped.
        bool push_parameter_change(const ParameterChange& change);

        /// @brief Processes one block through every node in topological order. Real-time safe.
        /// @param context The block's output buffers and frame count.
        void process(ProcessContext& context);

        /// @brief Sum of latency_samples() over the compiled chain. Allocation-free; call after
        /// compile(). Sums the chain; a parallel branch-and-rejoin graph would need the max over
        /// branches instead.
        /// @return Total latency in base-rate samples.
        std::uint32_t latency_samples() const;

    private:
        struct Edge { NodeHandle from; NodeHandle to; };

        std::uint32_t                             block_size_;
        BufferPool                                buffer_pool_;
        std::vector<std::unique_ptr<IAudioNode>>  nodes_;
        std::vector<Edge>                         edges_;
        std::vector<NodeHandle>                   topological_order_;
        NodeHandle                                sink_handle_ = 0;
        std::uint32_t                             cached_latency_samples_ = 0;
        std::vector<float*>                       node_output_scratch_;
        SpscRing<ParameterChange>                 parameter_ring_{ kParameterRingCapacity };
    };
}
