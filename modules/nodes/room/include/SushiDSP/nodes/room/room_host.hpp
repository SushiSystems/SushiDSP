/**************************************************************************/
/* room_host.hpp                                                          */
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
 * @file room_host.hpp
 * @brief What a host calls to install an arrangement, a scene and an HRTF set into a room node.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>
#include <string>

#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

namespace SushiDSP::room
{
    /// @brief Names where the room's HRTF came from, so a host can say so without reading a
    /// log. Spec §9.2.
    enum class HrtfSource : std::uint8_t
    {
        SphericalHead = 0,
        Measured
    };

    /// @brief A node that renders a room and takes its arrangement and its HRTF from a host. A
    /// host discovers it with dynamic_cast<RoomHost*> on an AudioNode, as it does with
    /// CabinetHost, so no host names a concrete room type. Spec §5.2 and §9.2.
    struct RoomHost
    {
            virtual ~RoomHost() = default;

            /// @brief Solves every pair of the arrangement and publishes one render set for all of
            /// them. Control thread only.
            /// @param arrangement The shell, the listener, the cabinets and the microphones.
            /// @param reason_out Holds the reason when the arrangement is refused.
            /// @return False when the arrangement is refused; nothing is published and the
            /// previous arrangement stays in place.
            virtual bool set_arrangement(const Room::Arrangement& arrangement,
                                         std::string& reason_out) = 0;

            /// @brief Returns the arrangement in place.
            /// @return The arrangement.
            virtual Room::Arrangement arrangement() const = 0;

            /// @brief Replaces the shell, the listener's pose and the first cabinet's pose from one
            /// scene, the bare room's whole description, and publishes. A room with no cabinet
            /// gains one under identity 0. Microphones are left as they are. Control thread only.
            /// @return False and fills reason_out when the scene is refused; nothing is published
            /// and the previous arrangement stays in place.
            virtual bool set_scene(const RoomScene& scene, std::string& reason_out) = 0;

            /// @brief Returns the shell with the listener's pose and the first cabinet's.
            virtual RoomScene scene() const = 0;

            /// @brief Installs a measured set and republishes. An empty set restores the
            /// spherical-head fallback. Control thread only.
            /// @return False when the set is refused and the previous set stays in place.
            virtual bool set_hrtf(HrtfSet set) = 0;

            /// @brief Returns where the set in place came from.
            virtual HrtfSource hrtf_source() const = 0;

            /// @brief Returns the scene's Schroeder frequency in hertz, for a panel to show beside
            /// the room.
            virtual double schroeder_frequency_hz() const = 0;

            /// @brief Frees every render set the audio thread has provably moved past.
            /// @pre Control thread, never concurrently with process(); once per GUI frame.
            virtual void reclaim_rooms() = 0;
    };
} // namespace SushiDSP::room
