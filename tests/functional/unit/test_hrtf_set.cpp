/****************************************************************/
/* test_hrtf_set.cpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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

#include "SushiDSP/room/HrtfSet.hpp"

namespace
{
    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Counts the directions of the synthetic grid: the six axes.
    constexpr std::uint32_t kAxisCount = 6;

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

    /// @brief Builds the six-axis set: +x, +y, +z, -x, -y, -z, in that order.
    SushiDSP::room::HrtfSet six_axis_set()
    {
        const auto half_pi = static_cast<float>(kPi / 2.0);
        const auto pi = static_cast<float>(kPi);
        std::vector<SushiDSP::room::HrtfSet::Direction> directions = {
            { 0.0f, 0.0f, itd_of(0) },     { half_pi, 0.0f, itd_of(1) }, { 0.0f, half_pi, itd_of(2) },
            { pi, 0.0f, itd_of(3) },       { -half_pi, 0.0f, itd_of(4) }, { 0.0f, -half_pi, itd_of(5) }
        };
        std::vector<float> left;
        std::vector<float> right;
        for (std::uint32_t direction = 0; direction < kAxisCount; ++direction)
        {
            for (std::uint32_t tap = 0; tap < SushiDSP::room::HrtfSet::kTapCount; ++tap)
            {
                left.push_back(left_tap(direction, tap));
                right.push_back(right_tap(direction, tap));
            }
        }
        return SushiDSP::room::HrtfSet(48000.0, std::move(directions), std::move(left),
                                       std::move(right));
    }

    /// @brief Returns the unit vector of a direction in the set's own convention.
    std::array<double, 3> unit_vector(double azimuth_rad, double elevation_rad)
    {
        return { std::cos(elevation_rad) * std::cos(azimuth_rad),
                 std::cos(elevation_rad) * std::sin(azimuth_rad), std::sin(elevation_rad) };
    }
}

TEST(Unit_Dsp, HrtfSetInterpolationAtAGridPointReturnsThatPointExactly)
{
    const SushiDSP::room::HrtfSet set = six_axis_set();
    ASSERT_EQ(set.direction_count(), kAxisCount);
    const SushiDSP::room::HrtfSet::Direction& grid = set.direction(3);
    const SushiDSP::room::HrtfSet::Pair pair = set.interpolate(grid.azimuth_rad, grid.elevation_rad);
    for (std::uint32_t tap = 0; tap < SushiDSP::room::HrtfSet::kTapCount; ++tap)
    {
        EXPECT_EQ(pair.left[tap], left_tap(3, tap)) << "tap " << tap;
        EXPECT_EQ(pair.right[tap], right_tap(3, tap)) << "tap " << tap;
    }
    EXPECT_EQ(pair.itd_frames, itd_of(3));
}

TEST(Unit_Dsp, HrtfSetInterpolationBetweenThreePointsIsABarycentricBlend)
{
    // By symmetry each of the three points carries one third. See tests/functional/unit/README.md.
    const SushiDSP::room::HrtfSet set = six_axis_set();
    const SushiDSP::room::HrtfSet::Pair pair =
        set.interpolate(kPi / 4.0, std::atan(1.0 / std::sqrt(2.0)));
    for (std::uint32_t tap = 0; tap < SushiDSP::room::HrtfSet::kTapCount; ++tap)
    {
        const double left_mean =
            (static_cast<double>(left_tap(0, tap)) + left_tap(1, tap) + left_tap(2, tap)) / 3.0;
        const double right_mean =
            (static_cast<double>(right_tap(0, tap)) + right_tap(1, tap) + right_tap(2, tap))
            / 3.0;
        EXPECT_NEAR(pair.left[tap], left_mean, 1e-6) << "tap " << tap;
        EXPECT_NEAR(pair.right[tap], right_mean, 1e-6) << "tap " << tap;
    }
    EXPECT_NEAR(pair.itd_frames, (static_cast<double>(itd_of(0)) + itd_of(1) + itd_of(2)) / 3.0,
                1e-6);
}

TEST(Unit_Dsp, HrtfSetInterpolationWeightsSumToOne)
{
    // Each weight is a coordinate's magnitude over the sum. See tests/functional/unit/README.md.
    const SushiDSP::room::HrtfSet set = six_axis_set();
    std::uint32_t state = 12345u;
    auto next_unit = [&state]() {
        state = state * 1664525u + 1013904223u;
        return static_cast<double>(state) / 4294967296.0;
    };
    for (int trial = 0; trial < 500; ++trial)
    {
        const double azimuth = (2.0 * next_unit() - 1.0) * kPi;
        const double elevation = std::asin(2.0 * next_unit() - 1.0);
        const std::array<double, 3> v = unit_vector(azimuth, elevation);
        const std::array<std::uint32_t, 3> neighbours = { v[0] > 0.0 ? 0u : 3u,
                                                          v[1] > 0.0 ? 1u : 4u,
                                                          v[2] > 0.0 ? 2u : 5u };
        const float lowest = std::min({ left_tap(neighbours[0], 0), left_tap(neighbours[1], 0),
                                        left_tap(neighbours[2], 0) });
        const float highest = std::max({ left_tap(neighbours[0], 0), left_tap(neighbours[1], 0),
                                         left_tap(neighbours[2], 0) });
        const double sum = std::fabs(v[0]) + std::fabs(v[1]) + std::fabs(v[2]);
        const double expected = (std::fabs(v[0]) * neighbours[0] + std::fabs(v[1]) * neighbours[1]
                                 + std::fabs(v[2]) * neighbours[2]) / sum;

        const SushiDSP::room::HrtfSet::Pair pair = set.interpolate(azimuth, elevation);
        EXPECT_GE(pair.left[0], lowest) << "trial " << trial;
        EXPECT_LE(pair.left[0], highest) << "trial " << trial;
        EXPECT_NEAR(pair.left[0], expected, 1e-5) << "trial " << trial;
    }
}

TEST(Unit_Dsp, HrtfSetIsEmptyWhenDefaultConstructed)
{
    const SushiDSP::room::HrtfSet set;
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(set.direction_count(), 0u);
    EXPECT_THROW(set.interpolate(0.0, 0.0), std::runtime_error);
}

TEST(Unit_Dsp, HrtfSetKeepsItsDecoderAndDiffuseFilters)
{
    SushiDSP::room::HrtfSet set = six_axis_set();
    set.set_decoder(3, std::vector<float>(32 * SushiDSP::room::HrtfSet::kDecoderTaps));
    EXPECT_EQ(set.decoder_order(), 3u);
    EXPECT_EQ(set.decoder_filters().size(), 8192u);

    std::array<float, SushiDSP::room::HrtfSet::kDiffuseTaps> left{};
    std::array<float, SushiDSP::room::HrtfSet::kDiffuseTaps> right{};
    left[0] = 0.25f;
    right[127] = -0.5f;
    set.set_diffuse_field(left, right);
    EXPECT_EQ(set.diffuse_field(0)[0], 0.25f);
    EXPECT_EQ(set.diffuse_field(1)[127], -0.5f);
    EXPECT_DOUBLE_EQ(set.sample_rate(), 48000.0);
}

TEST(Unit_Dsp, HrtfSetRefusesTwoDirectionsCloserThanOneDegree)
{
    const auto half_pi = static_cast<float>(kPi / 2.0);
    const auto pi = static_cast<float>(kPi);
    const auto close = static_cast<float>(0.5 * kPi / 180.0);
    std::vector<SushiDSP::room::HrtfSet::Direction> directions = {
        { 0.0f, 0.0f, 0.0f },  { half_pi, 0.0f, 0.0f },  { 0.0f, half_pi, 0.0f },
        { pi, 0.0f, 0.0f },    { -half_pi, 0.0f, 0.0f }, { 0.0f, -half_pi, 0.0f },
        { close, 0.0f, 0.0f }
    };
    const std::size_t taps = directions.size() * SushiDSP::room::HrtfSet::kTapCount;
    EXPECT_THROW(SushiDSP::room::HrtfSet(48000.0, std::move(directions),
                                         std::vector<float>(taps, 0.0f),
                                         std::vector<float>(taps, 0.0f)),
                 std::invalid_argument);
}
