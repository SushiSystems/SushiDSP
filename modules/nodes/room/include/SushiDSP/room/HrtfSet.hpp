/**************************************************************************/
/* HrtfSet.hpp                                                            */
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
 * @file HrtfSet.hpp
 * @brief Minimum-phase HRIR pairs on a direction grid, their ITDs and the triangle table they blend on.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace SushiDSP::room
{
    /// @brief Holds minimum-phase HRIR pairs on a direction grid, their ITDs, and the triangle
    /// table they interpolate on, all on the control thread; the audio thread interpolates
    /// nothing. Spec §9.3.
    class HrtfSet
    {
    public:
        /// @brief Counts the taps of one minimum-phase HRIR.
        static constexpr std::uint32_t kTapCount = 128;

        /// @brief Counts the taps of one Ambisonic decoder filter.
        static constexpr std::uint32_t kDecoderTaps = 256;

        /// @brief Counts the taps of one diffuse-field filter.
        static constexpr std::uint32_t kDiffuseTaps = 128;

        /// @brief Holds one grid direction and the ITD measured there.
        struct Direction
        {
            /// @brief Stores the azimuth in radians, 0 front, positive to the left.
            float azimuth_rad;

            /// @brief Stores the elevation in radians, 0 horizontal, positive up.
            float elevation_rad;

            /// @brief Stores the interaural delay in frames; positive means the right ear is
            /// later.
            float itd_frames;
        };

        /// @brief Holds one HRIR pair and its ITD.
        struct Pair
        {
            /// @brief Stores the left ear's minimum-phase response.
            std::array<float, kTapCount> left;

            /// @brief Stores the right ear's minimum-phase response.
            std::array<float, kTapCount> right;

            /// @brief Stores the interaural delay in frames; positive means the right ear is
            /// later.
            float itd_frames;
        };

        /// @brief Builds an empty set that holds no direction.
        HrtfSet();

        /// @brief Builds the set and its triangle table from a grid and its taps.
        /// @param left_taps direction count times kTapCount values, direction-major.
        /// @param right_taps The same layout for the right ear.
        /// @pre At least four distinct directions, not all within one hemisphere, so the grid's
        /// convex hull encloses the origin and every direction lies in some triangle.
        /// @throw std::invalid_argument when the tap vectors do not match the grid or the grid
        /// does not cover the sphere.
        HrtfSet(double sample_rate, std::vector<Direction> directions,
                std::vector<float> left_taps, std::vector<float> right_taps);

        /// @brief Reports whether the set holds no direction.
        bool empty() const noexcept;

        /// @brief Returns the sample rate the responses were measured at, in hertz.
        double sample_rate() const noexcept;

        /// @brief Counts the grid directions.
        std::uint32_t direction_count() const noexcept;

        /// @brief Returns one grid direction.
        /// @pre index < direction_count().
        const Direction& direction(std::uint32_t index) const noexcept;

        /// @brief Blends the three grid directions around one direction by barycentric weight.
        /// @throw std::runtime_error when the set is empty.
        Pair interpolate(double azimuth_rad, double elevation_rad) const;

        /// @brief Returns the 2 * (order + 1)^2 decoder filters, ACN order, left ear then right,
        /// each kDecoderTaps long.
        const std::vector<float>& decoder_filters() const noexcept;

        /// @brief Returns the Ambisonic order the decoder filters were designed for.
        std::uint32_t decoder_order() const noexcept;

        /// @brief Stores the decoder filters for one order.
        /// @pre filters.size() == 2 * (order + 1)^2 * kDecoderTaps.
        /// @throw std::invalid_argument when the size does not match the order.
        void set_decoder(std::uint32_t order, std::vector<float> filters);

        /// @brief Returns one ear's diffuse-field average, 0 left and 1 right. Spec §6.4.
        /// @pre ear < 2.
        const std::array<float, kDiffuseTaps>& diffuse_field(std::uint32_t ear) const noexcept;

        /// @brief Stores the two diffuse-field averages.
        void set_diffuse_field(std::array<float, kDiffuseTaps> left,
                               std::array<float, kDiffuseTaps> right);

    private:
        /// @brief Holds one unit vector on the sphere.
        using Vector = std::array<double, 3>;

        /// @brief Holds the three direction indices of one triangle, wound outward.
        using Triangle = std::array<std::uint32_t, 3>;

        /// @brief Builds the convex hull of the unit vectors, which is their triangulation on
        /// the sphere.
        void build_triangles();

        /// @brief Returns the triangle around a unit vector and the three weights that blend
        /// its vertices to the vector, summing to one.
        /// @return The triangle's index.
        std::uint32_t locate(const Vector& vector, std::array<double, 3>& weights) const noexcept;

        /// @brief Stores the sample rate in hertz.
        double sample_rate_;

        /// @brief Stores the grid directions.
        std::vector<Direction> directions_;

        /// @brief Stores each direction's unit vector.
        std::vector<Vector> vectors_;

        /// @brief Stores the triangle table over the direction indices.
        std::vector<Triangle> triangles_;

        /// @brief Stores the left taps, direction-major, kTapCount per direction.
        std::vector<float> left_taps_;

        /// @brief Stores the right taps, direction-major, kTapCount per direction.
        std::vector<float> right_taps_;

        /// @brief Stores the decoder's order.
        std::uint32_t decoder_order_;

        /// @brief Stores the decoder filters, ACN order, left ear then right.
        std::vector<float> decoder_filters_;

        /// @brief Stores the left and right diffuse-field averages.
        std::array<std::array<float, kDiffuseTaps>, 2> diffuse_field_;
    };
}
