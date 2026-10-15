/**************************************************************************/
/* benchmark_room_node.cpp                                                */
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
 * @file benchmark_room_node.cpp
 * @brief Times RoomNode's blocks alone, at its limits and behind JCM800 chains.
 * @author Mustafa Garip
 */

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/node_benchmark.hpp>
#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/nodes/jcm800/jcm800.hpp>
#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>
#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>
#include <SushiDSP/nodes/room_scenes/room_ports.hpp>

#include "garage_benchmark_scene.hpp"

namespace
{
    using SushiDSP::bench::K_BLOCK_SIZE;
    using SushiDSP::bench::K_SAMPLE_RATE;

    /// @brief Holds the gate of the cabinet section: three cabinets and eight microphones behind
    /// three JCM800 chains stay under half of the 5.33 ms block.
    constexpr double K_CABINET_SECTION_GATE_MS = 2.67;

    /// @brief Returns a prepared RoomNode rendering the garage with one cabinet and the listener.
    /// @param with_microphone True also places the scene's microphone, so the node renders two
    /// receivers.
    std::unique_ptr<SushiDSP::room::RoomNode> prepared_room_node(bool with_microphone)
    {
        auto node = std::make_unique<SushiDSP::room::RoomNode>();
        node->prepare({K_SAMPLE_RATE, K_BLOCK_SIZE, 2});
        const SushiDSP::room::RoomScene scene = SushiDSP::test::garage_benchmark_scene();
        SushiDSP::Room::Arrangement arrangement = SushiDSP::Room::Arrangement::from_scene(scene);
        std::string reason;
        if (with_microphone)
        {
            SushiDSP::Room::MicrophonePlacement microphone;
            microphone.id = 1;
            microphone.pose = {scene.microphone_position, scene.microphone_yaw_rad};
            microphone.pattern = scene.microphone_pattern;
            arrangement.add_microphone(microphone, reason);
        }
        if (!node->set_arrangement(arrangement, reason))
        {
            std::printf("RoomNode set_arrangement failed: %s\n", reason.c_str());
        }
        return node;
    }

    /// @brief Times the steady-state ears block: the garage, no publish in flight, every layer
    /// running. Plan Task 29 step 1; spec section 12 item 10 budgets 1.8 ms.
    SushiDSP::bench::Result measure_room_ears_steady_state()
    {
        auto node = prepared_room_node(false);
        const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[2] = {left.data(), right.data()};
        return SushiDSP::bench::measure("RoomNode ears (steady state)", K_BLOCK_SIZE, K_SAMPLE_RATE,
                                        [&]
                                        {
                                            SushiDSP::ProcessContext context{outputs, inputs,
                                                                             K_BLOCK_SIZE, 0.0};
                                            node->process(context);
                                        });
    }

    /// @brief Times the ears block while a publish lands every other block, so the 512-frame
    /// raised-cosine fade never lapses and every rendered block runs both early-reflection
    /// banks. Plan Task 29 step 2; spec section 12 item 10 budgets 2.8 ms.
    SushiDSP::bench::Result measure_room_ears_swap()
    {
        auto node = prepared_room_node(false);
        const SushiDSP::room::RoomScene base = SushiDSP::test::garage_benchmark_scene();
        SushiDSP::room::RoomScene alternate = base;
        alternate.source_position.x += 0.05;
        const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[2] = {left.data(), right.data()};
        bool alternate_next = true;
        std::string reason;
        return SushiDSP::bench::measure(
            "RoomNode ears (swap every block)", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                node->set_scene(alternate_next ? alternate : base, reason);
                alternate_next = !alternate_next;
                SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK_SIZE, 0.0};
                node->process(context);
                node->reclaim_rooms();
            });
    }

    /// @brief Times the steady-state block of one cabinet heard by the listener and one
    /// microphone.
    SushiDSP::bench::Result measure_room_microphone_steady_state()
    {
        auto node = prepared_room_node(true);
        const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[2] = {left.data(), right.data()};
        return SushiDSP::bench::measure(
            "RoomNode ears + 1 microphone (steady state)", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK_SIZE, 0.0};
                node->process(context);
            });
    }

    /// @brief One AudioNode root that plays the reference tone, so a RigGraph chain has a
    /// signal to carry without a host-supplied input buffer.
    class ToneSourceNode final : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo& info) override
            {
                tone_ = SushiDSP::bench::reference_tone(info.max_block);
            }

            void process(SushiDSP::ProcessContext& context) override
            {
                std::memcpy(context.outputs[0], tone_.data(), context.frames * sizeof(float));
            }

            SushiDSP::PortLayout ports() const override
            {
                return {0, 1};
            }

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            void set_parameter_for_control_thread(const char*, float) override
            {

            }

            void reset() override
            {

            }

        private:
            std::vector<float> tone_;
    };

    /// @brief Times the room alone at its limits: three cabinets, the listener and eight
    /// microphones, every port rendered.
    SushiDSP::bench::Result measure_room_at_the_limits()
    {
        auto node = std::make_unique<SushiDSP::room::RoomNode>();
        node->prepare({K_SAMPLE_RATE, K_BLOCK_SIZE, 2});
        std::string reason;
        if (!node->set_arrangement(
                SushiDSP::test::spread_arrangement(SushiDSP::test::garage_benchmark_scene(), 3, 8),
                reason))
        {
            std::printf("RoomNode set_arrangement failed: %s\n", reason.c_str());
        }
        const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
        const float* inputs[6] = {input.data(), input.data(), input.data(),
                                  input.data(), input.data(), input.data()};
        SushiDSP::test::RoomPorts ports(K_BLOCK_SIZE);
        return SushiDSP::bench::measure("RoomNode 3 cabinets, ears + 8 microphones", K_BLOCK_SIZE,
                                        K_SAMPLE_RATE,
                                        [&]
                                        {
                                            SushiDSP::ProcessContext context =
                                                ports.context(inputs, K_BLOCK_SIZE);
                                            node->process(context);
                                        });
    }

    /// @brief Times the cabinet section's gate: three JCM800 chains, each into its own input of
    /// one room that holds eight microphones and the listener.
    SushiDSP::bench::Result measure_cabinet_section_gate()
    {
        SushiDSP::RigGraph graph(K_BLOCK_SIZE, 64);
        auto room_handle = graph.add_node(std::make_unique<SushiDSP::room::RoomNode>());
        for (int path = 0; path < 3; ++path)
        {
            auto source_handle = graph.add_node(std::make_unique<ToneSourceNode>());
            auto amp_handle = graph.add_node(std::make_unique<SushiDSP::Jcm800>());
            graph.connect(source_handle, amp_handle);
            graph.connect(amp_handle, room_handle);
        }
        graph.compile({K_SAMPLE_RATE, K_BLOCK_SIZE, 2});

        auto* room = static_cast<SushiDSP::room::RoomNode*>(graph.node(room_handle));
        std::string reason;
        if (!room->set_arrangement(
                SushiDSP::test::spread_arrangement(SushiDSP::test::garage_benchmark_scene(), 3, 8),
                reason))
        {
            std::printf("RoomNode set_arrangement failed: %s\n", reason.c_str());
        }

        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        float* outputs[2] = {left.data(), right.data()};
        return SushiDSP::bench::measure(
            "3 x JCM800 + RoomNode (3 cabinets, 8 microphones)", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                SushiDSP::ProcessContext context{outputs, nullptr, K_BLOCK_SIZE, 0.0};
                graph.process(context);
            });
    }

    /// @brief Sets the JCM800 knobs the regression snapshots use.
    void set_reference_jcm800_knobs(SushiDSP::Jcm800& node)
    {
        node.set_preamp(0.6f);
        node.set_master(0.0f);
        node.set_bass(0.6f);
        node.set_middle(0.4f);
        node.set_treble(0.7f);
        node.set_presence(0.5f);
    }

    /// @brief Times the room's own share of one block in a JCM800-into-room `RigGraph`, the
    /// shape DE7's decision is made against. Plan Task 29 step 5; over 2.0 ms opens DE7.
    SushiDSP::bench::Result measure_room_beside_jcm800()
    {
        SushiDSP::RigGraph graph(K_BLOCK_SIZE, 8);
        auto source_handle = graph.add_node(std::make_unique<ToneSourceNode>());
        auto amp_handle = graph.add_node(std::make_unique<SushiDSP::Jcm800>());
        auto room_handle = graph.add_node(std::make_unique<SushiDSP::room::RoomNode>());
        graph.connect(source_handle, amp_handle);
        graph.connect(amp_handle, room_handle);
        graph.compile({K_SAMPLE_RATE, K_BLOCK_SIZE, 2});

        set_reference_jcm800_knobs(*static_cast<SushiDSP::Jcm800*>(graph.node(amp_handle)));

        auto* room = static_cast<SushiDSP::room::RoomNode*>(graph.node(room_handle));
        std::string reason;
        if (!room->set_scene(SushiDSP::test::garage_benchmark_scene(), reason))
        {
            std::printf("RoomNode set_scene failed: %s\n", reason.c_str());
        }

        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        float* outputs[2] = {left.data(), right.data()};

        const SushiDSP::bench::Result whole_chain = SushiDSP::bench::measure(
            "JCM800 + RoomNode (whole chain)", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                SushiDSP::ProcessContext context{outputs, nullptr, K_BLOCK_SIZE, 0.0};
                graph.process(context);
            });
        SushiDSP::bench::print(whole_chain);

        const SushiDSP::bench::Result amp_alone = SushiDSP::bench::measure_node<SushiDSP::Jcm800>(
            "Jcm800 alone (for subtraction)", set_reference_jcm800_knobs);

        // The room's share is the chain less the amplifier alone; see tests/nodes/room/README.md.
        const double room_share_ns =
            whole_chain.nanoseconds_per_sample - amp_alone.nanoseconds_per_sample;
        return {"RoomNode share of JCM800 + Room", room_share_ns,
                100.0 * room_share_ns / (1e9 / K_SAMPLE_RATE)};
    }

    const SushiDSP::bench::BenchmarkRegistration K_ROOM_NODE_CASE{
        "RoomNode",
        []
        {
            SushiDSP::bench::print(measure_room_ears_steady_state(), 1.8);
            SushiDSP::bench::print(measure_room_ears_swap(), 2.8);
            SushiDSP::bench::print(measure_room_microphone_steady_state());
            SushiDSP::bench::print(measure_room_beside_jcm800(), 2.0);
            SushiDSP::bench::print(measure_room_at_the_limits());
            SushiDSP::bench::print(measure_cabinet_section_gate(), K_CABINET_SECTION_GATE_MS);
        }};
} // namespace
