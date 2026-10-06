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
 * @file test_ambisonic_encoder.cpp
 * @brief Tests for AmbisonicEncoder against the closed-form ACN/SN3D harmonics.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "SushiDSP/room/AmbisonicEncoder.hpp"

namespace
{
    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Counts the Gauss-Legendre nodes in elevation, exact to degree 83 in sin(el).
    constexpr std::uint32_t kLatitudeNodes = 42;

    /// @brief Counts the equal azimuth steps, exact to harmonic 60 around the axis.
    constexpr std::uint32_t kAzimuthNodes = 61;

    /// @brief Holds one Gauss-Legendre node and its weight on [-1, 1].
    struct QuadratureNode
    {
        /// @brief Stores the abscissa, which is sin(el).
        double abscissa;

        /// @brief Stores the weight.
        double weight;
    };

    /// @brief Returns the Gauss-Legendre nodes and weights by Newton's method on P_n.
    std::vector<QuadratureNode> gauss_legendre(std::uint32_t count)
    {
        std::vector<QuadratureNode> nodes(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            double x = std::cos(kPi * (index + 0.75) / (count + 0.5));
            double derivative = 0.0;
            for (int iteration = 0; iteration < 100; ++iteration)
            {
                double previous = 1.0;
                double current = x;
                for (std::uint32_t degree = 2; degree <= count; ++degree)
                {
                    const double next = ((2.0 * degree - 1.0) * x * current
                                         - (degree - 1.0) * previous) / degree;
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
            nodes[index] = { x, 2.0 / ((1.0 - x * x) * derivative * derivative) };
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
}

TEST(Unit_Dsp, AmbisonicEncoderMatchesTheAcnSn3dHarmonicsInFront)
{
    std::array<float, 16> gains{};
    SushiDSP::room::AmbisonicEncoder::encode(0.0, 0.0, gains);
    const std::array<float, 16> expected = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, -0.5f, 0.0f, 0.866025f,
        0.0f, 0.0f, 0.0f, 0.0f, -0.612372f, 0.0f, 0.790569f };
    for (std::size_t channel = 0; channel < 16u; ++channel)
    {
        EXPECT_NEAR(gains[channel], expected[channel], 1e-5f) << "channel " << channel;
    }
}

TEST(Unit_Dsp, AmbisonicEncoderMatchesTheAcnSn3dHarmonicsToTheLeft)
{
    std::array<float, 16> gains{};
    SushiDSP::room::AmbisonicEncoder::encode(kPi / 2.0, 0.0, gains);
    const std::array<float, 16> expected = {
        1.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -0.5f, 0.0f, -0.866025f,
        -0.790569f, 0.0f, -0.612372f, 0.0f, 0.0f, 0.0f, 0.0f };
    for (std::size_t channel = 0; channel < 16u; ++channel)
    {
        EXPECT_NEAR(gains[channel], expected[channel], 1e-5f) << "channel " << channel;
    }
}

TEST(Unit_Dsp, AmbisonicEncoderKeepsChannelZeroAtOneEverywhere)
{
    std::uint32_t state = 2026u;
    auto next_unit = [&state]() {
        state = state * 1664525u + 1013904223u;
        return static_cast<double>(state) / 4294967296.0;
    };
    for (int trial = 0; trial < 200; ++trial)
    {
        const double azimuth = (2.0 * next_unit() - 1.0) * kPi;
        const double elevation = std::asin(2.0 * next_unit() - 1.0);
        std::array<float, 16> gains{};
        SushiDSP::room::AmbisonicEncoder::encode(azimuth, elevation, gains);
        EXPECT_EQ(gains[0], 1.0f) << "trial " << trial;
    }
}

TEST(Unit_Dsp, AmbisonicEncoderIsOrthonormalOverTheSphere)
{
    // The quadrature is exact here; the residual is rounding. See tests/functional/unit/README.md.
    const std::vector<QuadratureNode> latitudes = gauss_legendre(kLatitudeNodes);
    std::array<std::array<double, 16>, 16> mean{};
    for (const QuadratureNode& node : latitudes)
    {
        const double elevation = std::asin(node.abscissa);
        const double weight = node.weight / 2.0 / kAzimuthNodes;
        for (std::uint32_t step = 0; step < kAzimuthNodes; ++step)
        {
            std::array<float, 16> gains{};
            SushiDSP::room::AmbisonicEncoder::encode(2.0 * kPi * step / kAzimuthNodes, elevation,
                                                     gains);
            for (std::size_t row = 0; row < 16u; ++row)
            {
                for (std::size_t column = 0; column < 16u; ++column)
                {
                    mean[row][column] += weight * gains[row] * gains[column];
                }
            }
        }
    }
    for (std::uint32_t row = 0; row < 16u; ++row)
    {
        for (std::uint32_t column = 0; column < 16u; ++column)
        {
            const double expected = row == column ? 1.0 / (2.0 * order_of(row) + 1.0) : 0.0;
            EXPECT_NEAR(mean[row][column], expected, 1e-3) << row << ", " << column;
        }
    }
}
