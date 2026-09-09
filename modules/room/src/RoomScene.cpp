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

namespace SushiDSP::room
{
    namespace
    {

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

        for (std::uint32_t index = 0; index < patch_count; ++index)
        {
            const Patch& patch = patches[index];
            const double face_area = face_area_m2(patch.surface);
            if (patch.area_m2 > face_area)
            {
                reason = "a patch of " + std::to_string(patch.area_m2) + " m2 exceeds its face's "
                    + std::to_string(face_area) + " m2";
                return false;
            }
        }

        return true;
    }
}
