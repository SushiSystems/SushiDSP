/**************************************************************************/
/* room_arrangement.hpp                                                   */
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
 * @file room_arrangement.hpp
 * @brief Declares the arrangement of one room: its shell, its listener, its cabinets and its microphones.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "SushiDSP/room/microphone_model.hpp"
#include "SushiDSP/room/room_scene.hpp"

namespace SushiDSP::Room
{
    /// @brief Holds where a point stands in the room and which way it faces.
    struct Pose
    {
            /// @brief Stores the position in metres, inside the shoebox.
            room::Vec3 position{0.0, 0.0, 0.0};

            /// @brief Stores the yaw in radians, zero facing +x.
            double yaw_rad = 0.0;
    };

    /// @brief Holds one cabinet in the room, keyed by the identity of the rig element behind it.
    struct SourcePlacement
    {
            /// @brief Stores the host's identity for the cabinet; unique among the sources.
            std::uint64_t id = 0;

            /// @brief Stores where the baffle's centre stands and which way it faces.
            Pose pose;

            /// @brief Stores the cabinet's box and cones.
            room::CabinetGeometry cabinet = room::K_CABINET4X12;
    };

    /// @brief Holds one microphone in the room, keyed by the identity of the rig element behind it,
    /// with the gear data the room's solve reads.
    struct MicrophonePlacement
    {
            /// @brief Stores the host's identity for the microphone; unique among the microphones.
            std::uint64_t id = 0;

            /// @brief Stores where the microphone stands and faces.
            Pose pose;

            /// @brief Stores the capsule's polar pattern.
            room::PolarPattern pattern = room::PolarPattern::Cardioid;

            /// @brief Records that the capsule's gradient share takes the proximity effect.
            bool proximity = true;

            /// @brief Names the shipped microphone the capsule is.
            room::MicrophoneKind kind = room::MicrophoneKind::FlatCondenser;
    };

    /// @brief Holds everything placed in one room: the shell, the listener, the cabinets and the
    /// microphones. Pure data with its limits checked on every edit; control thread.
    ///
    /// A source's index is the room node's input slot, and a microphone's index plus one is its
    /// output port, so the order of each list is part of the arrangement.
    class Arrangement
    {
        public:
            /// @brief Bounds the cabinets one room takes.
            static constexpr std::size_t K_MAX_SOURCES = 3;

            /// @brief Bounds the microphones one room takes.
            static constexpr std::size_t K_MAX_MICROPHONES = 8;

            /// @brief Counts the receivers behind one cabinet at the limits: the listener and the
            /// microphones.
            static constexpr std::size_t K_MAX_RECEIVERS = 1 + K_MAX_MICROPHONES;

            /// @brief Starts with the starter room, the listener at its starter pose, no cabinet
            /// and no microphone.
            Arrangement();

            /// @brief Builds the bare room a single scene describes: its shell, its listener and
            /// its cabinet as source 0 under identity 0. The scene's microphone is not taken.
            /// @param scene The scene to read.
            /// @return The arrangement.
            static Arrangement from_scene(const room::RoomScene& scene);

            /// @brief Reads the shell: dimensions, materials, patches and furnishings. Its pose
            /// fields and its cabinet geometry are not the arrangement's.
            /// @return The shell scene.
            const room::RoomScene& shell() const noexcept;

            /// @brief Replaces the shell. The poses the scene carries are ignored.
            /// @param shell The new shell.
            void set_shell(const room::RoomScene& shell);

            /// @brief Reads the listener's pose.
            /// @return The pose.
            const Pose& listener() const noexcept;

            /// @brief Moves the listener.
            /// @param pose The new pose.
            void set_listener(const Pose& pose) noexcept;

            /// @brief Reads the cabinets in input-slot order.
            /// @return The sources.
            const std::vector<SourcePlacement>& sources() const noexcept;

            /// @brief Adds a cabinet at the end of the list.
            /// @param source The cabinet with its identity and pose.
            /// @param reason_out Holds the reason when the cabinet is refused.
            /// @return False when the room is full or the identity is taken.
            bool add_source(const SourcePlacement& source, std::string& reason_out);

            /// @brief Moves a cabinet.
            /// @param id The cabinet's identity.
            /// @param pose The new pose.
            /// @return False when no cabinet has that identity.
            bool move_source(std::uint64_t id, const Pose& pose) noexcept;

            /// @brief Replaces a cabinet's box and cones.
            /// @param id The cabinet's identity.
            /// @param cabinet The new geometry.
            /// @return False when no cabinet has that identity.
            bool set_source_cabinet(std::uint64_t id, const room::CabinetGeometry& cabinet) noexcept;

            /// @brief Removes a cabinet; the cabinets after it move one slot down.
            /// @param id The cabinet's identity.
            /// @return False when no cabinet has that identity.
            bool remove_source(std::uint64_t id) noexcept;

            /// @brief Looks a cabinet up.
            /// @param id The cabinet's identity.
            /// @return The cabinet, or nullptr when no cabinet has that identity.
            const SourcePlacement* find_source(std::uint64_t id) const noexcept;

            /// @brief Reads the microphones in port order.
            /// @return The microphones.
            const std::vector<MicrophonePlacement>& microphones() const noexcept;

            /// @brief Adds a microphone at the end of the list.
            /// @param microphone The microphone with its identity, pose and gear data.
            /// @param reason_out Holds the reason when the microphone is refused.
            /// @return False when the room is full or the identity is taken.
            bool add_microphone(const MicrophonePlacement& microphone, std::string& reason_out);

            /// @brief Replaces a microphone's pose and gear data.
            /// @param microphone The microphone, carrying the identity to replace.
            /// @return False when no microphone has that identity.
            bool update_microphone(const MicrophonePlacement& microphone) noexcept;

            /// @brief Removes a microphone; the microphones after it move one port down.
            /// @param id The microphone's identity.
            /// @return False when no microphone has that identity.
            bool remove_microphone(std::uint64_t id) noexcept;

            /// @brief Looks a microphone up.
            /// @param id The microphone's identity.
            /// @return The microphone, or nullptr when no microphone has that identity.
            const MicrophonePlacement* find_microphone(std::uint64_t id) const noexcept;

            /// @brief Replaces an identity wherever a cabinet or a microphone carries it, as a host
            /// does when it rebuilds the rig element behind it.
            /// @param from The identity to replace.
            /// @param to The identity that takes its place.
            void rename(std::uint64_t from, std::uint64_t to) noexcept;

            /// @brief Builds the scene one (cabinet, receiver) pair stands in: the shell with the
            /// cabinet's pose and geometry and the receiver's pose.
            /// @param source The cabinet's index, below sources().size().
            /// @param receiver 0 for the listener, 1 + m for microphone m.
            /// @return The scene for that pair.
            room::RoomScene pair_scene(std::size_t source, std::size_t receiver) const;

            /// @brief Checks the shell and every pair's scene against the room's bounds. A room
            /// with no cabinet checks the shell with the listener alone.
            /// @param reason_out Holds the first violation, naming the cabinet and the receiver.
            /// @return False when any scene is refused.
            bool is_valid(std::string& reason_out) const;

        private:
            /// @brief Stores the shell scene.
            room::RoomScene shell_;

            /// @brief Stores the listener's pose.
            Pose listener_;

            /// @brief Stores the cabinets in input-slot order.
            std::vector<SourcePlacement> sources_;

            /// @brief Stores the microphones in port order.
            std::vector<MicrophonePlacement> microphones_;
    };

    /// @brief Reports whether two scenes describe the same room and the same cabinet geometry,
    /// whatever stands where: every field a pose-free design reads.
    /// @param left One scene.
    /// @param right The other scene.
    /// @return True when the two rooms are equal.
    bool shells_equal(const room::RoomScene& left, const room::RoomScene& right) noexcept;

    /// @brief Reports whether two scenes describe the same room and the same poses, field by field.
    /// @param left One scene.
    /// @param right The other scene.
    /// @return True when every field a solve reads is equal.
    bool scenes_equal(const room::RoomScene& left, const room::RoomScene& right) noexcept;
} // namespace SushiDSP::Room
