/**************************************************************************/
/* test_spherical_head_hrtf.cpp                                           */
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

#include "SushiDSP/room/SphericalHeadHrtf.hpp"

namespace
{
    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Holds the sample rate every set in this file is built at.
    constexpr double kSampleRate = 48000.0;

    /// @brief Bounds how far two responses may drift and still count as equal.
    constexpr float kTapTolerance = 1.0e-6f;

    /// @brief Holds one ear's 128-tap response.
    using Taps = std::array<float, SushiDSP::room::HrtfSet::kTapCount>;

    /// @brief Builds the set once for the whole file.
    const SushiDSP::room::HrtfSet& set()
    {
        static const SushiDSP::room::HrtfSet instance =
            SushiDSP::room::SphericalHeadHrtf::build(kSampleRate);
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
                const double angle = 2.0 * kPi * hz * static_cast<double>(tap) / kSampleRate;
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
        return 10.0 * std::log10(octave_energy(pair.left, centre_hz)
                                 / octave_energy(pair.right, centre_hz));
    }
}

TEST(Unit_Dsp, SphericalHeadHrtfMatchesWoodworthsInterauralDelay)
{
    // (a / c) * (theta + sin theta) with a = 0.0875 m and c = 343 m/s.
    EXPECT_NEAR(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(kPi / 6.0),
                0.0002611, 2e-6);
    EXPECT_NEAR(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(kPi / 3.0),
                0.0004881, 2e-6);
    EXPECT_NEAR(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(kPi / 2.0),
                0.0006558, 2e-6);
}

TEST(Unit_Dsp, SphericalHeadHrtfPutsZeroDelayStraightAhead)
{
    EXPECT_DOUBLE_EQ(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(0.0), 0.0);
    const SushiDSP::room::HrtfSet::Pair front = set().interpolate(0.0, 0.0);
    EXPECT_EQ(front.itd_frames, 0.0f);
}

TEST(Unit_Dsp, SphericalHeadHrtfIsSymmetricAboutTheMedianPlane)
{
    const SushiDSP::room::HrtfSet::Pair left = set().interpolate(kPi / 3.0, 0.0);
    const SushiDSP::room::HrtfSet::Pair right = set().interpolate(-kPi / 3.0, 0.0);
    EXPECT_LE(max_difference(left.left, right.right), kTapTolerance);
    EXPECT_LE(max_difference(left.right, right.left), kTapTolerance);
    EXPECT_NEAR(left.itd_frames, -right.itd_frames, kTapTolerance);
    EXPECT_GT(left.itd_frames, 0.0f);
}

TEST(Unit_Dsp, SphericalHeadHrtfCannotTellFrontFromBack)
{
    // This is the model's documented limit and the reason the measured asset exists.
    const SushiDSP::room::HrtfSet::Pair front = set().interpolate(0.0, 0.0);
    const SushiDSP::room::HrtfSet::Pair back = set().interpolate(kPi, 0.0);
    EXPECT_LE(max_difference(front.left, back.left), kTapTolerance);
    EXPECT_LE(max_difference(front.right, back.right), kTapTolerance);
    EXPECT_NEAR(front.itd_frames, back.itd_frames, kTapTolerance);
}

TEST(Unit_Dsp, SphericalHeadHrtfShadowsTheFarEarAboveOneKilohertz)
{
    // With the ears on the axis, the far ear sits at the sphere's bright spot, so the level
    // difference at 90 degrees is the near ear's own rise: 4.7 dB in the 4 kHz octave.
    const SushiDSP::room::HrtfSet::Pair side = set().interpolate(kPi / 2.0, 0.0);
    EXPECT_GE(interaural_level_db(side, 4000.0), 4.0);
    EXPECT_LE(std::fabs(interaural_level_db(side, 125.0)), 2.0);
}

TEST(Unit_Dsp, SphericalHeadHrtfBuildsAGridThatCoversTheSphere)
{
    EXPECT_EQ(set().direction_count(), 614u);
    std::mt19937 generator(7);
    std::uniform_real_distribution<double> azimuth(-kPi, kPi);
    std::uniform_real_distribution<double> elevation(-kPi / 2.0, kPi / 2.0);
    for (int trial = 0; trial < 500; ++trial)
    {
        const SushiDSP::room::HrtfSet::Pair pair =
            set().interpolate(azimuth(generator), elevation(generator));
        ASSERT_TRUE(std::isfinite(pair.left[0]));
        ASSERT_TRUE(std::isfinite(pair.right[0]));
        ASSERT_TRUE(std::isfinite(pair.itd_frames));
    }
}
