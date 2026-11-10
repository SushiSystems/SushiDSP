/**************************************************************************/
/* test_ambisonic_encoder.cpp                                             */
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
 * @file test_ambisonic_encoder.cpp
 * @brief Tests for AmbisonicEncoder against the closed-form ACN/SN3D harmonics.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room/ambisonic_encoder.hpp>

#include <support/tolerance.hpp>

namespace
{
    using Gains = std::array<float, 16>;
    using Gram = std::array<std::array<double, 16>, 16>;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Counts the Gauss-Legendre nodes in elevation, exact to degree 83 in sin(el).
    constexpr std::uint32_t K_LATITUDE_NODES = 42;

    /// @brief Counts the equal azimuth steps, exact to harmonic 60 around the axis.
    constexpr std::uint32_t K_AZIMUTH_NODES = 61;

    /// @brief Bounds the absolute error of the double trigonometry behind a gain: a few
    /// units in the last place of values at most one, 1e-15 with room to spare.
    constexpr double K_TRIGONOMETRY_BOUND = 1.0e-15;

    /// @brief Bounds a Gram entry. The encoder rounds each gain to float once, so a product of
    /// two gains moves by at most 2 u_f times its size; the mean of |g_i g_j| over the sphere is
    /// at most the square root of the product of two mean squares, each at most one, and the
    /// quadrature is exact for this degree, so 2 u_f bounds every entry.
    constexpr double K_GRAM_BOUND = 2.0 * TestSupport::unit_roundoff<float>();

    /// @brief Holds one Gauss-Legendre node and its weight on [-1, 1].
    struct QuadratureNode
    {
            /// @brief Stores the abscissa, which is sin(el).
            double abscissa;

            /// @brief Stores the weight.
            double weight;
    };

    /// @brief Holds one direction with the sixteen coefficients its closed form gives.
    struct HarmonicsRow
    {
            const char* name;
            double azimuth_rad;
            double elevation_rad;
            std::array<double, 16> expected;
    };

    /// @brief Holds one direction.
    struct DirectionRow
    {
            const char* name;
            double azimuth_rad;
            double elevation_rad;
    };

    /// @brief Names a row for gtest.
    template <typename Row>
    std::string row_name(const ::testing::TestParamInfo<Row>& info)
    {
        return info.param.name;
    }

    /// @brief Returns the Gauss-Legendre nodes and weights by Newton's method on P_n.
    std::vector<QuadratureNode> gauss_legendre(std::uint32_t count)
    {
        std::vector<QuadratureNode> nodes(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            double x = std::cos(K_PI * (index + 0.75) / (count + 0.5));
            double derivative = 0.0;
            for (int iteration = 0; iteration < 100; ++iteration)
            {
                double previous = 1.0;
                double current = x;
                for (std::uint32_t degree = 2; degree <= count; ++degree)
                {
                    const double next =
                        ((2.0 * degree - 1.0) * x * current - (degree - 1.0) * previous) / degree;
                    previous = current;
                    current = next;
                }
                derivative = count * (x * current - previous) / (x * x - 1.0);
                const double step = current / derivative;
                x -= step;
                if (std::fabs(step) < 1e-15)
                {
                    break;
                }
            }
            nodes[index] = {x, 2.0 / ((1.0 - x * x) * derivative * derivative)};
        }
        return nodes;
    }

    /// @brief Returns the order n of an ACN channel: floor(sqrt(acn)).
    std::uint32_t order_of(std::uint32_t acn)
    {
        std::uint32_t order = 0;
        while ((order + 1) * (order + 1) <= acn)
        {
            ++order;
        }
        return order;
    }

    /// @brief Returns the mean over the sphere of the product of every two encoded gains.
    Gram integrate_gram()
    {
        const std::vector<QuadratureNode> latitudes = gauss_legendre(K_LATITUDE_NODES);
        Gram mean{};
        for (const QuadratureNode& node : latitudes)
        {
            const double elevation = std::asin(node.abscissa);
            const double weight = node.weight / 2.0 / K_AZIMUTH_NODES;
            for (std::uint32_t step = 0; step < K_AZIMUTH_NODES; ++step)
            {
                Gains gains{};
                SushiDSP::room::AmbisonicEncoder::encode(2.0 * K_PI * step / K_AZIMUTH_NODES,
                                                         elevation, gains);
                for (std::size_t row = 0; row < 16u; ++row)
                {
                    for (std::size_t column = 0; column < 16u; ++column)
                    {
                        mean[row][column] += weight * gains[row] * gains[column];
                    }
                }
            }
        }
        return mean;
    }

    /// @brief Expects every coefficient of @p gains within a float rounding of @p expected.
    void expect_harmonics(const Gains& gains, const std::array<double, 16>& expected)
    {
        for (std::size_t channel = 0; channel < 16u; ++channel)
        {
            const double bound =
                TestSupport::unit_roundoff<float>() * std::fabs(expected[channel]) + K_TRIGONOMETRY_BOUND;
            EXPECT_TRUE(TestSupport::is_within(
                TestSupport::measure_error(gains[channel], expected[channel], bound)))
                << "channel " << channel;
        }
    }

    /// @brief Expects each diagonal entry of @p gram to be 1 / (2n + 1) for its order n.
    void expect_gram_diagonal(const Gram& gram)
    {
        for (std::uint32_t index = 0; index < 16u; ++index)
        {
            const double expected = 1.0 / (2.0 * order_of(index) + 1.0);
            EXPECT_TRUE(TestSupport::is_within(
                TestSupport::measure_error(gram[index][index], expected, K_GRAM_BOUND)))
                << index;
        }
    }

    /// @brief Expects each entry of @p gram off the diagonal to be zero.
    void expect_gram_off_diagonal(const Gram& gram)
    {
        for (std::uint32_t row = 0; row < 16u; ++row)
        {
            for (std::uint32_t column = 0; column < 16u; ++column)
            {
                if (row != column)
                {
                    EXPECT_TRUE(TestSupport::is_within(
                        TestSupport::measure_error(gram[row][column], 0.0, K_GRAM_BOUND)))
                        << row << ", " << column;
                }
            }
        }
    }

    class Small_AmbisonicEncoderHarmonics : public ::testing::TestWithParam<HarmonicsRow>
    {
    };

    class Small_AmbisonicEncoderDirection : public ::testing::TestWithParam<DirectionRow>
    {
    };
} // namespace

TEST_P(Small_AmbisonicEncoderHarmonics, AmbisonicEncoderMatchesTheAcnSn3dHarmonics)
{
    const HarmonicsRow row = GetParam();
    Gains gains{};

    SushiDSP::room::AmbisonicEncoder::encode(row.azimuth_rad, row.elevation_rad, gains);

    expect_harmonics(gains, row.expected);
}

/*
 * Closed forms at x = cos(az) cos(el), y = sin(az) cos(el), z = sin(el): in front x = 1, to the
 * left y = 1. The order-two and order-three entries are 3z^2 - 1 over 2 = -1/2, sqrt(3)/2 (x^2 - y^2),
 * sqrt(5/8) y (3x^2 - y^2), sqrt(3/8) y (5z^2 - 1), sqrt(3/8) x (5z^2 - 1) and
 * sqrt(5/8) x (x^2 - 3y^2).
 */
INSTANTIATE_TEST_SUITE_P(
    Directions, Small_AmbisonicEncoderHarmonics,
    ::testing::Values(
        HarmonicsRow{"InFront",
                     0.0,
                     0.0,
                     {1.0, 0.0, 0.0, 1.0, 0.0, 0.0, -0.5, 0.0, std::sqrt(3.0) / 2.0, 0.0, 0.0, 0.0, 0.0,
                      -std::sqrt(3.0 / 8.0), 0.0, std::sqrt(5.0 / 8.0)}},
        HarmonicsRow{"ToTheLeft",
                     K_PI / 2.0,
                     0.0,
                     {1.0, 1.0, 0.0, 0.0, 0.0, 0.0, -0.5, 0.0, -std::sqrt(3.0) / 2.0,
                      -std::sqrt(5.0 / 8.0), 0.0, -std::sqrt(3.0 / 8.0), 0.0, 0.0, 0.0, 0.0}}),
    row_name<HarmonicsRow>);

TEST_P(Small_AmbisonicEncoderDirection, AmbisonicEncoderKeepsChannelZeroAtOne)
{
    const DirectionRow row = GetParam();
    Gains gains{};

    SushiDSP::room::AmbisonicEncoder::encode(row.azimuth_rad, row.elevation_rad, gains);

    EXPECT_EQ(gains[0], 1.0f);
}

INSTANTIATE_TEST_SUITE_P(Directions, Small_AmbisonicEncoderDirection,
                         ::testing::Values(DirectionRow{"InFront", 0.0, 0.0},
                                           DirectionRow{"ToTheLeft", K_PI / 2.0, 0.0},
                                           DirectionRow{"Behind", K_PI, 0.0},
                                           DirectionRow{"Above", 0.0, K_PI / 2.0},
                                           DirectionRow{"Below", 1.0, -K_PI / 2.0},
                                           DirectionRow{"OffAxis", -2.5, 0.7}),
                         row_name<DirectionRow>);

TEST(Small_AmbisonicEncoder, AmbisonicEncoderSelfProductsAreOneOverTwoOrderPlusOneOverTheSphere)
{
    // The quadrature is exact here; the residual is rounding. See tests/nodes/room/README.md.
    expect_gram_diagonal(integrate_gram());
}

TEST(Small_AmbisonicEncoder, AmbisonicEncoderDifferentChannelsAreOrthogonalOverTheSphere)
{
    expect_gram_off_diagonal(integrate_gram());
}
