/****************************************************************/
/* IRoomHost.hpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file IRoomHost.hpp
 * @brief What a host calls to install a scene and an HRTF set into a room node.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>
#include <string>

#include "SushiDSP/room/HrtfSet.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace SushiDSP::room
{
    /// @brief Names where the room's HRTF came from, so a host can say so without reading a
    /// log. Spec §9.2.
    enum class HrtfSource : std::uint8_t
    {
        SphericalHead = 0,
        Measured
    };

    /// @brief A node that renders a room and takes its scene and its HRTF from a host. A host
    /// discovers it with dynamic_cast<IRoomHost*> on an IAudioNode, as it does with
    /// ICabinetHost, so no host names a concrete room type. Spec §5.2 and §9.2.
    struct IRoomHost
    {
        virtual ~IRoomHost() = default;

        /// @brief Solves the scene and publishes a new render set. Control thread only.
        /// @return False and fills reason_out when the scene is refused; nothing is published
        /// and the previous scene stays in place.
        virtual bool set_scene(const RoomScene& scene, std::string& reason_out) = 0;

        /// @brief Returns the scene in place.
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
}
