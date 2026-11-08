/**************************************************************************/
/* in_memory_nodes.hpp                                                    */
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
 * @file in_memory_nodes.hpp
 * @brief Builds the in-memory nodes the graph tests wire together, each named for what it does.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include <SushiDSP/foundation/core/in_memory_node.hpp>

namespace TestSupport
{
    /// @brief Returns a node of @p ports that writes @p gain times its input plus @p level on every channel.
    inline std::unique_ptr<SushiDSP::InMemoryNode> affine_node(SushiDSP::PortLayout ports, float gain, float level)
    {
        SushiDSP::InMemoryNodeShape shape;
        shape.ports = ports;
        SushiDSP::InMemoryNodeSignal signal;
        signal.gain = gain;
        signal.levels[0].fill(level);
        return std::make_unique<SushiDSP::InMemoryNode>(shape, signal);
    }

    /// @brief Returns a mono source that writes @p level.
    inline std::unique_ptr<SushiDSP::InMemoryNode> source_node(float level)
    {
        return affine_node({0, 1}, 1.0f, level);
    }

    /// @brief Returns a source of @p channels channels that writes silence.
    inline std::unique_ptr<SushiDSP::InMemoryNode> silent_source_node(std::uint32_t channels)
    {
        return affine_node({0, channels}, 1.0f, 0.0f);
    }

    /// @brief Returns a stereo source that writes @p left and @p right.
    inline std::unique_ptr<SushiDSP::InMemoryNode> stereo_source_node(float left, float right)
    {
        SushiDSP::InMemoryNodeShape shape;
        shape.ports = {0, 2};
        SushiDSP::InMemoryNodeSignal signal;
        signal.levels[0] = {left, right};
        return std::make_unique<SushiDSP::InMemoryNode>(shape, signal);
    }

    /// @brief Returns a mono node that writes @p gain times its input.
    inline std::unique_ptr<SushiDSP::InMemoryNode> gain_node(float gain)
    {
        return affine_node({1, 1}, gain, 0.0f);
    }

    /// @brief Returns a stereo node that writes @p gain times each input channel to the same channel.
    inline std::unique_ptr<SushiDSP::InMemoryNode> stereo_gain_node(float gain)
    {
        return affine_node({2, 2}, gain, 0.0f);
    }

    /// @brief Returns a stereo node that writes @p gain times each input channel to the same channel and
    /// records its input in @p record.
    inline std::unique_ptr<SushiDSP::InMemoryNode> stereo_gain_node(float gain, SushiDSP::InMemoryNodeRecord& record)
    {
        SushiDSP::InMemoryNodeShape shape;
        shape.ports = {2, 2};
        SushiDSP::InMemoryNodeSignal signal;
        signal.gain = gain;
        return std::make_unique<SushiDSP::InMemoryNode>(shape, signal, record);
    }

    /// @brief Returns a mono node that writes the sum of its @p inputs input channels.
    inline std::unique_ptr<SushiDSP::InMemoryNode> sum_node(std::uint32_t inputs)
    {
        return affine_node({inputs, 1}, 1.0f, 0.0f);
    }

    /// @brief Returns a mono pass-through that declares @p latency samples of latency.
    inline std::unique_ptr<SushiDSP::InMemoryNode> latency_node(std::uint32_t latency)
    {
        SushiDSP::InMemoryNodeShape shape;
        shape.latency_samples = latency;
        return std::make_unique<SushiDSP::InMemoryNode>(shape, SushiDSP::InMemoryNodeSignal{});
    }

    /// @brief Returns a mono node that writes silence and records its input in @p record.
    inline std::unique_ptr<SushiDSP::InMemoryNode> recording_sink_node(SushiDSP::InMemoryNodeRecord& record)
    {
        SushiDSP::InMemoryNodeSignal signal;
        signal.gain = 0.0f;
        return std::make_unique<SushiDSP::InMemoryNode>(SushiDSP::InMemoryNodeShape{}, signal, record);
    }

    /// @brief Returns a mono source at @p level that records what it is given in @p record.
    inline std::unique_ptr<SushiDSP::InMemoryNode> recording_source_node(float level,
                                                                         SushiDSP::InMemoryNodeRecord& record)
    {
        SushiDSP::InMemoryNodeShape shape;
        shape.ports = {0, 1};
        SushiDSP::InMemoryNodeSignal signal;
        signal.levels[0].fill(level);
        return std::make_unique<SushiDSP::InMemoryNode>(shape, signal, record);
    }

    /// @brief Returns a mono node of @p ports that declares the type id @p type_id.
    inline std::unique_ptr<SushiDSP::InMemoryNode> named_node(SushiDSP::PortLayout ports, const char* type_id)
    {
        SushiDSP::InMemoryNodeShape shape;
        shape.ports = ports;
        shape.type_id = type_id;
        SushiDSP::InMemoryNodeSignal signal;
        signal.levels[0].fill(ports.input_channels == 0 ? 1.0f : 0.0f);
        return std::make_unique<SushiDSP::InMemoryNode>(shape, signal);
    }

    /// @brief Returns a mono source that declares @p ports output ports of the widths @p widths, each at
    /// the levels @p levels; a width left out is zero.
    inline std::unique_ptr<SushiDSP::InMemoryNode> multi_port_source_node(
        std::uint32_t ports, const std::array<std::uint32_t, SushiDSP::K_IN_MEMORY_NODE_MAX_PORTS>& widths,
        const SushiDSP::InMemoryNodeSignal& levels)
    {
        SushiDSP::InMemoryNodeShape shape;
        shape.ports = {0, widths[0]};
        shape.output_port_count = ports;
        shape.port_channels = widths;
        return std::make_unique<SushiDSP::InMemoryNode>(shape, levels);
    }

    /// @brief Returns a mono source that declares @p ports output ports and leaves their widths to AudioNode.
    inline std::unique_ptr<SushiDSP::InMemoryNode> undeclared_width_source_node(std::uint32_t ports)
    {
        SushiDSP::InMemoryNodeShape shape;
        shape.ports = {0, 1};
        shape.output_port_count = ports;
        return std::make_unique<SushiDSP::InMemoryNode>(shape, SushiDSP::InMemoryNodeSignal{});
    }
} // namespace TestSupport
