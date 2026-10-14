/**************************************************************************/
/* test_spherical_head_hrtf.cpp                                           */
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
 * @file test_spherical_head_hrtf.cpp
 * @brief Tests for SphericalHeadHrtf, the rigid-sphere fallback set and Woodworth's ITD.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>

#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

namespace
{
    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the sample rate every set in this file is built at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Bounds how far two responses may drift and still count as equal.
    constexpr float K_TAP_TOLERANCE = 1.0e-6f;

    /// @brief Holds one ear's 128-tap response.
    using Taps = std::array<float, SushiDSP::room::HrtfSet::K_TAP_COUNT>;

    /// @brief Builds the set once for the whole file.
    const SushiDSP::room::HrtfSet& set()
    {
        static const SushiDSP::room::HrtfSet instance =
            SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        return instance;
    }

    /// @brief Returns the largest absolute difference between two responses, tap for tap.
    float max_difference(const Taps& a, const Taps& b)
    {
        float largest = 0.0f;
        for (std::size_t tap = 0; tap < a.size(); ++tap)
        {
            largest = std::max(largest, std::fabs(a[tap] - b[tap]));
        }
        return largest;
    }

    /// @brief Returns the energy of a response over one octave centred on centre_hz, summed
    /// over 200 evenly spaced frequencies of its DFT.
    double octave_energy(const Taps& taps, double centre_hz)
    {
        const double low = centre_hz / std::sqrt(2.0);
        const double high = centre_hz * std::sqrt(2.0);
        const double step = (high - low) / 200.0;
        double energy = 0.0;
        for (double hz = low; hz < high; hz += step)
        {
            double real = 0.0;
            double imag = 0.0;
            for (std::size_t tap = 0; tap < taps.size(); ++tap)
            {
                const double angle = 2.0 * K_PI * hz * static_cast<double>(tap) / K_SAMPLE_RATE;
                real += taps[tap] * std::cos(angle);
                imag -= taps[tap] * std::sin(angle);
            }
            energy += real * real + imag * imag;
        }
        return energy;
    }

    /// @brief Returns how many decibels the near ear sits above the far ear in one octave.
    double interaural_level_db(const SushiDSP::room::HrtfSet::Pair& pair, double centre_hz)
    {
        return 10.0 * std::log10(octave_energy(pair.left, centre_hz) /
                                 octave_energy(pair.right, centre_hz));
    }

    /// @brief Returns the squared magnitude of a response's DFT at one frequency.
    double power_at(const Taps& taps, double hz)
    {
        double real = 0.0;
        double imag = 0.0;
        for (std::size_t tap = 0; tap < taps.size(); ++tap)
        {
            const double angle = 2.0 * K_PI * hz * static_cast<double>(tap) / K_SAMPLE_RATE;
            real += taps[tap] * std::cos(angle);
            imag -= taps[tap] * std::sin(angle);
        }
        return real * real + imag * imag;
    }

    /// @brief Returns the sum of a response's squared taps, its mean squared magnitude over
    /// the whole band by Parseval.
    double energy(const Taps& taps)
    {
        double sum = 0.0;
        for (const float tap : taps)
        {
            sum += static_cast<double>(tap) * tap;
        }
        return sum;
    }
} // namespace

TEST(SphericalHeadHrtf, SphericalHeadHrtfMatchesWoodworthsInterauralDelay)
{
    // (a / c) * (theta + sin theta) with a = 0.0875 m and c = 343 m/s.
    EXPECT_NEAR(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(K_PI / 6.0),
                0.0002611, 2e-6);
    EXPECT_NEAR(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(K_PI / 3.0),
                0.0004881, 2e-6);
    EXPECT_NEAR(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(K_PI / 2.0),
                0.0006558, 2e-6);
}

TEST(SphericalHeadHrtf, SphericalHeadHrtfPutsZeroDelayStraightAhead)
{
    EXPECT_DOUBLE_EQ(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(0.0), 0.0);
    const SushiDSP::room::HrtfSet::Pair front = set().interpolate(0.0, 0.0);
    EXPECT_EQ(front.itd_frames, 0.0f);
}

TEST(SphericalHeadHrtf, SphericalHeadHrtfIsSymmetricAboutTheMedianPlane)
{
    const SushiDSP::room::HrtfSet::Pair left = set().interpolate(K_PI / 3.0, 0.0);
    const SushiDSP::room::HrtfSet::Pair right = set().interpolate(-K_PI / 3.0, 0.0);
    EXPECT_LE(max_difference(left.left, right.right), K_TAP_TOLERANCE);
    EXPECT_LE(max_difference(left.right, right.left), K_TAP_TOLERANCE);
    EXPECT_NEAR(left.itd_frames, -right.itd_frames, K_TAP_TOLERANCE);
    EXPECT_GT(left.itd_frames, 0.0f);
}

TEST(SphericalHeadHrtf, SphericalHeadHrtfCannotTellFrontFromBack)
{
    // This is the model's documented limit and the reason the measured asset exists.
    const SushiDSP::room::HrtfSet::Pair front = set().interpolate(0.0, 0.0);
    const SushiDSP::room::HrtfSet::Pair back = set().interpolate(K_PI, 0.0);
    EXPECT_LE(max_difference(front.left, back.left), K_TAP_TOLERANCE);
    EXPECT_LE(max_difference(front.right, back.right), K_TAP_TOLERANCE);
    EXPECT_NEAR(front.itd_frames, back.itd_frames, K_TAP_TOLERANCE);
}

TEST(SphericalHeadHrtf, SphericalHeadHrtfShadowsTheFarEarAboveOneKilohertz)
{
    // The far ear sits at the sphere's bright spot. See tests/nodes/room/README.md.
    const SushiDSP::room::HrtfSet::Pair side = set().interpolate(K_PI / 2.0, 0.0);
    EXPECT_GE(interaural_level_db(side, 4000.0), 4.0);
    EXPECT_LE(std::fabs(interaural_level_db(side, 125.0)), 2.0);
    double below = interaural_level_db(side, 125.0);
    for (const double centre_hz : {250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0})
    {
        const double difference = interaural_level_db(side, centre_hz);
        EXPECT_GT(difference, below) << "octave at " << centre_hz << " Hz";
        below = difference;
    }
}

TEST(SphericalHeadHrtf, SphericalHeadHrtfBuildsAGridThatCoversTheSphere)
{
    EXPECT_EQ(set().direction_count(), 614u);
    std::mt19937 generator(7);
    std::uniform_real_distribution<double> azimuth(-K_PI, K_PI);
    std::uniform_real_distribution<double> elevation(-K_PI / 2.0, K_PI / 2.0);
    for (int trial = 0; trial < 500; ++trial)
    {
        const SushiDSP::room::HrtfSet::Pair pair =
            set().interpolate(azimuth(generator), elevation(generator));
        ASSERT_TRUE(std::isfinite(pair.left[0]));
        ASSERT_TRUE(std::isfinite(pair.right[0]));
        ASSERT_TRUE(std::isfinite(pair.itd_frames));
    }
}

TEST(SphericalHeadHrtf, SphericalHeadHrtfFillsTheDiffuseFieldPair)
{
    for (const std::uint32_t ear : {0u, 1u})
    {
        const Taps& taps = set().diffuse_field(ear);
        EXPECT_GT(energy(taps), 0.5) << "ear " << ear;
        for (const float tap : taps)
        {
            ASSERT_TRUE(std::isfinite(tap)) << "ear " << ear;
        }
    }
}

TEST(SphericalHeadHrtf, SphericalHeadHrtfHearsTheSameDiffuseFieldAtBothEars)
{
    // The grid mirrors about the median plane. See tests/nodes/room/README.md.
    EXPECT_LE(max_difference(set().diffuse_field(0), set().diffuse_field(1)), K_TAP_TOLERANCE);
}

TEST(SphericalHeadHrtf, SphericalHeadHrtfDiffuseFieldCarriesTheRigidSpheresDiffuseGain)
{
    // The sphere's mean diffuse power is 1.8472, +2.67 dB. See tests/nodes/room/README.md.
    for (const std::uint32_t ear : {0u, 1u})
    {
        const Taps& taps = set().diffuse_field(ear);
        EXPECT_NEAR(power_at(taps, 0.0), 1.0, 2.0e-3) << "ear " << ear;
        EXPECT_NEAR(power_at(taps, 4000.0), 1.7765, 0.02) << "ear " << ear;
        EXPECT_NEAR(energy(taps), 1.847, 0.01) << "ear " << ear;
    }
}
