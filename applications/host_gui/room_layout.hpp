/**************************************************************************/
/* room_layout.hpp                                                        */
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
 * @file room_layout.hpp
 * @brief Declares the host's record of the cabinet section: the room, the listener, the cabinets'
 * places and the microphones.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "SushiDSP/room/microphone_model.hpp"
#include "SushiDSP/room/room_scene.hpp"

/// @brief Where a point stands in the room and which way it faces.
struct RoomPlacement
{
        /// @brief The position in metres, inside the shoebox.
        SushiDSP::room::Vec3 position{0.0, 0.0, 0.0};

        /// @brief The yaw in radians, zero facing +x.
        double yaw_rad = 0.0;
};

/// @brief One microphone in the room: its place, its pattern and whether it is an overhead.
struct MicrophoneSpot
{
        /// @brief The host's id for the microphone; unique within one layout.
        std::uint32_t id = 0;

        /// @brief Where the microphone stands and faces.
        RoomPlacement place;

        /// @brief The polar pattern the room's microphone model applies.
        SushiDSP::room::PolarPattern pattern = SushiDSP::room::PolarPattern::Cardioid;

        /// @brief True for an overhead microphone, which the mixer groups on the Room strip.
        bool room_mic = false;
};

/// @brief The host's record of the cabinet section. Pure data and checks; no node is touched here.
///
/// Cabinets are sources by path: source k is the last slot of path k, so the layout names a place
/// per path, not per cabinet slot. Microphones are keyed by the id the layout hands out. The shell
/// holds the room's materials and furnishings; each placement replaces only its own pose.
class RoomLayout
{
    public:
        /// @brief The most cabinets the room takes, the three-way splitter's path count.
        static constexpr std::size_t K_MAX_CABINETS = 3;

        /// @brief The most microphones the room takes. A room's microphones and its listener share
        /// the bus's eight inputs, with two taken by the listener's ears, so six.
        static constexpr std::size_t K_MAX_MICROPHONES = 6;

        /// @brief Starts with the starter room, the listener at the room's default pose and no
        /// microphones.
        RoomLayout();

        /// @brief Reads the room's shell: materials, furnishings and patches.
        /// @return The shell scene.
        const SushiDSP::room::RoomScene& shell() const noexcept;

        /// @brief Replaces the shell. Its poses are kept per placement, not taken from the shell.
        /// @param shell The new shell.
        void set_shell(const SushiDSP::room::RoomScene& shell);

        /// @brief Reads the listener's place.
        /// @return The listener's placement.
        const RoomPlacement& listener() const noexcept;

        /// @brief Moves the listener.
        /// @param place The new placement.
        void set_listener(const RoomPlacement& place);

        /// @brief Reads the cabinets' places, one per path that holds a cabinet.
        /// @return The places, index k naming path k.
        const std::vector<RoomPlacement>& cabinets() const noexcept;

        /// @brief Places the cabinet of one path.
        /// @param index The path, below K_MAX_CABINETS.
        /// @param place The placement.
        /// @param reason_out Holds the reason when the placement is refused.
        /// @return False when the index is out of range or the cabinet is not placed yet and the
        /// index is more than one past the last placed cabinet.
        bool set_cabinet(std::size_t index, const RoomPlacement& place, std::string& reason_out);

        /// @brief Reads the microphones in the order they were added.
        /// @return The microphones.
        const std::vector<MicrophoneSpot>& microphones() const noexcept;

        /// @brief Adds a microphone and gives it the next id.
        /// @param spot The microphone; its id is replaced.
        /// @param reason_out Holds the reason when the room is full.
        /// @return The id the microphone was given, or 0 when refused.
        std::uint32_t add_microphone(MicrophoneSpot spot, std::string& reason_out);

        /// @brief Removes a microphone.
        /// @param id The microphone's id.
        /// @return False when no microphone has that id.
        bool remove_microphone(std::uint32_t id);

        /// @brief Replaces a microphone's placement, pattern and room flag.
        /// @param spot The microphone, carrying the id to replace.
        /// @return False when no microphone has that id.
        bool update_microphone(const MicrophoneSpot& spot);

        /// @brief Looks up one microphone.
        /// @param id The microphone's id.
        /// @return The microphone, or nullptr when no microphone has that id.
        const MicrophoneSpot* find_microphone(std::uint32_t id) const noexcept;

        /// @brief Builds the room scene one (cabinet, receiver) pair stands in: the shell with the
        /// cabinet's and the receiver's poses. Used by the room environment's configuration.
        /// @param cabinet_index The cabinet's path.
        /// @param receiver_index 0 for the listener, 1 + m for microphone m.
        /// @return The scene for that pair.
        SushiDSP::room::RoomScene pair_scene(std::size_t cabinet_index,
                                             std::size_t receiver_index) const;

    private:
        /// @brief Holds the shell scene.
        SushiDSP::room::RoomScene shell_;

        /// @brief Holds the listener's place.
        RoomPlacement listener_;

        /// @brief Holds the cabinets' places, one per placed path.
        std::vector<RoomPlacement> cabinets_;

        /// @brief Holds the microphones, in the order they were added.
        std::vector<MicrophoneSpot> microphones_;

        /// @brief Holds the id the next microphone receives.
        std::uint32_t next_id_ = 1;
};
