/**************************************************************************/
/* driver_field.hpp                                                       */
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
 * @file driver_field.hpp
 * @brief Declares the cabinet's cones as radiating elements and what reaches a point from them.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>

#include "SushiDSP/room/band_response.hpp"
#include "SushiDSP/room/cabinet_geometry.hpp"
#include "SushiDSP/room/room_scene.hpp"

namespace SushiDSP::room
{
    /// @brief Breaks a cabinet's cones into radiating elements and returns what each sends along
    /// the line of sight to a point, so a receiver near the baffle hears cones and not a point.
    /// Control thread.
    /// @see modules/nodes/room/README.md, "The cones in the near field"
    class DriverField
    {
        public:
            /// @brief Counts the elements of one cone: its centre and six points on a ring.
            static constexpr std::uint32_t K_CONE_ELEMENTS = 7;

            /// @brief Bounds the elements of one cabinet.
            static constexpr std::uint32_t K_MAX_ELEMENTS =
                CabinetGeometry::K_MAX_DRIVERS * K_CONE_ELEMENTS;

            /// @brief Holds the ring's radius over the cone's, the centroid of the annulus the six
            /// ring elements stand for.
            static constexpr double K_RING_RADIUS_RATIO = 0.72;

            /// @brief Bounds the distance an element is heard from, in metres, from below.
            static constexpr double K_MIN_DISTANCE_M = 0.02;

            /// @brief Holds what one element sends to the receiver.
            struct Arrival
            {
                    /// @brief Stores the path length, in metres, never under K_MIN_DISTANCE_M.
                    double distance_m;

                    /// @brief Stores the unit direction from the element to the receiver, in room
                    /// space.
                    Vec3 direction;

                    /// @brief Stores the element's share of the cabinet's pressure per band, the
                    /// rear shelf included behind the baffle; 1 / r is not included.
                    BandResponse gain;
            };

            /// @brief Holds every element's arrival at one receiver, nearest first.
            struct Field
            {
                    /// @brief Stores the arrivals; entries past count are never read.
                    std::array<Arrival, K_MAX_ELEMENTS> arrivals;

                    /// @brief Counts the arrivals in use.
                    std::uint32_t count;
            };

            /// @brief Builds the elements of one geometry.
            explicit DriverField(const CabinetGeometry& geometry);

            /// @brief Returns the number of elements: K_CONE_ELEMENTS per cone.
            std::uint32_t element_count() const noexcept;

            /// @brief Returns where one element stands in the room.
            /// @param source The baffle's centre.
            /// @param yaw_rad The cabinet's yaw, zero facing +x.
            /// @param element The element, below element_count().
            Vec3 element_position(const Vec3& source, double yaw_rad,
                                  std::uint32_t element) const noexcept;

            /// @brief Returns one element's share of the cabinet's pressure per band. The shares
            /// of a cabinet sum to one in every band.
            /// @param element The element, below element_count().
            BandResponse element_share(std::uint32_t element) const noexcept;

            /// @brief Returns every element's arrival at a receiver, nearest first.
            /// @param source The baffle's centre.
            /// @param yaw_rad The cabinet's yaw, zero facing +x.
            /// @param receiver The point the paths end at.
            Field toward(const Vec3& source, double yaw_rad, const Vec3& receiver) const noexcept;

        private:
            /// @brief Returns the share of one cone's pressure its centre element carries at one
            /// frequency: the centre disc's part of the effective piston's area.
            double centre_share(double frequency_hz) const noexcept;

            /// @brief Stores the geometry the elements are laid out on.
            CabinetGeometry geometry_;

            /// @brief Stores a centre element's share per band, the cone count divided out.
            BandResponse centre_share_{};

            /// @brief Stores one ring element's share per band, the cone count divided out.
            BandResponse ring_share_{};

            /// @brief Stores the gain behind the baffle plane per band.
            BandResponse rear_{};
    };
} // namespace SushiDSP::room
