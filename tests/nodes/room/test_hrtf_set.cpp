/**************************************************************************/
/* test_hrtf_set.cpp                                                      */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_hrtf_set.cpp
 * @brief Tests for HrtfSet, the direction grid and the triangle table it interpolates on.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <SushiDSP/nodes/room/hrtf_set.hpp>

#include "support/tolerance.hpp"

namespace
{
    using SushiDSP::room::HrtfSet;
    using Taps = std::array<float, HrtfSet::K_TAP_COUNT>;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Counts the directions of the synthetic grid: the six axes.
    constexpr std::uint32_t K_AXIS_COUNT = 6;

    /// @brief Counts the seeded directions of the weight property.
    constexpr int K_TRIALS = 500;

    /// @brief Bounds how far a grid angle held as a float sits from the angle it was made from: the
    /// half ulp of float(pi), which lies in [2, 4) and so has the half ulp 2 u.
    constexpr double K_GRID_ANGLE_ERROR = 2.0 * TestSupport::unit_roundoff<float>();

    /// @brief Counts the units a barycentric weight vector moves by, in its one-norm, per unit of
    /// angle error on the three unit vectors it solves against: the vertices are the identity up to
    /// that error, so the solve moves each of the three weights by at most the error.
    constexpr double K_WEIGHT_ERROR_UNITS = 3.0;

    /// @brief Counts the roundings of the float result: the three products, two sums and the store.
    constexpr double K_RESULT_ROUNDINGS = 6.0;

    /// @brief Bounds the miss of a blend whose three taps differ by at most @p spread and whose
    /// result has the magnitude @p magnitude: the weight error times the spread, plus the roundings.
    double blend_bound(double spread, double magnitude)
    {
        return K_WEIGHT_ERROR_UNITS * K_GRID_ANGLE_ERROR * spread +
               K_RESULT_ROUNDINGS * TestSupport::unit_roundoff<float>() * magnitude;
    }

    /// @brief Returns the left tap a grid direction carries: the index plus a slope per tap.
    float left_tap(std::uint32_t direction, std::uint32_t tap)
    {
        return static_cast<float>(direction) + 0.001f * static_cast<float>(tap);
    }

    /// @brief Returns the right tap a grid direction carries: ten past the left.
    float right_tap(std::uint32_t direction, std::uint32_t tap)
    {
        return 10.0f + left_tap(direction, tap);
    }

    /// @brief Returns the ITD a grid direction carries: half its index, less one.
    float itd_of(std::uint32_t direction)
    {
        return 0.5f * static_cast<float>(direction) - 1.0f;
    }

    /// @brief Returns the left taps of a grid direction.
    Taps left_taps_of(std::uint32_t direction)
    {
        Taps taps{};
        for (std::uint32_t tap = 0; tap < HrtfSet::K_TAP_COUNT; ++tap)
        {
            taps[tap] = left_tap(direction, tap);
        }
        return taps;
    }

    /// @brief Returns the right taps of a grid direction.
    Taps right_taps_of(std::uint32_t direction)
    {
        Taps taps{};
        for (std::uint32_t tap = 0; tap < HrtfSet::K_TAP_COUNT; ++tap)
        {
            taps[tap] = right_tap(direction, tap);
        }
        return taps;
    }

    /// @brief Returns the mean of the first three grid directions' taps, as the given accessor reads them, in double.
    std::array<double, HrtfSet::K_TAP_COUNT> mean_of_first_three(Taps (*taps_of)(std::uint32_t))
    {
        std::array<double, HrtfSet::K_TAP_COUNT> mean{};
        for (std::uint32_t tap = 0; tap < HrtfSet::K_TAP_COUNT; ++tap)
        {
            mean[tap] = (static_cast<double>(taps_of(0)[tap]) + taps_of(1)[tap] + taps_of(2)[tap]) / 3.0;
        }
        return mean;
    }

    /// @brief Returns the largest absolute difference between a float tap set and a double one.
    double worst_difference(const Taps& computed, const std::array<double, HrtfSet::K_TAP_COUNT>& reference)
    {
        double worst = 0.0;
        for (std::uint32_t tap = 0; tap < HrtfSet::K_TAP_COUNT; ++tap)
        {
            worst = std::max(worst, std::fabs(static_cast<double>(computed[tap]) - reference[tap]));
        }
        return worst;
    }

    /// @brief Builds the six-axis set: +x, +y, +z, -x, -y, -z, in that order.
    HrtfSet six_axis_set()
    {
        const auto half_pi = static_cast<float>(K_PI / 2.0);
        const auto pi = static_cast<float>(K_PI);
        std::vector<HrtfSet::Direction> directions = {
            {0.0f, 0.0f, itd_of(0)}, {half_pi, 0.0f, itd_of(1)},  {0.0f, half_pi, itd_of(2)},
            {pi, 0.0f, itd_of(3)},   {-half_pi, 0.0f, itd_of(4)}, {0.0f, -half_pi, itd_of(5)}};
        std::vector<float> left;
        std::vector<float> right;
        for (std::uint32_t direction = 0; direction < K_AXIS_COUNT; ++direction)
        {
            for (std::uint32_t tap = 0; tap < HrtfSet::K_TAP_COUNT; ++tap)
            {
                left.push_back(left_tap(direction, tap));
                right.push_back(right_tap(direction, tap));
            }
        }
        return HrtfSet(48000.0, std::move(directions), std::move(left), std::move(right));
    }

    /// @brief Returns the unit vector of a direction in the set's own convention.
    std::array<double, 3> unit_vector(double azimuth_rad, double elevation_rad)
    {
        return {std::cos(elevation_rad) * std::cos(azimuth_rad), std::cos(elevation_rad) * std::sin(azimuth_rad),
                std::sin(elevation_rad)};
    }

    /// @brief Holds what the seeded weight trials found over the six-axis set.
    struct WeightTrials
    {
            /// @brief Counts the results that fell outside the range of their three neighbours.
            int outside_the_neighbours = 0;

            /// @brief Stores the largest miss of the first left tap from the weighted neighbours.
            double worst_miss = 0.0;
    };

    /// @brief Runs K_TRIALS seeded directions through the set. Each weight is a coordinate's magnitude
    /// over the sum of the three magnitudes. See tests/nodes/room/README.md.
    WeightTrials run_weight_trials()
    {
        const HrtfSet set = six_axis_set();
        WeightTrials found;
        std::uint32_t state = 12345u;
        auto next_unit = [&state]()
        {
            state = state * 1664525u + 1013904223u;
            return static_cast<double>(state) / 4294967296.0;
        };
        for (int trial = 0; trial < K_TRIALS; ++trial)
        {
            const double azimuth = (2.0 * next_unit() - 1.0) * K_PI;
            const double elevation = std::asin(2.0 * next_unit() - 1.0);
            const std::array<double, 3> v = unit_vector(azimuth, elevation);
            const std::array<std::uint32_t, 3> neighbours = {v[0] > 0.0 ? 0u : 3u, v[1] > 0.0 ? 1u : 4u,
                                                             v[2] > 0.0 ? 2u : 5u};
            const float lowest =
                std::min({left_tap(neighbours[0], 0), left_tap(neighbours[1], 0), left_tap(neighbours[2], 0)});
            const float highest =
                std::max({left_tap(neighbours[0], 0), left_tap(neighbours[1], 0), left_tap(neighbours[2], 0)});
            const double sum = std::fabs(v[0]) + std::fabs(v[1]) + std::fabs(v[2]);
            const double expected = (std::fabs(v[0]) * neighbours[0] + std::fabs(v[1]) * neighbours[1] +
                                     std::fabs(v[2]) * neighbours[2]) /
                                    sum;

            const HrtfSet::Pair pair = set.interpolate(azimuth, elevation);
            if (pair.left[0] < lowest || pair.left[0] > highest)
            {
                ++found.outside_the_neighbours;
            }
            found.worst_miss = std::max(found.worst_miss, std::fabs(static_cast<double>(pair.left[0]) - expected));
        }
        return found;
    }

    /// @brief Returns the grid of the six axes plus one direction half a degree from the first.
    HrtfSet make_set_with_two_close_directions()
    {
        const auto half_pi = static_cast<float>(K_PI / 2.0);
        const auto pi = static_cast<float>(K_PI);
        const auto close = static_cast<float>(0.5 * K_PI / 180.0);
        std::vector<HrtfSet::Direction> directions = {
            {0.0f, 0.0f, 0.0f},     {half_pi, 0.0f, 0.0f},  {0.0f, half_pi, 0.0f}, {pi, 0.0f, 0.0f},
            {-half_pi, 0.0f, 0.0f}, {0.0f, -half_pi, 0.0f}, {close, 0.0f, 0.0f}};
        const std::size_t taps = directions.size() * HrtfSet::K_TAP_COUNT;
        return HrtfSet(48000.0, std::move(directions), std::vector<float>(taps, 0.0f),
                       std::vector<float>(taps, 0.0f));
    }
} // namespace

TEST(Small_HrtfSet, HrtfSetInterpolationAtAGridPointReturnsThatPointExactly)
{
    const HrtfSet set = six_axis_set();
    const HrtfSet::Direction& grid = set.direction(3);

    const HrtfSet::Pair pair = set.interpolate(grid.azimuth_rad, grid.elevation_rad);

    EXPECT_EQ(pair.left, left_taps_of(3));
    EXPECT_EQ(pair.right, right_taps_of(3));
    EXPECT_EQ(pair.itd_frames, itd_of(3));
}

TEST(Small_HrtfSet, HrtfSetInterpolationBetweenThreePointsIsABarycentricBlend)
{
    // By symmetry each of the three points carries one third. See tests/nodes/room/README.md.
    const HrtfSet set = six_axis_set();

    const HrtfSet::Pair pair = set.interpolate(K_PI / 4.0, std::atan(1.0 / std::sqrt(2.0)));

    const double left_spread = 2.0 + 0.001 * HrtfSet::K_TAP_COUNT;
    EXPECT_LE(worst_difference(pair.left, mean_of_first_three(left_taps_of)), blend_bound(left_spread, 3.0));
    EXPECT_LE(worst_difference(pair.right, mean_of_first_three(right_taps_of)), blend_bound(left_spread, 13.0));
}

TEST(Small_HrtfSet, HrtfSetInterpolationBetweenThreePointsBlendsTheItd)
{
    const HrtfSet set = six_axis_set();

    const HrtfSet::Pair pair = set.interpolate(K_PI / 4.0, std::atan(1.0 / std::sqrt(2.0)));

    const double mean = (static_cast<double>(itd_of(0)) + itd_of(1) + itd_of(2)) / 3.0;
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(pair.itd_frames, mean, blend_bound(1.0, 1.0))));
}

TEST(Small_HrtfSet, HrtfSetInterpolationWeightsKeepTheResultInsideItsThreeNeighbours)
{
    EXPECT_EQ(run_weight_trials().outside_the_neighbours, 0);
}

TEST(Small_HrtfSet, HrtfSetInterpolationWeightsSumToOne)
{
    // The neighbours differ by up to five in index, and the result stays below five.
    EXPECT_LE(run_weight_trials().worst_miss, blend_bound(5.0, 5.0));
}

TEST(Small_HrtfSet, HrtfSetInterpolateOfAnEmptySetThrows)
{
    const HrtfSet set;

    EXPECT_THROW(set.interpolate(0.0, 0.0), std::runtime_error);
}

TEST(Small_HrtfSet, HrtfSetKeepsItsDecoderOrder)
{
    HrtfSet set = six_axis_set();

    set.set_decoder(3, std::vector<float>(32 * HrtfSet::K_DECODER_TAPS));

    EXPECT_EQ(set.decoder_order(), 3u);
}

TEST(Small_HrtfSet, HrtfSetKeepsItsDecoderFilters)
{
    HrtfSet set = six_axis_set();

    set.set_decoder(3, std::vector<float>(32 * HrtfSet::K_DECODER_TAPS));

    EXPECT_EQ(set.decoder_filters().size(), 32u * HrtfSet::K_DECODER_TAPS);
}

TEST(Small_HrtfSet, HrtfSetKeepsItsLeftDiffuseFilter)
{
    HrtfSet set = six_axis_set();
    std::array<float, HrtfSet::K_DIFFUSE_TAPS> left{};
    left[0] = 0.25f;

    set.set_diffuse_field(left, std::array<float, HrtfSet::K_DIFFUSE_TAPS>{});

    EXPECT_EQ(set.diffuse_field(0), left);
}

TEST(Small_HrtfSet, HrtfSetKeepsItsRightDiffuseFilter)
{
    HrtfSet set = six_axis_set();
    std::array<float, HrtfSet::K_DIFFUSE_TAPS> right{};
    right[HrtfSet::K_DIFFUSE_TAPS - 1] = -0.5f;

    set.set_diffuse_field(std::array<float, HrtfSet::K_DIFFUSE_TAPS>{}, right);

    EXPECT_EQ(set.diffuse_field(1), right);
}

TEST(Small_HrtfSet, HrtfSetRefusesTwoDirectionsCloserThanOneDegree)
{
    EXPECT_THROW(make_set_with_two_close_directions(), std::invalid_argument);
}
