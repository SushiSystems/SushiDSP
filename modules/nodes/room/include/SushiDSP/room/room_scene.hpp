/**************************************************************************/
/* room_scene.hpp                                                         */
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
 * @file room_scene.hpp
 * @brief The shoebox room: its six surfaces, their patches, and the source, listener and
 * microphone poses every solver reads.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "SushiDSP/room/band_response.hpp"
#include "SushiDSP/room/cabinet_directivity.hpp"
#include "SushiDSP/room/furniture.hpp"
#include "SushiDSP/room/materials.hpp"
#include "SushiDSP/room/microphone_model.hpp"

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
    inline constexpr std::size_t K_SURFACE_COUNT = 6;

    /// @brief The whole room, control-thread owned, written by the host and read by every solver.
    /// Spec §4.1.
    struct RoomScene
    {
            /// @brief Bounds a shoebox dimension from below, in metres.
            static constexpr double K_MIN_DIMENSION = 1.5;

            /// @brief Bounds a shoebox dimension from above, in metres.
            static constexpr double K_MAX_DIMENSION = 12.0;

            /// @brief Bounds the reverberation time a scene may ask the module to render, in
            /// seconds. Spec section 4.1.
            static constexpr double K_MAX_DECAY_SECONDS = 12.0;

            /// @brief Counts the patches a scene may carry.
            static constexpr std::size_t K_MAX_PATCHES = 8;

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

            /// @brief Counts the furnishings a scene may carry.
            static constexpr std::size_t K_MAX_FURNISHINGS = 16;

            /// @brief A quantity of one furniture row standing in the room.
            struct Furnishing
            {
                    /// @brief Names the furniture row.
                    FurnitureId item;

                    /// @brief Counts items, or square metres, as the row's unit says.
                    double quantity;
            };

            /// @brief Stores the shoebox's width, depth and height, in metres.
            Vec3 dimensions{6.0, 4.0, 2.5};

            /// @brief Stores each face's material, ordered as Surface.
            std::array<MaterialId, K_SURFACE_COUNT> surfaces{
                MaterialId::GypsumBoardOnStuds,    MaterialId::GypsumBoardOnStuds,
                MaterialId::GypsumBoardOnStuds,    MaterialId::GypsumBoardOnStuds,
                MaterialId::HeavyCarpetOnConcrete, MaterialId::GypsumBoardOnStuds};

            /// @brief Stores each face's scattering, ordered as Surface.
            std::array<Scattering, K_SURFACE_COUNT> scattering{};

            /// @brief Stores the patches carried by this scene; only patch_count of these are live.
            std::array<Patch, K_MAX_PATCHES> patches{};

            /// @brief Counts the live entries of patches.
            std::uint32_t patch_count = 0;

            /// @brief Stores the furnishings carried by this scene; only furnishing_count of these
            /// are live.
            std::array<Furnishing, K_MAX_FURNISHINGS> furnishings{};

            /// @brief Counts the live entries of furnishings.
            std::uint32_t furnishing_count = 0;

            /// @brief Adds K_BUILT_ROOM_LOSS to every surface's absorption when set; off for an ideal
            /// shoebox or a sealed chamber.
            bool built_room_loss = false;

            /// @brief Stores the cabinet's position.
            Vec3 source_position{1.5, 2.0, 0.5};

            /// @brief Stores the cabinet's yaw, in radians, zero facing +x.
            double source_yaw_rad = 0.0;

            /// @brief Stores the cabinet's geometry.
            CabinetGeometry cabinet = K_CABINET4X12;

            /// @brief Stores the listener's position.
            Vec3 listener_position{4.0, 2.0, 1.2};

            /// @brief Stores the listener's yaw, in radians, zero facing +x.
            double listener_yaw_rad = 0.0;

            /// @brief Stores the microphone's position.
            Vec3 microphone_position{1.6, 2.0, 0.5};

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

            /// @brief Area-weighted absorption per band over one face and the patches on it, plus
            /// K_BUILT_ROOM_LOSS when built_room_loss is set, capped at 1.
            std::array<double, K_BAND_COUNT> face_absorption(Surface surface) const;

            /// @brief Area-weighted absorption per band over the six faces and their patches.
            std::array<double, K_BAND_COUNT> mean_absorption() const;

            /// @brief Sums the live furnishings' equivalent absorption area per band, in square
            /// metres; reads at most K_MAX_FURNISHINGS entries.
            /// @pre every live entry names a furniture row.
            std::array<double, K_BAND_COUNT> furnishing_absorption_area() const;

            /// @brief Returns the absorption one band must reach for this shoebox to decay inside
            /// K_MAX_DECAY_SECONDS, taking its surfaces as the only absorber.
            double min_mean_absorption() const noexcept;

            /// @brief Whether every dimension, every pose, every patch, every furnishing and every
            /// band's absorption is inside its bound.
            /// @param reason Filled with the first violation when the scene is refused.
            bool is_valid(std::string& reason) const;
    };
} // namespace SushiDSP::room
