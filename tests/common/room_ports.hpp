/**************************************************************************/
/* room_ports.hpp                                                         */
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
 * @file room_ports.hpp
 * @brief The port table and the spread arrangement the room node's tests and benchmarks share.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

namespace SushiDSP::test
{
    /// @brief Holds one block of every output port of a RoomNode and the table that hands them to
    /// process(), as a RigGraph would. Port 0 is the listener's stereo pair; port 1 + m is
    /// microphone m.
    class RoomPorts
    {
        public:
            /// @brief Allocates one block per port channel and builds the table.
            /// @param block_frames The block length.
            explicit RoomPorts(std::uint32_t block_frames)
            {
                for (std::vector<float>& ear : listener_)
                {
                    ear.assign(block_frames, 0.0f);
                }
                listener_channels_ = {listener_[0].data(), listener_[1].data()};
                table_[SushiDSP::room::RoomNode::K_LISTENER_PORT] = listener_channels_.data();
                for (std::uint32_t index = 0; index < microphones_.size(); ++index)
                {
                    microphones_[index].assign(block_frames, 0.0f);
                    microphone_channels_[index] = microphones_[index].data();
                    table_[SushiDSP::room::RoomNode::microphone_port(index)] =
                        &microphone_channels_[index];
                }
            }

            RoomPorts(const RoomPorts&) = delete;
            RoomPorts& operator=(const RoomPorts&) = delete;

            /// @brief Builds the context one block of process() takes.
            /// @param inputs The node's input pointers, two per cabinet slot.
            /// @param frames The block length.
            /// @return The context, its outputs and its port table pointing at this object.
            SushiDSP::ProcessContext context(const float* const* inputs, std::uint32_t frames)
            {
                return SushiDSP::ProcessContext{listener_channels_.data(), inputs, frames, 0.0,
                                                table_.data()};
            }

            /// @brief Returns one ear of the listener's block.
            /// @param ear 0 for left, 1 for right.
            /// @return The block.
            const std::vector<float>& listener(std::uint32_t ear) const
            {
                return listener_[ear];
            }

            /// @brief Returns one microphone's block.
            /// @param index The microphone's index in the arrangement.
            /// @return The block.
            const std::vector<float>& microphone(std::uint32_t index) const
            {
                return microphones_[index];
            }

        private:
            /// @brief Stores the listener's two ears.
            std::array<std::vector<float>, 2> listener_;

            /// @brief Points at the listener's two ears.
            std::array<float*, 2> listener_channels_{};

            /// @brief Stores one block per microphone port.
            std::array<std::vector<float>, SushiDSP::room::RoomNode::K_MAX_MICROPHONES>
                microphones_;

            /// @brief Points at each microphone's block.
            std::array<float*, SushiDSP::room::RoomNode::K_MAX_MICROPHONES>
                microphone_channels_{};

            /// @brief Stores the table process() reads as ProcessContext::port_outputs.
            std::array<float* const*, SushiDSP::room::RoomNode::K_OUTPUT_PORTS> table_{};
    };

    /// @brief Places cabinets and microphones in a shell at the limits a room takes: spread
    /// across the floor at fractions of the shell's dimensions, so every pose is inside any valid
    /// shell. Cabinet s has identity 100 + s and microphone m identity 200 + m.
    /// @param shell The room's shell.
    /// @param sources The number of cabinets, at most Arrangement::K_MAX_SOURCES.
    /// @param microphones The number of microphones, at most Arrangement::K_MAX_MICROPHONES.
    /// @return The arrangement.
    inline SushiDSP::Room::Arrangement spread_arrangement(const SushiDSP::room::RoomScene& shell,
                                                          std::uint32_t sources,
                                                          std::uint32_t microphones)
    {
        const SushiDSP::room::Vec3 size = shell.dimensions;
        SushiDSP::Room::Arrangement arrangement;
        arrangement.set_shell(shell);
        arrangement.set_listener({{0.75 * size.x, 0.5 * size.y, 0.5 * size.z}, 3.14159265358979});
        std::string reason;
        for (std::uint32_t source = 0; source < sources; ++source)
        {
            const double across = (1.0 + static_cast<double>(source)) / 4.0;
            arrangement.add_source({100u + source, {{0.2 * size.x, across * size.y, 0.3}, 0.0}},
                                   reason);
        }
        for (std::uint32_t microphone = 0; microphone < microphones; ++microphone)
        {
            SushiDSP::Room::MicrophonePlacement placement;
            placement.id = 200u + microphone;
            const double across = (1.0 + static_cast<double>(microphone)) / 9.0;
            placement.pose = {{0.3 * size.x + 0.05 * microphone, across * size.y, 0.4 * size.z},
                              3.14159265358979};
            placement.pattern = microphone % 2 == 0 ? SushiDSP::room::PolarPattern::Cardioid
                                                    : SushiDSP::room::PolarPattern::Omni;
            arrangement.add_microphone(placement, reason);
        }
        return arrangement;
    }
} // namespace SushiDSP::test
