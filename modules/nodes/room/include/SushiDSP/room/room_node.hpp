/**************************************************************************/
/* room_node.hpp                                                          */
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
 * @file room_node.hpp
 * @brief The room node: up to three cabinets, the listener and eight microphones in one shoebox.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "SushiDSP/core/audio_node.hpp"
#include "SushiDSP/core/node_descriptor.hpp"
#include "SushiDSP/core/parameter_table.hpp"
#include "SushiDSP/core/rt_swap_slot.hpp"
#include "SushiDSP/room/early_tap_set.hpp"
#include "SushiDSP/room/environment_render_set.hpp"
#include "SushiDSP/room/fade_window.hpp"
#include "SushiDSP/room/hrtf_set.hpp"
#include "SushiDSP/room/modal_design.hpp"
#include "SushiDSP/room/pair_stage.hpp"
#include "SushiDSP/room/receiver_output.hpp"
#include "SushiDSP/room/room_arrangement.hpp"
#include "SushiDSP/room/room_host.hpp"
#include "SushiDSP/room/room_render_set.hpp"
#include "SushiDSP/room/room_scene.hpp"
#include "SushiDSP/room/source_stage.hpp"

namespace SushiDSP::room
{
    /// @brief One room that holds up to three cabinets, the listener and up to eight microphones.
    /// Each cabinet is one input slot; output port 0 is the listener's two ears and port 1 + m is
    /// microphone m, mono. A port with no receiver behind it carries silence.
    /// @see modules/nodes/room/README.md, "The room as an environment"
    class RoomNode final : public AudioNode, public RoomHost
    {
        public:
            /// @brief Counts the frames a newly acquired set fades in over, raised cosine.
            static constexpr std::uint32_t K_CROSSFADE_FRAMES = room::K_CROSSFADE_FRAMES;

            /// @brief Names the crossfade window every stage blends under.
            using FadeWindow = room::FadeWindow;

            /// @brief Bounds the cabinets, the node's input slots.
            static constexpr std::uint32_t K_MAX_SOURCES =
                static_cast<std::uint32_t>(Room::Arrangement::K_MAX_SOURCES);

            /// @brief Bounds the microphones.
            static constexpr std::uint32_t K_MAX_MICROPHONES =
                static_cast<std::uint32_t>(Room::Arrangement::K_MAX_MICROPHONES);

            /// @brief Names the output port that carries the listener's two ears.
            static constexpr std::uint32_t K_LISTENER_PORT = 0;

            /// @brief Counts the output ports: the listener's and one per microphone.
            static constexpr std::uint32_t K_OUTPUT_PORTS = 1 + K_MAX_MICROPHONES;

            /// @brief Counts the channels of one input slot. A slot is as wide as the main output;
            /// the room reads its first channel.
            static constexpr std::uint32_t K_SLOT_CHANNELS = 2;

            /// @brief Returns the output port of one microphone.
            /// @param microphone The microphone's index in the arrangement.
            /// @return 1 + microphone.
            static constexpr std::uint32_t microphone_port(std::uint32_t microphone) noexcept
            {
                return 1u + microphone;
            }

            /// @brief Puts every parameter at its descriptor default and the starter room in
            /// place, with one cabinet under identity 0 and no microphone.
            RoomNode();

            /// @brief Frees the stages.
            ~RoomNode() override;

            /// @brief Sizes every stage, builds the spherical-head fallback, publishes its decoder
            /// and a first set for the arrangement in place.
            /// @param info Sample rate and block size.
            void prepare(const PrepareInfo& info) override;

            /// @brief Renders one block: one acquire per slot, each cabinet's stage, each pair,
            /// and each receiver's sum into its port. A context with no port table, a node run
            /// outside a RigGraph, renders the listener alone into outputs.
            /// @param context The block's buffers; inputs[K_SLOT_CHANNELS * s] is cabinet s.
            void process(ProcessContext& context) override;

            /// @brief Returns K_MAX_SOURCES input slots of two channels and two output channels.
            /// @return The port layout.
            PortLayout ports() const override;

            /// @brief Returns K_OUTPUT_PORTS.
            /// @return The number of output ports.
            std::uint32_t output_port_count() const override;

            /// @brief Returns two channels for the listener's port and one for a microphone's.
            /// @param port The port index.
            /// @return The width, or 0 for a port the node does not declare.
            std::uint32_t output_port_channels(std::uint32_t port) const override;

            /// @brief Reads a parameter by id; an unknown id reads 0.
            /// @param id "direct_delay".
            /// @return The value.
            float get_parameter(const char* id) const override;

            /// @brief Writes a parameter by id. Control thread only. A changed direct delay
            /// rebuilds the set; the audio thread applies it through the next published set.
            /// @param id "direct_delay".
            /// @param value 0 compensated, 1 physical.
            void set_parameter_for_control_thread(const char* id, float value) override;

            /// @brief Writes a parameter by descriptor index; stores a target and nothing else, so
            /// a direct delay written here waits for the next publish.
            /// @param index The parameter's index in the descriptor table.
            /// @param value The value.
            void set_parameter_for_audio_thread(std::uint16_t index, float value) override;

            /// @brief This node's parameters, one row each, feeding the descriptor and both
            /// dispatches.
            /// @return The table, built once on first use.
            static const ParameterTable<RoomNode, 1>& parameters();

            /// @brief Silences every stage and the crossfade.
            void reset() override;

            /// @brief Returns the decoder's partition, the delay every port carries, plus the
            /// first cabinet's propagation delay to the listener when direct_delay is physical.
            /// @return The latency in samples.
            std::uint32_t latency_samples() const override;

            /// @brief Returns "sushidsp.room".
            /// @return The type id.
            const char* type_id() const override;

            bool set_arrangement(const Room::Arrangement& arrangement,
                                 std::string& reason_out) override;
            Room::Arrangement arrangement() const override;
            bool set_scene(const RoomScene& scene, std::string& reason_out) override;
            RoomScene scene() const override;
            bool set_hrtf(HrtfSet set) override;
            HrtfSource hrtf_source() const override;
            double schroeder_frequency_hz() const override;
            void reclaim_rooms() override;

            /// @brief Copies a set's taps into a bank, as a pair stage does it. Audio thread.
            /// @pre The taps are sorted by arrival, so tap 0 is the direct image whenever count > 0.
            /// @return True when direct was written.
            static bool take_direct_image(const RoomRenderSet& set, EarlyTapSet& reflections,
                                          EarlyTap& direct) noexcept;

            /// @brief Moves an ears set's whole render earlier by its direct propagation delay; a
            /// microphone set is left as it was. Control thread. Spec §9.4.
            /// @param set The set to shift in place.
            static void compensate_direct_delay(RoomRenderSet& set) noexcept;

            /// @brief Returns true when two designs hold the same resonators, as a source stage
            /// judges it.
            static bool same_resonators(const ModalDesign& retiring,
                                        const ModalDesign& arriving) noexcept;

        private:
            /// @brief Holds every stage; allocated once, so the node itself stays small.
            struct Stages;

            /// @brief Reports whether the direct_delay parameter names physical.
            bool direct_delay_physical() const noexcept;

            /// @brief Builds a set for the arrangement, HRTF and direct delay in place, publishes
            /// it and records the frequency the set splits at.
            /// @return False and fills reason_out when the build refuses a pair.
            bool publish(std::string& reason_out);

            /// @brief Hands a newly acquired set to every stage it names and starts the fade; the
            /// first set ever is taken in from zero state with no fade.
            void adopt(const Room::EnvironmentRenderSet& set) noexcept;

            /// @brief Writes silence to every port the context carries.
            void write_silence(ProcessContext& context, std::uint32_t frames) noexcept;

            /// @brief Stores the rate prepare() ran at, in hertz.
            double sample_rate_ = 48000.0;

            /// @brief Bounds the block length prepare() sized the buffers for.
            std::uint32_t max_block_ = 0;

            /// @brief Stores the decoder's partition, the frames every port is held back.
            std::uint32_t alignment_frames_ = 0;

            /// @brief Records that prepare() has run.
            bool prepared_ = false;

            /// @brief Stores the arrangement in place; control thread.
            Room::Arrangement arrangement_;

            /// @brief Stores the frequency the published set splits at, in hertz: the solver's
            /// crossover while the modal layer runs, Eyring's Schroeder frequency otherwise.
            double schroeder_hz_ = 0.0;

            /// @brief Stores the direct-delay choice, 0 compensated and 1 physical; written on
            /// either thread, read at publish.
            std::atomic<float> direct_delay_{0.0f};

            /// @brief Records that the set published last was built physical; control thread.
            bool published_physical_ = false;

            /// @brief Stores the propagation delay the set published last adds to the latency in
            /// frames: the first cabinet's to the listener when physical, zero otherwise.
            std::uint32_t published_direct_delay_frames_ = 0;

            /// @brief Builds each set and remembers what its pairs were built from.
            Room::EnvironmentBuilder builder_;

            /// @brief Carries each published set to the audio thread.
            RtSwapSlot<Room::EnvironmentRenderSet> slot_;

            /// @brief Identifies the set adopted last; compared, never dereferenced past its block.
            const Room::EnvironmentRenderSet* adopted_ = nullptr;

            /// @brief Counts the cabinets of the set adopted last.
            std::uint32_t source_count_ = 0;

            /// @brief Counts the microphones of the set adopted last.
            std::uint32_t microphone_count_ = 0;

            /// @brief Owns the head the listener hears through.
            Room::HeadHolder head_;

            /// @brief Holds the source, pair and output stages.
            std::unique_ptr<Stages> stages_;

            /// @brief Counts the crossfade frames left; zero when no fade runs.
            std::uint32_t fade_remaining_ = 0;

            /// @brief Counts the frames the running fade spans: K_CROSSFADE_FRAMES plus its lag.
            std::uint32_t fade_total_ = 0;

            /// @brief Stores the lag the running fade is read at by the paths that blend at the
            /// output, the alignment delay.
            std::uint32_t fade_lag_ = 0;

            /// @brief Stores one block of a microphone's port when the context carries no table.
            std::vector<float> scratch_;
    };

    /// @brief Returns the registration entry for "sushidsp.room".
    const NodeDescriptor& room_descriptor();
} // namespace SushiDSP::room
