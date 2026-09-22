/**************************************************************************/
/* RoomScene.cpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file RoomScene.cpp
 * @brief The shoebox's geometry, its area-weighted absorption and its pose validation.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/RoomScene.hpp"

#include <algorithm>
#include <cmath>

namespace SushiDSP::room
{
    namespace
    {

        /// @brief Holds Sabine's constant 24 ln(10) / c at 20 degrees, in seconds per metre.
        constexpr double kSabineConstant = 0.161;

        /// @brief Names an axis for a validation reason.
        const char* axis_name(int axis) noexcept
        {
            constexpr std::array<const char*, 3> kNames = { "x", "y", "z" };
            return kNames[static_cast<std::size_t>(axis)];
        }

        /// @brief Checks one pose's three coordinates against the shoebox's bounds.
        /// @return true when the pose lies inside the room.
        bool pose_inside(const Vec3& position, const Vec3& dimensions, const char* who,
                         std::string& reason)
        {
            const std::array<double, 3> coordinate = { position.x, position.y, position.z };
            const std::array<double, 3> bound = { dimensions.x, dimensions.y, dimensions.z };
            for (int axis = 0; axis < 3; ++axis)
            {
                if (coordinate[static_cast<std::size_t>(axis)] < 0.0
                    || coordinate[static_cast<std::size_t>(axis)] > bound[static_cast<std::size_t>(axis)])
                {
                    reason = std::string(who) + " " + axis_name(axis) + " is outside the room";
                    return false;
                }
            }
            return true;
        }

        /// @brief Checks the live furnishings' count, rows and quantities.
        /// @return true when every live furnishing is a known row with a positive quantity.
        bool furnishings_valid(const RoomScene& scene, std::string& reason)
        {
            if (scene.furnishing_count > RoomScene::kMaxFurnishings)
            {
                reason = "more than " + std::to_string(RoomScene::kMaxFurnishings) + " furnishings";
                return false;
            }
            for (std::uint32_t index = 0; index < scene.furnishing_count; ++index)
            {
                const RoomScene::Furnishing& entry = scene.furnishings[index];
                if (static_cast<std::size_t>(entry.item) >= kFurnitureCount)
                {
                    reason = "furnishing " + std::to_string(index) + " names no furniture row";
                    return false;
                }
                if (!std::isfinite(entry.quantity) || !(entry.quantity > 0.0))
                {
                    reason = "furnishing " + std::to_string(index)
                        + " has a non-positive quantity";
                    return false;
                }
            }
            return true;
        }

        /// @brief Returns the surfaces' mean absorption with furniture standing in the room, per
        /// band: 1 - (1 - a_s) exp(-A_f / S).
        std::array<double, kBandCount> furnished_absorption(const RoomScene& scene)
        {
            const std::array<double, kBandCount> surfaces = scene.mean_absorption();
            const std::array<double, kBandCount> area = scene.furnishing_absorption_area();
            const double surface_area = scene.surface_area_m2();
            std::array<double, kBandCount> result{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                result[band] = 1.0 - (1.0 - surfaces[band]) * std::exp(-area[band] / surface_area);
            }
            return result;
        }
    }

    double RoomScene::volume_m3() const noexcept
    {
        return dimensions.x * dimensions.y * dimensions.z;
    }

    double RoomScene::surface_area_m2() const noexcept
    {
        double total = 0.0;
        for (std::size_t surface = 0; surface < kSurfaceCount; ++surface)
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

    std::array<double, kBandCount> RoomScene::face_absorption(Surface surface) const
    {
        // A patch replaces area on its face rather than adding to it, so its own material's
        // area is subtracted from the face before either is weighted.
        const double face_area = face_area_m2(surface);
        std::array<double, kBandCount> weighted{};
        double patch_area = 0.0;
        for (std::uint32_t index = 0; index < patch_count; ++index)
        {
            if (patches[index].surface != surface)
            {
                continue;
            }
            const Material& patch_material = material(patches[index].material);
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                weighted[band] += patches[index].area_m2
                    * static_cast<double>(patch_material.absorption[band]);
            }
            patch_area += patches[index].area_m2;
        }

        const Material& bare_material = material(surfaces[static_cast<std::size_t>(surface)]);
        std::array<double, kBandCount> result{};
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            weighted[band] += (face_area - patch_area)
                * static_cast<double>(bare_material.absorption[band]);
            result[band] = weighted[band] / face_area;
        }
        return result;
    }

    std::array<double, kBandCount> RoomScene::mean_absorption() const
    {
        std::array<double, kBandCount> weighted{};
        double total_area = 0.0;
        for (std::size_t index = 0; index < kSurfaceCount; ++index)
        {
            const Surface surface = static_cast<Surface>(index);
            const double face_area = face_area_m2(surface);
            const std::array<double, kBandCount> absorption = face_absorption(surface);
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                weighted[band] += face_area * absorption[band];
            }
            total_area += face_area;
        }

        std::array<double, kBandCount> result{};
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            result[band] = weighted[band] / total_area;
        }
        return result;
    }

    std::array<double, kBandCount> RoomScene::furnishing_absorption_area() const
    {
        const std::size_t live = std::min<std::size_t>(furnishing_count, kMaxFurnishings);
        std::array<double, kBandCount> area{};
        for (std::size_t index = 0; index < live; ++index)
        {
            const FurnitureItem& row = furniture(furnishings[index].item);
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                area[band] += furnishings[index].quantity
                    * static_cast<double>(row.absorption_area[band]);
            }
        }
        return area;
    }

    double RoomScene::min_mean_absorption() const noexcept
    {
        // Eyring inverted for alpha at kMaxDecaySeconds, the air term dropped; spec section 4.1.
        return 1.0
            - std::exp(-kSabineConstant * volume_m3()
                       / (surface_area_m2() * kMaxDecaySeconds));
    }

    bool RoomScene::is_valid(std::string& reason) const
    {
        const std::array<double, 3> length = { dimensions.x, dimensions.y, dimensions.z };
        for (int axis = 0; axis < 3; ++axis)
        {
            const double value = length[static_cast<std::size_t>(axis)];
            if (value < kMinDimension || value > kMaxDimension)
            {
                reason = std::string("dimension ") + axis_name(axis) + " = " + std::to_string(value)
                    + " is outside [" + std::to_string(kMinDimension) + ", "
                    + std::to_string(kMaxDimension) + "]";
                return false;
            }
        }

        if (!pose_inside(source_position, dimensions, "source", reason)
            || !pose_inside(listener_position, dimensions, "listener", reason)
            || !pose_inside(microphone_position, dimensions, "microphone", reason))
        {
            return false;
        }

        if (patch_count > kMaxPatches)
        {
            reason = "more than " + std::to_string(kMaxPatches) + " patches";
            return false;
        }
        std::array<double, kSurfaceCount> covered{};
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
                reason = "a patch of " + std::to_string(patch.area_m2) + " m2 exceeds its face's "
                    + std::to_string(face_area) + " m2";
                return false;
            }
            double& total = covered[static_cast<std::size_t>(patch.surface)];
            total += patch.area_m2;
            if (total > face_area)
            {
                reason = "the patches on one face total " + std::to_string(total)
                    + " m2, over its face's " + std::to_string(face_area) + " m2";
                return false;
            }
        }

        if (!furnishings_valid(*this, reason))
        {
            return false;
        }

        const double floor_absorption = min_mean_absorption();
        const std::array<double, kBandCount> absorption =
            furnishing_count > 0 ? furnished_absorption(*this) : mean_absorption();
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            if (absorption[band] < floor_absorption)
            {
                reason = "mean absorption " + std::to_string(absorption[band]) + " at "
                    + std::to_string(static_cast<int>(kBandCentresHz[band]))
                    + " Hz is under the " + std::to_string(floor_absorption)
                    + " this room needs to decay inside "
                    + std::to_string(static_cast<int>(kMaxDecaySeconds)) + " s";
                return false;
            }
        }

        return true;
    }
}
