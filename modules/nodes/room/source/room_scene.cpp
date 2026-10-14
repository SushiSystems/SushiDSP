/**************************************************************************/
/* room_scene.cpp                                                         */
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
 * @file room_scene.cpp
 * @brief The shoebox's geometry, its area-weighted absorption and its pose validation.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/room_scene.hpp>

#include <algorithm>
#include <cmath>
#include <string>

#include <SushiDSP/nodes/room/built_room_loss.hpp>

namespace SushiDSP::room
{
    namespace
    {

        /// @brief Holds Sabine's constant 24 ln(10) / c at 20 degrees, in seconds per metre.
        constexpr double K_SABINE_CONSTANT = 0.161;

        /// @brief Names an axis for a validation reason.
        const char* axis_name(int axis) noexcept
        {
            constexpr std::array<const char*, 3> K_NAMES = {"x", "y", "z"};
            return K_NAMES[static_cast<std::size_t>(axis)];
        }

        /// @brief Checks one pose's three coordinates against the shoebox's bounds.
        /// @return true when the pose lies inside the room.
        bool pose_inside(const Vec3& position, const Vec3& dimensions, const char* who,
                         std::string& reason)
        {
            const std::array<double, 3> coordinate = {position.x, position.y, position.z};
            const std::array<double, 3> bound = {dimensions.x, dimensions.y, dimensions.z};
            for (int axis = 0; axis < 3; ++axis)
            {
                if (coordinate[static_cast<std::size_t>(axis)] < 0.0 ||
                    coordinate[static_cast<std::size_t>(axis)] >
                        bound[static_cast<std::size_t>(axis)])
                {
                    reason = std::string(who) + " " + axis_name(axis) + " is outside the room";
                    return false;
                }
            }
            return true;
        }

        /// @brief Checks the live furnishings' count, rows, quantities and places.
        /// @return true when every live furnishing is a known row with a positive quantity and,
        /// when it has a place, stands inside the room.
        bool furnishings_valid(const RoomScene& scene, std::string& reason)
        {
            if (scene.furnishing_count > RoomScene::K_MAX_FURNISHINGS)
            {
                reason =
                    "more than " + std::to_string(RoomScene::K_MAX_FURNISHINGS) + " furnishings";
                return false;
            }
            for (std::uint32_t index = 0; index < scene.furnishing_count; ++index)
            {
                const RoomScene::Furnishing& entry = scene.furnishings[index];
                if (static_cast<std::size_t>(entry.item) >= K_FURNITURE_COUNT)
                {
                    reason = "furnishing " + std::to_string(index) + " names no furniture row";
                    return false;
                }
                if (!std::isfinite(entry.quantity) || !(entry.quantity > 0.0))
                {
                    reason = "furnishing " + std::to_string(index) + " has a non-positive quantity";
                    return false;
                }
                const bool inside = std::isfinite(entry.x_m) && std::isfinite(entry.y_m) &&
                                    std::isfinite(entry.yaw_rad) && entry.x_m >= 0.0 &&
                                    entry.x_m <= scene.dimensions.x && entry.y_m >= 0.0 &&
                                    entry.y_m <= scene.dimensions.y;
                if (entry.placed && !inside)
                {
                    reason = "furnishing " + std::to_string(index) + " stands outside the room";
                    return false;
                }
            }
            return true;
        }

        /// @brief Returns the surfaces' mean absorption with furniture standing in the room, per
        /// band: 1 - (1 - a_s) exp(-A_f / S).
        std::array<double, K_BAND_COUNT> furnished_absorption(const RoomScene& scene)
        {
            const std::array<double, K_BAND_COUNT> surfaces = scene.mean_absorption();
            const std::array<double, K_BAND_COUNT> area = scene.furnishing_absorption_area();
            const double surface_area = scene.surface_area_m2();
            std::array<double, K_BAND_COUNT> result{};
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                result[band] = 1.0 - (1.0 - surfaces[band]) * std::exp(-area[band] / surface_area);
            }
            return result;
        }
    } // namespace

    double RoomScene::volume_m3() const noexcept
    {
        return dimensions.x * dimensions.y * dimensions.z;
    }

    double RoomScene::surface_area_m2() const noexcept
    {
        double total = 0.0;
        for (std::size_t surface = 0; surface < K_SURFACE_COUNT; ++surface)
        {
            total += face_area_m2(static_cast<Surface>(surface));
        }
        return total;
    }

    double RoomScene::face_area_m2(Surface surface) const noexcept
    {
        switch (surface)
        {
        case Surface::MinusX:
        case Surface::PlusX:
            return dimensions.y * dimensions.z;
        case Surface::MinusY:
        case Surface::PlusY:
            return dimensions.x * dimensions.z;
        case Surface::Floor:
        case Surface::Ceiling:
            return dimensions.x * dimensions.y;
        }
        return 0.0;
    }

    std::array<double, K_BAND_COUNT> RoomScene::face_absorption(Surface surface) const
    {
        // A patch replaces area on its face; see modules/nodes/room/README.md.
        const double face_area = face_area_m2(surface);
        std::array<double, K_BAND_COUNT> weighted{};
        double patch_area = 0.0;
        for (std::uint32_t index = 0; index < patch_count; ++index)
        {
            if (patches[index].surface != surface)
            {
                continue;
            }
            const Material& patch_material = material(patches[index].material);
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                weighted[band] +=
                    patches[index].area_m2 * static_cast<double>(patch_material.absorption[band]);
            }
            patch_area += patches[index].area_m2;
        }

        const Material& bare_material = material(surfaces[static_cast<std::size_t>(surface)]);
        std::array<double, K_BAND_COUNT> result{};
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            weighted[band] +=
                (face_area - patch_area) * static_cast<double>(bare_material.absorption[band]);
            result[band] = weighted[band] / face_area;
            if (built_room_loss)
            {
                result[band] = std::min(1.0, result[band] + K_BUILT_ROOM_LOSS[band]);
            }
        }
        return result;
    }

    std::array<double, K_BAND_COUNT> RoomScene::mean_absorption() const
    {
        std::array<double, K_BAND_COUNT> weighted{};
        double total_area = 0.0;
        for (std::size_t index = 0; index < K_SURFACE_COUNT; ++index)
        {
            const Surface surface = static_cast<Surface>(index);
            const double face_area = face_area_m2(surface);
            const std::array<double, K_BAND_COUNT> absorption = face_absorption(surface);
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                weighted[band] += face_area * absorption[band];
            }
            total_area += face_area;
        }

        std::array<double, K_BAND_COUNT> result{};
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            result[band] = weighted[band] / total_area;
        }
        return result;
    }

    std::array<double, K_BAND_COUNT> RoomScene::furnishing_absorption_area() const
    {
        const std::size_t live = std::min<std::size_t>(furnishing_count, K_MAX_FURNISHINGS);
        std::array<double, K_BAND_COUNT> area{};
        for (std::size_t index = 0; index < live; ++index)
        {
            const FurnitureItem& row = furniture(furnishings[index].item);
            for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
            {
                area[band] +=
                    furnishings[index].quantity * static_cast<double>(row.absorption_area[band]);
            }
        }
        return area;
    }

    double RoomScene::min_mean_absorption() const noexcept
    {
        // Eyring inverted for alpha at K_MAX_DECAY_SECONDS, the air term dropped; spec section 4.1.
        return 1.0 - std::exp(-K_SABINE_CONSTANT * volume_m3() /
                              (surface_area_m2() * K_MAX_DECAY_SECONDS));
    }

    bool RoomScene::is_valid(std::string& reason) const
    {
        const std::array<double, 3> length = {dimensions.x, dimensions.y, dimensions.z};
        for (int axis = 0; axis < 3; ++axis)
        {
            const double value = length[static_cast<std::size_t>(axis)];
            if (value < K_MIN_DIMENSION || value > K_MAX_DIMENSION)
            {
                reason = std::string("dimension ") + axis_name(axis) + " = " +
                         std::to_string(value) + " is outside [" + std::to_string(K_MIN_DIMENSION) +
                         ", " + std::to_string(K_MAX_DIMENSION) + "]";
                return false;
            }
        }

        if (!pose_inside(source_position, dimensions, "source", reason) ||
            !pose_inside(listener_position, dimensions, "listener", reason) ||
            !pose_inside(microphone_position, dimensions, "microphone", reason))
        {
            return false;
        }

        if (patch_count > K_MAX_PATCHES)
        {
            reason = "more than " + std::to_string(K_MAX_PATCHES) + " patches";
            return false;
        }
        std::array<double, K_SURFACE_COUNT> covered{};
        for (std::uint32_t index = 0; index < patch_count; ++index)
        {
            const Patch& patch = patches[index];
            const double face_area = face_area_m2(patch.surface);
            if (!std::isfinite(patch.area_m2) || !(patch.area_m2 > 0.0))
            {
                reason = "patch " + std::to_string(index) + " has a non-positive area";
                return false;
            }
            if (patch.area_m2 > face_area)
            {
                reason = "a patch of " + std::to_string(patch.area_m2) + " m2 exceeds its face's " +
                         std::to_string(face_area) + " m2";
                return false;
            }
            double& total = covered[static_cast<std::size_t>(patch.surface)];
            total += patch.area_m2;
            if (total > face_area)
            {
                reason = "the patches on one face total " + std::to_string(total) +
                         " m2, over its face's " + std::to_string(face_area) + " m2";
                return false;
            }
        }

        if (!furnishings_valid(*this, reason))
        {
            return false;
        }

        const double floor_absorption = min_mean_absorption();
        const std::array<double, K_BAND_COUNT> absorption =
            furnishing_count > 0 ? furnished_absorption(*this) : mean_absorption();
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            if (absorption[band] < floor_absorption)
            {
                reason = "mean absorption " + std::to_string(absorption[band]) + " at " +
                         std::to_string(static_cast<int>(K_BAND_CENTRES_HZ[band])) +
                         " Hz is under the " + std::to_string(floor_absorption) +
                         " this room needs to decay inside " +
                         std::to_string(static_cast<int>(K_MAX_DECAY_SECONDS)) + " s";
                return false;
            }
        }

        return true;
    }
} // namespace SushiDSP::room
