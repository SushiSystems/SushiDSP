/**************************************************************************/
/* furnishing_field.hpp                                                   */
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
 * @file furnishing_field.hpp
 * @brief Declares where in a room the furnishings' absorption lies, for a path and for a mode.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/medium_absorption.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

namespace SushiDSP::room
{
    /// @brief Spreads each placed furnishing's absorption as a density around where it stands,
    /// and answers what a reflected path and a room mode lose to it. The room's total is the one
    /// MediumAbsorption gives; a scene with no placed furnishing answers exactly as it does.
    /// Control thread.
    /// @see docs/design/ROOM_FURNISHINGS.md, "Furniture with a place"
    class FurnishingField
    {
        public:
            /// @brief Bounds a mode index whose weight is tabulated; a higher index weighs one.
            static constexpr std::uint32_t K_MAX_MODE_INDEX = 48;

            /// @brief Bounds a density's spread across the floor from below, in metres.
            static constexpr double K_MIN_SPREAD_M = 0.15;

            /// @brief Reads the scene's furnishings and tabulates each placed one's density.
            /// @pre scene.is_valid() holds.
            explicit FurnishingField(const RoomScene& scene);

            /// @brief Reports whether any furnishing has a place.
            bool placed() const noexcept;

            /// @brief Returns the pressure gain a reflected path keeps, per band, through the air
            /// and the furnishings it runs past.
            /// @param image The path's start in unfolded space: the image source.
            /// @param receiver The path's end, inside the room.
            /// @param metres The path's length.
            BandResponse over_path(const Vec3& image, const Vec3& receiver,
                                   double metres) const noexcept;

            /// @brief Returns how many times the room's mean density a path runs through, for one
            /// placed furnishing. Averaged over every path of a diffuse field it is one.
            /// @param furnishing The placed furnishing's index among the placed ones.
            double path_weight(std::size_t furnishing, const Vec3& image, const Vec3& receiver,
                               double metres) const noexcept;

            /// @brief Returns the furnishings' energy attenuation per metre as one mode sees it,
            /// for one band: each placed furnishing weighed by the mode's pressure squared over its
            /// density, relative to the mode's mean.
            /// @pre band < K_BAND_COUNT.
            double mode_coefficient_per_metre(std::size_t band, std::uint32_t nx, std::uint32_t ny,
                                              std::uint32_t nz) const noexcept;

            /// @brief Returns the number of placed furnishings.
            std::size_t placed_count() const noexcept;

            /// @brief Returns the density of one placed furnishing at a point in the room, per
            /// cubic metre; its integral over the room is one.
            double density(std::size_t furnishing, const Vec3& point) const noexcept;

        private:
            /// @brief Holds one placed furnishing's density and its loss.
            struct Item
            {
                    /// @brief Stores the density's centre.
                    Vec3 centre;

                    /// @brief Stores the spread along x, y and z, in metres.
                    std::array<double, 3> spread;

                    /// @brief Stores one over the unnormalised density's integral over the room.
                    double scale;

                    /// @brief Stores A / 4V per band, in energy attenuation per metre.
                    std::array<double, K_BAND_COUNT> coefficient;

                    /// @brief Stores, per axis and mode index, the mode's pressure squared over
                    /// the density relative to its mean.
                    std::array<std::array<double, K_MAX_MODE_INDEX + 1>, 3> mode_weight;
            };

            /// @brief Returns one axis's mode weights for a density centred at centre.
            static std::array<double, K_MAX_MODE_INDEX + 1>
            axis_mode_weights(double centre, double spread, double length) noexcept;

            /// @brief Stores the room's three lengths, in metres.
            std::array<double, 3> lengths_{};

            /// @brief Stores the room's volume, in cubic metres.
            double volume_ = 0.0;

            /// @brief Evaluates the scene as MediumAbsorption does, the answer without a place.
            MediumAbsorption medium_;

            /// @brief Stores the unplaced furnishings' A / 4V per band.
            std::array<double, K_BAND_COUNT> uniform_{};

            /// @brief Stores the placed furnishings.
            std::vector<Item> items_;
    };
} // namespace SushiDSP::room
