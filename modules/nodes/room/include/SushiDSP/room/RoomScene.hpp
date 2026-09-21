/**************************************************************************/
/* RoomScene.hpp                                                          */
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
 * @file RoomScene.hpp
 * @brief The shoebox room: its six surfaces, their patches, and the source, listener and
 * microphone poses every solver reads.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/CabinetDirectivity.hpp"
#include "SushiDSP/room/Materials.hpp"
#include "SushiDSP/room/MicrophoneModel.hpp"

namespace SushiDSP::room
{
    /// @brief Holds a point or a direction in room space, in metres.
    struct Vec3
    {
        double x;
        double y;
        double z;
    };

    /// @brief Which face of the shoebox a surface index names.
    enum class Surface : std::uint8_t
    {
        MinusX = 0,
        PlusX,
        MinusY,
        PlusY,
        Floor,
        Ceiling
    };

    /// @brief Counts the shoebox's faces.
    inline constexpr std::size_t kSurfaceCount = 6;

    /// @brief The whole room, control-thread owned, written by the host and read by every solver.
    /// Spec §4.1.
    struct RoomScene
    {
        /// @brief Bounds a shoebox dimension from below, in metres.
        static constexpr double kMinDimension = 1.5;

        /// @brief Bounds a shoebox dimension from above, in metres.
        static constexpr double kMaxDimension = 12.0;

        /// @brief Bounds the reverberation time a scene may ask the module to render, in
        /// seconds. Spec section 4.1.
        static constexpr double kMaxDecaySeconds = 12.0;

        /// @brief Counts the patches a scene may carry.
        static constexpr std::size_t kMaxPatches = 4;

        /// @brief A patch of a different material on one face: the open doorway, the window.
        struct Patch
        {
            /// @brief Names the face the patch sits on.
            Surface surface;

            /// @brief Names the patch's material, distinct from its face's own material.
            MaterialId material;

            /// @brief Stores the patch's area, in square metres.
            /// @pre area_m2 does not exceed its face's own area.
            double area_m2;
        };

        /// @brief Stores the shoebox's width, depth and height, in metres.
        Vec3 dimensions{ 6.0, 4.0, 2.5 };

        /// @brief Stores each face's material, ordered as Surface.
        std::array<MaterialId, kSurfaceCount> surfaces{ MaterialId::GypsumBoardOnStuds,
                                                        MaterialId::GypsumBoardOnStuds,
                                                        MaterialId::GypsumBoardOnStuds,
                                                        MaterialId::GypsumBoardOnStuds,
                                                        MaterialId::HeavyCarpetOnConcrete,
                                                        MaterialId::GypsumBoardOnStuds };

        /// @brief Stores each face's scattering, ordered as Surface.
        std::array<Scattering, kSurfaceCount> scattering{};

        /// @brief Stores the patches carried by this scene; only patch_count of these are live.
        std::array<Patch, kMaxPatches> patches{};

        /// @brief Counts the live entries of patches.
        std::uint32_t patch_count = 0;

        /// @brief Stores the cabinet's position.
        Vec3 source_position{ 1.5, 2.0, 0.5 };

        /// @brief Stores the cabinet's yaw, in radians, zero facing +x.
        double source_yaw_rad = 0.0;

        /// @brief Stores the cabinet's geometry.
        CabinetGeometry cabinet = kCabinet4x12;

        /// @brief Stores the listener's position.
        Vec3 listener_position{ 4.0, 2.0, 1.2 };

        /// @brief Stores the listener's yaw, in radians, zero facing +x.
        double listener_yaw_rad = 0.0;

        /// @brief Stores the microphone's position.
        Vec3 microphone_position{ 1.6, 2.0, 0.5 };

        /// @brief Stores the microphone's yaw, in radians, zero facing +x.
        double microphone_yaw_rad = 0.0;

        /// @brief Stores the microphone's polar pattern.
        PolarPattern microphone_pattern = PolarPattern::Cardioid;

        /// @brief Returns the shoebox's volume, in cubic metres.
        double volume_m3() const noexcept;

        /// @brief Returns the shoebox's total surface area, in square metres.
        double surface_area_m2() const noexcept;

        /// @brief Returns one face's area, in square metres.
        double face_area_m2(Surface surface) const noexcept;

        /// @brief Area-weighted absorption per band over one face and the patches on it.
        std::array<double, kBandCount> face_absorption(Surface surface) const;

        /// @brief Area-weighted absorption per band over the six faces and their patches.
        std::array<double, kBandCount> mean_absorption() const;

        /// @brief Returns the absorption one band must reach for this shoebox to decay inside
        /// kMaxDecaySeconds, taking its surfaces as the only absorber.
        double min_mean_absorption() const noexcept;

        /// @brief Whether every dimension, every pose, every patch and every band's absorption
        /// is inside its bound.
        /// @param reason Filled with the first violation when the scene is refused.
        bool is_valid(std::string& reason) const;
    };
}
