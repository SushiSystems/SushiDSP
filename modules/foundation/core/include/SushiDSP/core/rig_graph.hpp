/**************************************************************************/
/* rig_graph.hpp                                                          */
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
 * @file rig_graph.hpp
 * @brief Declares the directed graph of audio nodes that is compiled and replayed per block.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include "SushiDSP/core/buffer_pool.hpp"
#include "SushiDSP/core/audio_node.hpp"
#include "SushiDSP/core/parameter_change.hpp"
#include "SushiDSP/core/spsc_ring.hpp"

namespace SushiDSP
{
    /// @brief A user-editable directed graph of AudioNode instances, compiled to a topological order and replayed per block.
    class RigGraph
    {
        public:
            /// @brief Maximum parameter changes that may be pending between two process() calls.
            static constexpr std::size_t K_PARAMETER_RING_CAPACITY = 1024;

            /// @brief Maximum outgoing edges from, or incoming edges into, any single node.
            /// Bounds the fixed-size input-gather array RigGraph::process() uses per node.
            static constexpr std::size_t K_MAX_FAN_IN_OUT = 8;

            /// @brief Maximum channels a node may declare on either side of its PortLayout.
            /// Bounds every fixed-size per-channel array RigGraph::process() uses.
            static constexpr std::uint32_t K_MAX_CHANNELS = 2;

            /// @brief Maximum output ports a node may declare. Bounds the fixed-size port tables
            /// RigGraph::process() fills for each node.
            static constexpr std::uint32_t K_MAX_OUTPUT_PORTS = 8;

            /// @brief Reports whether the adaptation table covers a producer/consumer channel pair.
            /// The table is 1 into 2 (copied), 2 into 1 (summed and halved), and n into n.
            /// @param from_channels The producer's channel count.
            /// @param to_channels The consumer's channel count.
            /// @return False when the pair has no rule, which compile() turns into a refusal.
            static bool channels_adapt(std::uint32_t from_channels, std::uint32_t to_channels);

            /// @brief Constructs an empty rig graph.
            /// @param block_size Frames per process() call.
            /// @param max_live_buffers Maximum transient buffers live at once during a block.
            RigGraph(std::uint32_t block_size, std::uint32_t max_live_buffers);

            /// @brief Adds a node to the graph, unconnected. Not real-time safe; call only off the processing thread.
            /// @param node The node to take ownership of.
            /// @return A handle identifying the node for connect() calls.
            NodeHandle add_node(std::unique_ptr<AudioNode> node);

            /// @brief Connects one node's output port to another's input. Not real-time safe.
            /// @param from The upstream node.
            /// @param to The downstream node.
            /// @param from_port The upstream output port the edge reads; 0 is the node's main output.
            /// compile() refuses a port the upstream node does not declare.
            void connect(NodeHandle from, NodeHandle to, std::uint32_t from_port = 0);

            /// @brief Topologically sorts the graph and calls prepare() on every node. Not real-time safe; call after every add_node/connect.
            /// @param info Sample rate, block size, and channel count for every node.
            void compile(const PrepareInfo& info);

            /// @brief Removes a node and every edge touching it. Not real-time safe; call only off
            /// the processing thread. The handle becomes invalid; other handles are unaffected.
            /// @param handle The node to remove.
            void remove_node(NodeHandle handle);

            /// @brief Removes every edge from one node to another, whichever output ports they read.
            /// Not real-time safe.
            /// @param from The upstream node.
            /// @param to The downstream node.
            void disconnect(NodeHandle from, NodeHandle to);

            /// @brief Looks up a live node by handle.
            /// @param handle The node's handle.
            /// @return The node, or nullptr if handle is out of range or was removed.
            AudioNode* node(NodeHandle handle) const;

            /// @brief Lists every live (non-removed) node handle, in add_node() order.
            /// @return The live handles.
            std::vector<NodeHandle> nodes() const;

            /// @brief Reports whether compile() produced an empty schedule, i.e. the graph holds no
            /// live node and process() would write nothing. Real-time safe: reads one vector's size
            /// and allocates nothing.
            /// @return True when process() would write nothing.
            bool is_empty() const noexcept;

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

            /// @brief Longest path through the compiled graph, in base-rate samples: a node's latency
            /// plus the largest latency among its upstream paths, taken at the sink. Allocation-free;
            /// call after compile(), which caches the value.
            /// @return Total latency in base-rate samples.
            std::uint32_t latency_samples() const;

            /// @brief Reports the single-channel buffers the compiled graph needs live at once: one
            /// per node output channel, plus the reserve the edge adaptation borrows while gathering
            /// one node's inputs. compile() sizes buffer_pool_ from it; a host may read it to size
            /// its own pool. Valid only after a successful compile().
            /// @return The buffer count.
            std::uint32_t live_buffer_count() const;

        private:
            struct Edge
            {
                    NodeHandle from;
                    NodeHandle to;
                    /// @brief Holds the upstream output port the edge reads.
                    std::uint32_t from_port;
            };

            /// @brief The channel widths and input slot count RigGraph derives from a node's PortLayout.
            struct NodeChannels
            {
                    /// @brief Holds how many channels one input slot carries.
                    std::uint32_t input_width;

                    /// @brief Holds how many channels the node writes.
                    std::uint32_t output_width;

                    /// @brief Holds how many separate inputs the node reads.
                    std::uint32_t input_slots;
            };

            /// @brief Derives the widths and slot count RigGraph routes by from a node's PortLayout.
            /// @param layout The node's declared layout.
            /// @return The derived widths; see docs/archive/superpowers/specs/2026-09-07-stereo-tracks-design.md §3.
            static NodeChannels channels_of(const PortLayout& layout);

            std::uint32_t block_size_;
            std::uint32_t initial_max_live_buffers_;
            BufferPool buffer_pool_;
            std::vector<std::unique_ptr<AudioNode>> nodes_;
            std::vector<Edge> edges_;
            std::vector<NodeHandle> topological_order_;
            NodeHandle sink_handle_ = 0;
            std::uint32_t cached_latency_samples_ = 0;
            /// @brief Holds the widths and slot count derived from each node's PortLayout.
            std::vector<NodeChannels> node_channels_;

            /// @brief Holds the widest output_width in the compiled graph.
            std::uint32_t max_output_channels_ = 1;

            /// @brief Holds how many channels the host asks process() to fill.
            std::uint32_t host_output_channels_ = 1;

            /// @brief Holds the buffer count compile() sized the pool from.
            std::uint32_t live_buffer_count_ = 0;

            /// @brief Holds the output port count each node declared at compile(), indexed by handle.
            std::vector<std::uint32_t> port_count_;

            /// @brief Holds the channel width of each output port, indexed handle * K_MAX_OUTPUT_PORTS + port.
            std::vector<std::uint32_t> port_widths_;

            /// @brief Holds one buffer per node output channel, indexed
            /// (handle * K_MAX_OUTPUT_PORTS + port) * max_output_channels_ + channel.
            std::vector<float*> node_output_scratch_;

            /// @brief Holds the channel pointers of each port of the node process() is running.
            float* port_output_ptrs_[K_MAX_OUTPUT_PORTS][K_MAX_CHANNELS] = {};

            /// @brief Holds the table handed to the node as ProcessContext::port_outputs.
            float* const* port_output_table_[K_MAX_OUTPUT_PORTS] = {};

            /// @brief Holds the input pointers one node's gather hands to process().
            const float* gathered_inputs_[K_MAX_FAN_IN_OUT * K_MAX_CHANNELS] = {};

            /// @brief Holds the pool buffers the current gather borrowed, for release after the node runs.
            float* gather_scratch_[K_MAX_FAN_IN_OUT * K_MAX_CHANNELS] = {};
            SpscRing<ParameterChange> parameter_ring_{K_PARAMETER_RING_CAPACITY};
    };
} // namespace SushiDSP
