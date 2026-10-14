/**************************************************************************/
/* driver_field.cpp                                                       */
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
 * @file driver_field.cpp
 * @brief Lays the cones out as elements and evaluates each one's path to a receiver.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/driver_field.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <SushiDSP/nodes/room/cabinet_directivity.hpp>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double K_PI = 3.14159265358979323846;

        /// @brief Counts the elements on a cone's ring.
        constexpr std::uint32_t K_RING_ELEMENTS = DriverField::K_CONE_ELEMENTS - 1u;
    } // namespace

    DriverField::DriverField(const CabinetGeometry& geometry) : geometry_(geometry)
    {
        const CabinetDirectivity directivity(geometry);
        rear_ = directivity.rear_response();
        const double cones = static_cast<double>(std::max(geometry.driver_count(), 1u));
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            const double centre = centre_share(K_BAND_CENTRES_HZ[band]);
            centre_share_.gain[band] = static_cast<float>(centre / cones);
            ring_share_.gain[band] =
                static_cast<float>((1.0 - centre) / (static_cast<double>(K_RING_ELEMENTS) * cones));
        }
    }

    double DriverField::centre_share(double frequency_hz) const noexcept
    {
        // The centre disc holds one element's part of the whole cone's area.
        const double disc_radius =
            geometry_.cone_radius_m / std::sqrt(static_cast<double>(K_CONE_ELEMENTS));
        const double effective = CabinetDirectivity(geometry_).effective_radius(frequency_hz);
        if (!(effective > disc_radius))
        {
            return 1.0;
        }
        const double ratio = disc_radius / effective;
        return ratio * ratio;
    }

    std::uint32_t DriverField::element_count() const noexcept
    {
        return geometry_.driver_count() * K_CONE_ELEMENTS;
    }

    Vec3 DriverField::element_position(const Vec3& source, double yaw_rad,
                                       std::uint32_t element) const noexcept
    {
        const std::uint32_t driver = element / K_CONE_ELEMENTS;
        const std::uint32_t point = element % K_CONE_ELEMENTS;
        double left = geometry_.driver_left_m(driver);
        double up = geometry_.driver_up_m(driver);
        if (point > 0u)
        {
            const double angle =
                2.0 * K_PI * static_cast<double>(point - 1u) / static_cast<double>(K_RING_ELEMENTS);
            const double radius = K_RING_RADIUS_RATIO * geometry_.cone_radius_m;
            left += radius * std::cos(angle);
            up += radius * std::sin(angle);
        }
        // The cabinet's left is its forward turned a quarter turn counter-clockwise.
        return Vec3{source.x - std::sin(yaw_rad) * left, source.y + std::cos(yaw_rad) * left,
                    source.z + up};
    }

    BandResponse DriverField::element_share(std::uint32_t element) const noexcept
    {
        return element % K_CONE_ELEMENTS == 0u ? centre_share_ : ring_share_;
    }

    DriverField::Field DriverField::toward(const Vec3& source, double yaw_rad,
                                           const Vec3& receiver) const noexcept
    {
        Field field{};
        field.count = element_count();
        const double forward_x = std::cos(yaw_rad);
        const double forward_y = std::sin(yaw_rad);
        for (std::uint32_t element = 0; element < field.count; ++element)
        {
            const Vec3 position = element_position(source, yaw_rad, element);
            const double dx = receiver.x - position.x;
            const double dy = receiver.y - position.y;
            const double dz = receiver.z - position.z;
            const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
            Arrival& arrival = field.arrivals[element];
            arrival.distance_m = std::max(length, K_MIN_DISTANCE_M);
            arrival.direction = length > 0.0 ? Vec3{dx / length, dy / length, dz / length}
                                             : Vec3{forward_x, forward_y, 0.0};
            arrival.gain = element_share(element);
            if (dx * forward_x + dy * forward_y < 0.0)
            {
                for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
                {
                    arrival.gain.gain[band] *= rear_.gain[band];
                }
            }
        }
        std::stable_sort(field.arrivals.begin(),
                         field.arrivals.begin() + static_cast<std::ptrdiff_t>(field.count),
                         [](const Arrival& a, const Arrival& b) noexcept
                         {
                             return a.distance_m < b.distance_m;
                         });
        return field;
    }
} // namespace SushiDSP::room
