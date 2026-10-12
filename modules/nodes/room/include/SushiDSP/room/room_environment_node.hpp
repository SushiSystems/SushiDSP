/**************************************************************************/
/* room_environment_node.hpp                                              */
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
 * @file room_environment_node.hpp
 * @brief Declares the room environment: several cabinets and microphones placed in one room.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "SushiDSP/core/audio_node.hpp"
#include "SushiDSP/room/microphone_model.hpp"
#include "SushiDSP/room/room_node.hpp"
#include "SushiDSP/room/room_scene.hpp"

namespace SushiDSP::Room
{
    /// @brief One room holding up to three cabinets and eight microphones, plus the listener.
    ///
    /// Each cabinet is one mono input slot. Every (cabinet, receiver) pair is one RoomNode, so the
    /// room's own solvers stay unchanged; the pair outputs are summed per receiver. Output ports are
    /// all mono: port 0 is the listener's left ear, port 1 its right ear, and port 2 + m is
    /// microphone m. A port with no receiver behind it carries silence.
    ///
    /// The layout is fixed for the life of the node: every pair exists from construction, and
    /// configure() only chooses which pairs are active and where they stand. Moving a receiver is
    /// therefore a control-thread solve, not a graph rebuild. Cost grows with active pairs.
    class RoomEnvironmentNode final : public AudioNode
    {
        public:
            /// @brief The most cabinets the room takes; the three-way splitter's cabinet count.
            static constexpr std::uint32_t K_MAX_SOURCES = 3;

            /// @brief The most microphones the room takes; the graph's fan-in limit.
            static constexpr std::uint32_t K_MAX_MICROPHONES = 8;

            /// @brief The output ports that carry the listener's two ears.
            static constexpr std::uint32_t K_LISTENER_PORTS = 2;

            /// @brief The output ports the node declares: the two ears, then one per microphone slot.
            static constexpr std::uint32_t K_OUTPUT_PORTS = K_LISTENER_PORTS + K_MAX_MICROPHONES;

            /// @brief A point and a facing in the room.
            struct Placement
            {
                    /// @brief The position, in metres, inside the shoebox.
                    room::Vec3 position{0.0, 0.0, 0.0};

                    /// @brief The yaw in radians, zero facing +x.
                    double yaw_rad = 0.0;
            };

            /// @brief A microphone's placement and its polar pattern.
            struct Microphone
            {
                    /// @brief Where the microphone stands and faces.
                    Placement place;

                    /// @brief The pattern the room's microphone model applies.
                    room::PolarPattern pattern = room::PolarPattern::Cardioid;
            };

            /// @brief Constructs the environment with one RoomNode per (cabinet, receiver) pair, none active.
            RoomEnvironmentNode();

            /// @brief Sizes every pair and the scratch block, then applies the configuration the
            /// node already holds. Not real-time safe.
            /// @param info Sample rate and block size.
            void prepare(const PrepareInfo& info) override;

            /// @brief Sums every active pair's render into its receiver's port. Real-time safe.
            /// @param context Needs port_outputs; a node with output ports runs under RigGraph only.
            void process(ProcessContext& context) override;

            /// @brief Returns K_MAX_SOURCES mono inputs and one mono output channel per port.
            /// @return The port layout.
            PortLayout ports() const override;

            /// @brief Returns K_OUTPUT_PORTS.
            /// @return The number of output ports.
            std::uint32_t output_port_count() const override;

            /// @brief Returns one channel for every port.
            /// @param port The port index.
            /// @return 1.
            std::uint32_t output_port_channels(std::uint32_t port) const override;

            /// @brief Clears every pair's state.
            void reset() override;

            /// @brief The room has no parameters of its own; the rooms behind it do.
            /// @param id Ignored.
            /// @return 0.
            float get_parameter(const char* id) const override;

            /// @brief Ignored; the room is placed through configure().
            /// @param id Ignored.
            /// @param value Ignored.
            void set_parameter_for_control_thread(const char* id, float value) override;

            /// @brief Ignored; the room is placed through configure().
            /// @param index Ignored.
            /// @param value Ignored.
            void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

            /// @brief The largest latency any active pair reports, in samples.
            /// @return The latency.
            std::uint32_t latency_samples() const override;

            /// @brief This node's registered type id.
            /// @return "sushidsp.room_environment".
            const char* type_id() const override;

            /// @brief Places the cabinets, the listener and the microphones in the shell's room.
            ///
            /// Control thread only. Every pair is validated before any pair is changed, so a refusal
            /// leaves the previous placement standing.
            /// @param shell The room's shell, its materials and furnishings; its source, listener and
            /// microphone poses are replaced for each pair.
            /// @param sources The cabinets' placements, at most K_MAX_SOURCES.
            /// @param listener The listener's placement.
            /// @param microphones The microphones' placements, at most K_MAX_MICROPHONES.
            /// @param reason_out Holds the reason when the configuration is refused.
            /// @return False when a count is out of range or a pair's scene is refused.
            bool configure(const room::RoomScene& shell, const std::vector<Placement>& sources,
                           const Placement& listener, const std::vector<Microphone>& microphones,
                           std::string& reason_out);

        private:
            /// @brief One (cabinet, receiver) pair's room and the scene it stands in.
            struct Pair
            {
                    /// @brief The room that renders this pair.
                    std::unique_ptr<room::RoomNode> node;

                    /// @brief The scene last configured for this pair.
                    room::RoomScene scene;

                    /// @brief True when the receiver is the listener's ears rather than a microphone.
                    bool ears = false;
            };

            /// @brief The number of receivers behind one cabinet: the listener and the microphones.
            static constexpr std::uint32_t K_RECEIVERS = 1 + K_MAX_MICROPHONES;

            /// @brief Holds every pair, indexed [cabinet][receiver]; receiver 0 is the listener.
            std::array<std::array<Pair, K_RECEIVERS>, K_MAX_SOURCES> pairs_;

            /// @brief Holds how many cabinets are active; written by configure(), read by process().
            std::atomic<std::uint32_t> active_sources_{0};

            /// @brief Holds how many microphones are active; written by configure(), read by process().
            std::atomic<std::uint32_t> active_microphones_{0};

            /// @brief Holds the stereo scratch one pair renders into before it is summed.
            std::vector<float> scratch_left_;

            /// @brief Holds the right half of the stereo scratch.
            std::vector<float> scratch_right_;

            /// @brief Holds the sample rate and block size prepare() was given.
            PrepareInfo prepared_info_{48000.0, 0, 1};

            /// @brief True once prepare() has run.
            bool prepared_ = false;

            /// @brief Applies one pair's scene and listener kind; control thread.
            /// @return False and fills reason_out when the room refuses the scene.
            static bool apply_pair(Pair& pair, std::string& reason_out);
    };

    /// @brief The room environment's registration entry point.
    /// @return The static NodeDescriptor for "sushidsp.room_environment".
    const NodeDescriptor& room_environment_descriptor();
} // namespace SushiDSP::Room
