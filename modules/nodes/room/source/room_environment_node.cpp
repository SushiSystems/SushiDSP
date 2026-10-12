/**************************************************************************/
/* room_environment_node.cpp                                              */
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
 * @file room_environment_node.cpp
 * @brief Implements the room environment that places several cabinets and microphones in one room.
 * @author Mustafa Garip
 */

#include "SushiDSP/room/room_environment_node.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>

namespace SushiDSP::Room
{
    RoomEnvironmentNode::RoomEnvironmentNode()
    {
        for (auto& row : pairs_)
        {
            for (Pair& pair : row)
            {
                pair.node = std::make_unique<room::RoomNode>();
                pair.scene = room::RoomScene{};
            }
        }
    }

    void RoomEnvironmentNode::prepare(const PrepareInfo& info)
    {
        prepared_info_ = info;
        scratch_left_.assign(info.max_block, 0.0f);
        scratch_right_.assign(info.max_block, 0.0f);

        const PrepareInfo pair_info{info.sample_rate, info.max_block, 2};
        for (auto& row : pairs_)
        {
            for (Pair& pair : row)
            {
                pair.node->prepare(pair_info);
            }
        }

        // The configuration the node already holds is published now that every pair exists. The
        // scenes were validated by configure(), so a refusal here cannot occur; the reason is unused.
        std::string reason;
        const std::uint32_t sources = active_sources_.load(std::memory_order_relaxed);
        const std::uint32_t microphones = active_microphones_.load(std::memory_order_relaxed);
        for (std::uint32_t s = 0; s < sources; ++s)
        {
            for (std::uint32_t r = 0; r <= microphones; ++r)
            {
                apply_pair(pairs_[s][r], reason);
            }
        }
        prepared_ = true;
    }

    void RoomEnvironmentNode::process(ProcessContext& context)
    {
        if (context.port_outputs == nullptr || !prepared_)
        {
            return;
        }

        for (std::uint32_t port = 0; port < K_OUTPUT_PORTS; ++port)
        {
            float* out = context.port_outputs[port][0];
            std::fill(out, out + context.frames, 0.0f);
        }

        const std::uint32_t sources = active_sources_.load(std::memory_order_relaxed);
        const std::uint32_t microphones = active_microphones_.load(std::memory_order_relaxed);
        float* stereo[2] = {scratch_left_.data(), scratch_right_.data()};

        for (std::uint32_t s = 0; s < sources; ++s)
        {
            const float* cabinet = context.inputs != nullptr ? context.inputs[s] : nullptr;
            if (cabinet == nullptr)
            {
                continue;
            }
            const float* pair_inputs[1] = {cabinet};

            for (std::uint32_t r = 0; r <= microphones; ++r)
            {
                ProcessContext pair_context{stereo, pair_inputs, context.frames,
                                            context.time_seconds, nullptr};
                pairs_[s][r].node->process(pair_context);

                if (r == 0)
                {
                    float* left = context.port_outputs[0][0];
                    float* right = context.port_outputs[1][0];
                    for (std::uint32_t i = 0; i < context.frames; ++i)
                    {
                        left[i] += scratch_left_[i];
                        right[i] += scratch_right_[i];
                    }
                }
                else
                {
                    float* mic = context.port_outputs[K_LISTENER_PORTS + r - 1][0];
                    for (std::uint32_t i = 0; i < context.frames; ++i)
                    {
                        mic[i] += scratch_left_[i];
                    }
                }
            }
        }
    }

    PortLayout RoomEnvironmentNode::ports() const
    {
        return {K_MAX_SOURCES, 1};
    }

    std::uint32_t RoomEnvironmentNode::output_port_count() const
    {
        return K_OUTPUT_PORTS;
    }

    std::uint32_t RoomEnvironmentNode::output_port_channels(std::uint32_t port) const
    {
        return port < K_OUTPUT_PORTS ? 1u : 0u;
    }

    void RoomEnvironmentNode::reset()
    {
        for (auto& row : pairs_)
        {
            for (Pair& pair : row)
            {
                pair.node->reset();
            }
        }
    }

    float RoomEnvironmentNode::get_parameter(const char*) const
    {
        return 0.0f;
    }

    void RoomEnvironmentNode::set_parameter_for_control_thread(const char*, float) {}

    void RoomEnvironmentNode::set_parameter_for_audio_thread(std::uint16_t, float) {}

    std::uint32_t RoomEnvironmentNode::latency_samples() const
    {
        std::uint32_t latency = 0;
        const std::uint32_t sources = active_sources_.load(std::memory_order_relaxed);
        const std::uint32_t microphones = active_microphones_.load(std::memory_order_relaxed);
        for (std::uint32_t s = 0; s < sources; ++s)
        {
            for (std::uint32_t r = 0; r <= microphones; ++r)
            {
                latency = std::max(latency, pairs_[s][r].node->latency_samples());
            }
        }
        return latency;
    }

    const char* RoomEnvironmentNode::type_id() const
    {
        return "sushidsp.room_environment";
    }

    bool RoomEnvironmentNode::configure(const room::RoomScene& shell,
                                        const std::vector<Placement>& sources,
                                        const Placement& listener,
                                        const std::vector<Microphone>& microphones,
                                        std::string& reason_out)
    {
        if (sources.size() > K_MAX_SOURCES)
        {
            reason_out = "a room takes at most " + std::to_string(K_MAX_SOURCES) + " cabinets";
            return false;
        }
        if (microphones.size() > K_MAX_MICROPHONES)
        {
            reason_out =
                "a room takes at most " + std::to_string(K_MAX_MICROPHONES) + " microphones";
            return false;
        }

        // Every pair's scene is built and validated before any pair is changed, so a refusal
        // leaves the previous placement standing.
        const std::size_t receivers = 1 + microphones.size();
        std::vector<room::RoomScene> staged(sources.size() * receivers, shell);
        for (std::size_t s = 0; s < sources.size(); ++s)
        {
            for (std::size_t r = 0; r < receivers; ++r)
            {
                room::RoomScene& scene = staged[s * receivers + r];
                scene.source_position = sources[s].position;
                scene.source_yaw_rad = sources[s].yaw_rad;
                if (r == 0)
                {
                    scene.listener_position = listener.position;
                    scene.listener_yaw_rad = listener.yaw_rad;
                }
                else
                {
                    const Microphone& microphone = microphones[r - 1];
                    scene.microphone_position = microphone.place.position;
                    scene.microphone_yaw_rad = microphone.place.yaw_rad;
                    scene.microphone_pattern = microphone.pattern;
                }

                std::string why;
                if (!scene.is_valid(why))
                {
                    reason_out = "cabinet " + std::to_string(s) + ", " +
                                 (r == 0 ? std::string("listener")
                                         : "microphone " + std::to_string(r - 1)) +
                                 ": " + why;
                    return false;
                }
            }
        }

        for (std::size_t s = 0; s < sources.size(); ++s)
        {
            for (std::size_t r = 0; r < receivers; ++r)
            {
                Pair& pair = pairs_[s][r];
                pair.scene = staged[s * receivers + r];
                pair.ears = r == 0;
                if (prepared_)
                {
                    if (!apply_pair(pair, reason_out))
                    {
                        return false;
                    }
                }
            }
        }

        active_sources_.store(static_cast<std::uint32_t>(sources.size()),
                              std::memory_order_relaxed);
        active_microphones_.store(static_cast<std::uint32_t>(microphones.size()),
                                  std::memory_order_relaxed);
        return true;
    }

    bool RoomEnvironmentNode::apply_pair(Pair& pair, std::string& reason_out)
    {
        if (!pair.node->set_scene(pair.scene, reason_out))
        {
            return false;
        }
        const float wanted = pair.ears ? 0.0f : 1.0f;
        if (pair.node->get_parameter("listener") != wanted)
        {
            pair.node->set_parameter_for_control_thread("listener", wanted);
        }
        return true;
    }

    namespace
    {
        std::unique_ptr<AudioNode> make_room_environment(const PrepareInfo&)
        {
            return std::make_unique<RoomEnvironmentNode>();
        }
    } // namespace

    const NodeDescriptor& room_environment_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.room_environment",
            "Room Environment",
            nullptr,
            0,
            PortLayout{RoomEnvironmentNode::K_MAX_SOURCES, 1},
            &make_room_environment,
        };
        return descriptor;
    }
} // namespace SushiDSP::Room
