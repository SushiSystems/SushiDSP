/**************************************************************************/
/* HrtfSet.cpp                                                            */
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
 * @file HrtfSet.cpp
 * @brief An incremental convex hull over the grid and a barycentric blend inside its triangles.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/HrtfSet.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds one unit vector on the sphere.
        using Vector = std::array<double, 3>;

        /// @brief Holds the three direction indices of one hull face, wound outward.
        using Triangle = std::array<std::uint32_t, 3>;

        /// @brief Bounds how far off a face's plane a point may sit and still count as on it.
        constexpr double kPlaneEpsilon = 1.0e-12;

        /// @brief Bounds how small a barycentric weight may be before it reads as zero.
        constexpr double kWeightEpsilon = 1.0e-6;

        /// @brief Holds the cosine of the smallest angle two grid directions may lie apart, 1 degree.
        const double kMinSpacingCosine = std::cos(1.0 * 3.14159265358979323846 / 180.0);

        /// @brief Returns the unit vector of a direction: x forward, y left, z up.
        Vector unit_vector(double azimuth_rad, double elevation_rad) noexcept
        {
            const double cos_elevation = std::cos(elevation_rad);
            return { cos_elevation * std::cos(azimuth_rad), cos_elevation * std::sin(azimuth_rad),
                     std::sin(elevation_rad) };
        }

        /// @brief Returns a minus b.
        Vector subtract(const Vector& a, const Vector& b) noexcept
        {
            return { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
        }

        /// @brief Returns the cross product of a and b.
        Vector cross(const Vector& a, const Vector& b) noexcept
        {
            return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                     a[0] * b[1] - a[1] * b[0] };
        }

        /// @brief Returns the dot product of a and b.
        double dot(const Vector& a, const Vector& b) noexcept
        {
            return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
        }

        /// @brief Returns the scalar triple product of a, b and c.
        double determinant(const Vector& a, const Vector& b, const Vector& c) noexcept
        {
            return dot(a, cross(b, c));
        }

        /// @brief Returns how far a point sits in front of a face's plane, scaled by twice the
        /// face's area.
        double side_of(const std::vector<Vector>& points, const Triangle& face,
                       const Vector& point) noexcept
        {
            const Vector& a = points[face[0]];
            const Vector normal =
                cross(subtract(points[face[1]], a), subtract(points[face[2]], a));
            return dot(normal, subtract(point, a));
        }

        /// @brief Returns the four indices of a non-degenerate starting tetrahedron.
        /// @throw std::invalid_argument when every point is coplanar.
        std::array<std::uint32_t, 4> starting_tetrahedron(const std::vector<Vector>& points)
        {
            const auto count = static_cast<std::uint32_t>(points.size());
            std::array<std::uint32_t, 4> chosen = { 0, 0, 0, 0 };
            double best = 0.0;
            for (std::uint32_t index = 1; index < count; ++index)
            {
                const Vector difference = subtract(points[index], points[0]);
                const double distance = dot(difference, difference);
                if (distance > best)
                {
                    best = distance;
                    chosen[1] = index;
                }
            }
            const Vector edge = subtract(points[chosen[1]], points[0]);
            best = 0.0;
            for (std::uint32_t index = 1; index < count; ++index)
            {
                const Vector area = cross(edge, subtract(points[index], points[0]));
                const double magnitude = dot(area, area);
                if (magnitude > best)
                {
                    best = magnitude;
                    chosen[2] = index;
                }
            }
            const Vector normal = cross(edge, subtract(points[chosen[2]], points[0]));
            best = 0.0;
            for (std::uint32_t index = 1; index < count; ++index)
            {
                const double volume = std::fabs(dot(normal, subtract(points[index], points[0])));
                if (volume > best)
                {
                    best = volume;
                    chosen[3] = index;
                }
            }
            if (best <= kPlaneEpsilon)
            {
                throw std::invalid_argument("HrtfSet: the grid does not span three dimensions");
            }
            return chosen;
        }

        /// @brief Returns a face wound so that its normal points away from an interior point.
        Triangle wound_outward(const std::vector<Vector>& points, Triangle face,
                               const Vector& interior) noexcept
        {
            if (side_of(points, face, interior) > 0.0)
            {
                std::swap(face[1], face[2]);
            }
            return face;
        }

        /// @brief Adds one point to a hull, replacing every face it sees by a fan from its
        /// horizon.
        void add_point(const std::vector<Vector>& points, std::vector<Triangle>& faces,
                       std::uint32_t index)
        {
            const Vector& point = points[index];
            std::vector<bool> visible(faces.size());
            bool any_visible = false;
            for (std::size_t face = 0; face < faces.size(); ++face)
            {
                visible[face] = side_of(points, faces[face], point) >= -kPlaneEpsilon;
                any_visible = any_visible || visible[face];
            }
            if (!any_visible)
            {
                return;
            }
            std::vector<std::pair<std::uint32_t, std::uint32_t>> visible_edges;
            for (std::size_t face = 0; face < faces.size(); ++face)
            {
                if (visible[face])
                {
                    for (std::size_t corner = 0; corner < 3; ++corner)
                    {
                        visible_edges.emplace_back(faces[face][corner],
                                                   faces[face][(corner + 1) % 3]);
                    }
                }
            }
            std::sort(visible_edges.begin(), visible_edges.end());
            std::vector<Triangle> kept;
            kept.reserve(faces.size() + 2);
            for (std::size_t face = 0; face < faces.size(); ++face)
            {
                if (!visible[face])
                {
                    kept.push_back(faces[face]);
                }
            }
            for (const auto& edge : visible_edges)
            {
                const auto reverse = std::make_pair(edge.second, edge.first);
                if (!std::binary_search(visible_edges.begin(), visible_edges.end(), reverse))
                {
                    kept.push_back({ edge.first, edge.second, index });
                }
            }
            faces.swap(kept);
        }
    }

    HrtfSet::HrtfSet()
        : sample_rate_(0.0), decoder_order_(0), diffuse_field_{}
    {
    }

    HrtfSet::HrtfSet(double sample_rate, std::vector<Direction> directions,
                     std::vector<float> left_taps, std::vector<float> right_taps)
        : sample_rate_(sample_rate), directions_(std::move(directions)),
          left_taps_(std::move(left_taps)), right_taps_(std::move(right_taps)),
          decoder_order_(0), diffuse_field_{}
    {
        const std::size_t expected = directions_.size() * kTapCount;
        if (left_taps_.size() != expected || right_taps_.size() != expected)
        {
            throw std::invalid_argument("HrtfSet: the tap vectors do not match the grid");
        }
        if (directions_.size() < 4)
        {
            throw std::invalid_argument("HrtfSet: a grid needs at least four directions");
        }
        vectors_.reserve(directions_.size());
        for (const Direction& direction : directions_)
        {
            vectors_.push_back(unit_vector(direction.azimuth_rad, direction.elevation_rad));
        }
        // Near-coincident directions leave the hull build's plane tests degenerate.
        for (std::size_t first = 0; first < vectors_.size(); ++first)
        {
            for (std::size_t second = first + 1; second < vectors_.size(); ++second)
            {
                const double cosine = vectors_[first][0] * vectors_[second][0]
                    + vectors_[first][1] * vectors_[second][1]
                    + vectors_[first][2] * vectors_[second][2];
                if (cosine > kMinSpacingCosine)
                {
                    throw std::invalid_argument(
                        "HrtfSet: two grid directions lie closer than 1 degree");
                }
            }
        }
        build_triangles();
    }

    void HrtfSet::build_triangles()
    {
        const std::array<std::uint32_t, 4> seed = starting_tetrahedron(vectors_);
        Vector interior = { 0.0, 0.0, 0.0 };
        for (const std::uint32_t index : seed)
        {
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                interior[axis] += vectors_[index][axis] / 4.0;
            }
        }
        triangles_ = { wound_outward(vectors_, { seed[0], seed[1], seed[2] }, interior),
                       wound_outward(vectors_, { seed[0], seed[1], seed[3] }, interior),
                       wound_outward(vectors_, { seed[0], seed[2], seed[3] }, interior),
                       wound_outward(vectors_, { seed[1], seed[2], seed[3] }, interior) };
        const auto count = static_cast<std::uint32_t>(vectors_.size());
        for (std::uint32_t index = 0; index < count; ++index)
        {
            if (std::find(seed.begin(), seed.end(), index) == seed.end())
            {
                add_point(vectors_, triangles_, index);
                // A hull over n points on a sphere holds 2n - 4 faces; more means a degenerate build.
                if (triangles_.size() > 2u * static_cast<std::size_t>(count))
                {
                    throw std::invalid_argument("HrtfSet: the grid's hull build degenerated");
                }
            }
        }
        // Every face keeps the origin strictly behind it and every grid point is a vertex.
        std::vector<bool> used(count);
        for (const Triangle& triangle : triangles_)
        {
            if (determinant(vectors_[triangle[0]], vectors_[triangle[1]],
                            vectors_[triangle[2]]) <= kPlaneEpsilon)
            {
                throw std::invalid_argument("HrtfSet: the grid does not cover the sphere");
            }
            for (const std::uint32_t index : triangle)
            {
                used[index] = true;
            }
        }
        if (std::find(used.begin(), used.end(), false) != used.end())
        {
            throw std::invalid_argument("HrtfSet: a grid direction lies inside the hull");
        }
    }

    std::uint32_t HrtfSet::locate(const Vector& vector,
                                  std::array<double, 3>& weights) const noexcept
    {
        std::uint32_t best = 0;
        double best_margin = -1.0;
        std::array<double, 3> best_parts = { 1.0, 0.0, 0.0 };
        for (std::size_t index = 0; index < triangles_.size(); ++index)
        {
            const Triangle& triangle = triangles_[index];
            const Vector& a = vectors_[triangle[0]];
            const Vector& b = vectors_[triangle[1]];
            const Vector& c = vectors_[triangle[2]];
            const std::array<double, 3> parts = { determinant(vector, b, c),
                                                  determinant(a, vector, c),
                                                  determinant(a, b, vector) };
            const double margin = std::min({ parts[0], parts[1], parts[2] });
            if (margin > best_margin)
            {
                best_margin = margin;
                best = static_cast<std::uint32_t>(index);
                best_parts = parts;
            }
        }
        for (double& part : best_parts)
        {
            part = std::max(part, 0.0);
        }
        double sum = best_parts[0] + best_parts[1] + best_parts[2];
        for (double& part : best_parts)
        {
            part = part / sum < kWeightEpsilon ? 0.0 : part;
        }
        sum = best_parts[0] + best_parts[1] + best_parts[2];
        for (std::size_t corner = 0; corner < 3; ++corner)
        {
            weights[corner] = best_parts[corner] / sum;
        }
        return best;
    }

    HrtfSet::Pair HrtfSet::interpolate(double azimuth_rad, double elevation_rad) const
    {
        if (empty())
        {
            throw std::runtime_error("HrtfSet: interpolate on an empty set");
        }
        std::array<double, 3> weights{};
        const Triangle& triangle =
            triangles_[locate(unit_vector(azimuth_rad, elevation_rad), weights)];
        std::array<double, kTapCount> left_sum{};
        std::array<double, kTapCount> right_sum{};
        double itd = 0.0;
        for (std::size_t corner = 0; corner < 3; ++corner)
        {
            const std::uint32_t index = triangle[corner];
            const double weight = weights[corner];
            const float* left = left_taps_.data() + static_cast<std::size_t>(index) * kTapCount;
            const float* right = right_taps_.data() + static_cast<std::size_t>(index) * kTapCount;
            for (std::uint32_t tap = 0; tap < kTapCount; ++tap)
            {
                left_sum[tap] += weight * left[tap];
                right_sum[tap] += weight * right[tap];
            }
            itd += weight * directions_[index].itd_frames;
        }
        Pair pair{};
        for (std::uint32_t tap = 0; tap < kTapCount; ++tap)
        {
            pair.left[tap] = static_cast<float>(left_sum[tap]);
            pair.right[tap] = static_cast<float>(right_sum[tap]);
        }
        pair.itd_frames = static_cast<float>(itd);
        return pair;
    }

    bool HrtfSet::empty() const noexcept
    {
        return directions_.empty();
    }

    double HrtfSet::sample_rate() const noexcept
    {
        return sample_rate_;
    }

    std::uint32_t HrtfSet::direction_count() const noexcept
    {
        return static_cast<std::uint32_t>(directions_.size());
    }

    const HrtfSet::Direction& HrtfSet::direction(std::uint32_t index) const noexcept
    {
        return directions_[index];
    }

    const std::vector<float>& HrtfSet::decoder_filters() const noexcept
    {
        return decoder_filters_;
    }

    std::uint32_t HrtfSet::decoder_order() const noexcept
    {
        return decoder_order_;
    }

    void HrtfSet::set_decoder(std::uint32_t order, std::vector<float> filters)
    {
        const std::size_t channels = static_cast<std::size_t>(order + 1) * (order + 1);
        if (filters.size() != 2 * channels * kDecoderTaps)
        {
            throw std::invalid_argument("HrtfSet: the decoder filters do not match the order");
        }
        decoder_order_ = order;
        decoder_filters_ = std::move(filters);
    }

    const std::array<float, HrtfSet::kDiffuseTaps>&
    HrtfSet::diffuse_field(std::uint32_t ear) const noexcept
    {
        return diffuse_field_[ear];
    }

    void HrtfSet::set_diffuse_field(std::array<float, kDiffuseTaps> left,
                                    std::array<float, kDiffuseTaps> right)
    {
        diffuse_field_[0] = left;
        diffuse_field_[1] = right;
    }
}
