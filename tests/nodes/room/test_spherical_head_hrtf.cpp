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
 * @file test_spherical_head_hrtf.cpp
 * @brief Tests for SphericalHeadHrtf, the rigid-sphere fallback set and Woodworth's ITD.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <string>

#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

#include "support/tolerance.hpp"

namespace
{
    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Holds the sample rate every set in this file is built at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Holds the ratio of error to bound above which a derived check fails.
    constexpr double K_LIMIT = TestSupport::K_DEFAULT_THRESHOLD;

    /// @brief Counts the rounded operations that make one tap of an interpolated response: three
    /// weights times three taps and the two sums that join them, rounded to the next float.
    constexpr std::size_t K_INTERPOLATION_OPERATIONS = 12;

    /// @brief Holds half a unit of the seventh decimal, the most a delay written to seven decimals
    /// can sit from the exact value.
    constexpr double K_HALF_UNIT_OF_SEVENTH_DECIMAL = 0.5e-7;

    /// @brief Holds the largest interaural delay of the head in frames, Woodworth's figure at 90
    /// degrees, which bounds the rounding of a delay in any direction.
    constexpr double K_MAX_ITD_FRAMES = 31.479;

    /// @brief Holds the level difference the far ear's shadow must reach in the 4 kHz octave, in
    /// decibels.
    constexpr double K_SHADOW_AT_FOUR_KILOHERTZ_DB = 4.0;

    /// @brief Holds the level difference the two ears may show in the 125 Hz octave, in decibels.
    constexpr double K_LOW_OCTAVE_DIFFERENCE_DB = 2.0;

    /// @brief Holds the energy a diffuse-field response must carry at the least.
    constexpr double K_MIN_DIFFUSE_ENERGY = 0.5;

    /// @brief Holds how far the diffuse power at DC may sit from the sphere's 1. Characterised,
    /// not derived: the grid weights solid angle by 10 degree cells.
    constexpr double K_DC_POWER_TOLERANCE = 2.0e-3;

    /// @brief Holds how far the diffuse power at 4 kHz may sit from the sphere's 1.7765.
    /// Characterised, not derived.
    constexpr double K_FOUR_KILOHERTZ_POWER_TOLERANCE = 0.02;

    /// @brief Holds how far the diffuse energy may sit from the sphere's mean power of 1.847.
    /// Characterised, not derived.
    constexpr double K_DIFFUSE_ENERGY_TOLERANCE = 0.01;

    /// @brief Holds how many directions the coverage tests draw, seeded.
    constexpr int K_COVERAGE_TRIALS = 500;

    /// @brief Holds the step of the grid in degrees, in azimuth and in elevation.
    constexpr double K_GRID_STEP_DEGREES = 10.0;

    /// @brief Holds the largest elevation ring, 80 degrees, on either side of the equator.
    constexpr int K_LAST_RING_DEGREES = 80;

    /// @brief Holds the angle between a point and the nearest of four grid corners at most, when
    /// the point is the centre of the widest cell: the cell at the equator is 10 degrees wide and
    /// 10 degrees tall, so the centre is 5 degrees from each side and the great circle to a
    /// corner has cos d = cos(5 degrees)^2. A cell off the equator is narrower, and the cap
    /// between the 80 degree ring and the pole is at most 5 degrees across.
    const double K_WIDEST_CELL_HALF_DIAGONAL_RAD =
        std::acos(std::cos(K_PI / 36.0) * std::cos(K_PI / 36.0));

    /// @brief Holds the angle a direction stored as floats may sit from the angle it was written
    /// from: sixteen roundings of an angle as large as pi.
    const double K_STORED_ANGLE_SLACK_RAD =
        16.0 * TestSupport::unit_roundoff<float>() * K_PI;

    /// @brief Holds one ear's 128-tap response.
    using Taps = std::array<float, SushiDSP::room::HrtfSet::K_TAP_COUNT>;

    /// @brief Builds the set once for the whole file.
    const SushiDSP::room::HrtfSet& set()
    {
        static const SushiDSP::room::HrtfSet instance =
            SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        return instance;
    }

    /// @brief Returns the largest absolute tap of a response.
    float peak(const Taps& taps)
    {
        float largest = 0.0f;
        for (const float tap : taps)
        {
            largest = std::max(largest, std::fabs(tap));
        }
        return largest;
    }

    /// @brief Returns how many times its bound the largest tap difference of two responses
    /// reads, the bound being the given number of float roundings of the larger peak.
    double tap_ratio(const Taps& a, const Taps& b, std::size_t operations)
    {
        float largest = 0.0f;
        for (std::size_t tap = 0; tap < a.size(); ++tap)
        {
            largest = std::max(largest, std::fabs(a[tap] - b[tap]));
        }
        const double bound = static_cast<double>(operations) *
                             TestSupport::unit_roundoff<float>() *
                             static_cast<double>(std::max(peak(a), peak(b)));
        return TestSupport::measure_error(largest, 0.0, bound).ratio;
    }

    /// @brief Returns how many times its bound the difference of two delays reads, the bound
    /// being the given number of float roundings of the largest delay there is.
    double delay_ratio(double a, double b, std::size_t operations)
    {
        const double bound = static_cast<double>(operations) *
                             TestSupport::unit_roundoff<float>() * K_MAX_ITD_FRAMES;
        return TestSupport::measure_error(a, b, bound).ratio;
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
            energy += power_at(taps, hz);
        }
        return energy;
    }

    /// @brief Returns how many decibels the near ear sits above the far ear in one octave.
    double interaural_level_db(const SushiDSP::room::HrtfSet::Pair& pair, double centre_hz)
    {
        return 10.0 * std::log10(octave_energy(pair.left, centre_hz) /
                                 octave_energy(pair.right, centre_hz));
    }

    /// @brief Returns the level difference at the side, ninety degrees from the nose.
    double side_level_db(double centre_hz)
    {
        return interaural_level_db(set().interpolate(K_PI / 2.0, 0.0), centre_hz);
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

    /// @brief Returns whether every tap of a response is finite.
    bool all_finite(const Taps& taps)
    {
        return std::all_of(taps.begin(), taps.end(), [](float tap) { return std::isfinite(tap); });
    }

    /// @brief Holds the directions the coverage tests read, drawn once from a fixed seed.
    std::array<std::array<double, 2>, K_COVERAGE_TRIALS> seeded_directions()
    {
        std::mt19937 generator(7);
        std::uniform_real_distribution<double> azimuth(-K_PI, K_PI);
        std::uniform_real_distribution<double> elevation(-K_PI / 2.0, K_PI / 2.0);
        std::array<std::array<double, 2>, K_COVERAGE_TRIALS> directions{};
        for (std::array<double, 2>& direction : directions)
        {
            direction = {azimuth(generator), elevation(generator)};
        }
        return directions;
    }

    /// @brief Returns whether every seeded direction gives finite responses and a finite delay.
    bool every_direction_is_finite()
    {
        const auto directions = seeded_directions();
        return std::all_of(directions.begin(), directions.end(),
                           [](const std::array<double, 2>& direction)
                           {
                               const SushiDSP::room::HrtfSet::Pair pair =
                                   set().interpolate(direction[0], direction[1]);
                               return all_finite(pair.left) && all_finite(pair.right) &&
                                      std::isfinite(pair.itd_frames);
                           });
    }

    /// @brief Returns the least energy either ear's response carries over the seeded directions.
    double least_energy_over_directions()
    {
        double least = std::numeric_limits<double>::infinity();
        for (const std::array<double, 2>& direction : seeded_directions())
        {
            const SushiDSP::room::HrtfSet::Pair pair = set().interpolate(direction[0], direction[1]);
            least = std::min(least, std::min(energy(pair.left), energy(pair.right)));
        }
        return least;
    }

    /// @brief Returns the unit vector of an azimuth and an elevation, x ahead, y left, z up.
    std::array<double, 3> unit_vector(double azimuth_rad, double elevation_rad)
    {
        return {std::cos(elevation_rad) * std::cos(azimuth_rad),
                std::cos(elevation_rad) * std::sin(azimuth_rad), std::sin(elevation_rad)};
    }

    /// @brief Returns the angle in radians between two unit vectors, from the chord between them.
    double angle_between(const std::array<double, 3>& a, const std::array<double, 3>& b)
    {
        const double dx = a[0] - b[0];
        const double dy = a[1] - b[1];
        const double dz = a[2] - b[2];
        return 2.0 * std::asin(0.5 * std::sqrt(dx * dx + dy * dy + dz * dz));
    }

    /// @brief Returns the angle from a direction to the nearest direction the set holds.
    double angle_to_the_nearest_grid_direction(const std::array<double, 3>& vector)
    {
        double nearest = K_PI;
        for (std::uint32_t index = 0; index < set().direction_count(); ++index)
        {
            const SushiDSP::room::HrtfSet::Direction& grid = set().direction(index);
            nearest = std::min(nearest,
                               angle_between(vector, unit_vector(grid.azimuth_rad, grid.elevation_rad)));
        }
        return nearest;
    }

    /// @brief Returns the largest angle from a seeded direction to the nearest grid direction.
    double largest_gap_over_seeded_directions()
    {
        double largest = 0.0;
        for (const std::array<double, 2>& direction : seeded_directions())
        {
            largest = std::max(largest, angle_to_the_nearest_grid_direction(
                                            unit_vector(direction[0], direction[1])));
        }
        return largest;
    }

    /// @brief Returns how many cells of the grid worked out here, a ring of 36 azimuths at every
    /// 10 degrees of elevation from -80 to 80 and the two poles, have no direction of the set at
    /// them.
    std::size_t grid_cells_the_set_misses()
    {
        std::size_t missed = 0;
        for (int ring = -K_LAST_RING_DEGREES; ring <= K_LAST_RING_DEGREES;
             ring += static_cast<int>(K_GRID_STEP_DEGREES))
        {
            for (int step = 0; step < 36; ++step)
            {
                const double azimuth = static_cast<double>(step) * K_GRID_STEP_DEGREES * K_PI / 180.0;
                const double elevation = static_cast<double>(ring) * K_PI / 180.0;
                missed += angle_to_the_nearest_grid_direction(unit_vector(azimuth, elevation)) >
                                  K_STORED_ANGLE_SLACK_RAD
                              ? 1u
                              : 0u;
            }
        }
        for (const double pole : {-K_PI / 2.0, K_PI / 2.0})
        {
            missed += angle_to_the_nearest_grid_direction(unit_vector(0.0, pole)) >
                              K_STORED_ANGLE_SLACK_RAD
                          ? 1u
                          : 0u;
        }
        return missed;
    }

    /// @brief Holds one azimuth and the interaural delay worked out by hand to seven decimals:
    /// (a / c) (theta + sin theta) with a = 0.0875 m and c = 343 m/s.
    struct Delay
    {
            const char* name;
            double azimuth_rad;
            double seconds;
    };

    /// @brief Holds the delays at 30, 60 and 90 degrees: 0.00025510 times 1.0235988, 1.9132230
    /// and 2.5707963.
    const std::array<Delay, 3> kDelays = {{{"ThirtyDegrees", K_PI / 6.0, 0.0002611},
                                           {"SixtyDegrees", K_PI / 3.0, 0.0004881},
                                           {"NinetyDegrees", K_PI / 2.0, 0.0006558}}};

    /// @brief Names a delay case after its azimuth.
    std::string delay_name(const ::testing::TestParamInfo<Delay>& info)
    {
        return info.param.name;
    }

    /// @brief Holds one octave centre and the one below it, whose level difference it must exceed.
    struct Octave
    {
            const char* name;
            double centre_hz;
            double below_hz;
    };

    /// @brief Holds the octaves from 250 Hz to 8 kHz, each against the one below.
    const std::array<Octave, 6> kOctaves = {{{"Hz250", 250.0, 125.0},
                                             {"Hz500", 500.0, 250.0},
                                             {"Hz1000", 1000.0, 500.0},
                                             {"Hz2000", 2000.0, 1000.0},
                                             {"Hz4000", 4000.0, 2000.0},
                                             {"Hz8000", 8000.0, 4000.0}}};

    /// @brief Names an octave case after its centre.
    std::string octave_name(const ::testing::TestParamInfo<Octave>& info)
    {
        return info.param.name;
    }

    /// @brief Names an ear case after its index.
    std::string ear_name(const ::testing::TestParamInfo<std::uint32_t>& info)
    {
        return info.param == 0u ? "Left" : "Right";
    }
} // namespace

/// @brief Builds the set before the first case of the suite, so the 2 s the build takes in a debug
/// build is not charged to a case. The set is built once for the process.
class Small_SphericalHeadHrtf : public ::testing::Test
{
    protected:
        static void SetUpTestSuite()
        {
            set();
        }
};

class Small_SphericalHeadHrtfDelay : public ::testing::TestWithParam<Delay>
{
    protected:
        static void SetUpTestSuite()
        {
            set();
        }
};

class Small_SphericalHeadHrtfShadow : public ::testing::TestWithParam<Octave>
{
    protected:
        static void SetUpTestSuite()
        {
            set();
        }
};

class Small_SphericalHeadHrtfDiffuse : public ::testing::TestWithParam<std::uint32_t>
{
    protected:
        static void SetUpTestSuite()
        {
            set();
        }
};

TEST_P(Small_SphericalHeadHrtfDelay, MatchesWoodworthsInterauralDelay)
{
    // (a / c) * (theta + sin theta) with a = 0.0875 m and c = 343 m/s.
    EXPECT_NEAR(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(GetParam().azimuth_rad),
                GetParam().seconds, K_HALF_UNIT_OF_SEVENTH_DECIMAL);
}

INSTANTIATE_TEST_SUITE_P(Azimuths, Small_SphericalHeadHrtfDelay, ::testing::ValuesIn(kDelays),
                         delay_name);

TEST_F(Small_SphericalHeadHrtf, SphericalHeadHrtfPutsZeroDelayStraightAhead)
{
    EXPECT_DOUBLE_EQ(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(0.0), 0.0);
    const SushiDSP::room::HrtfSet::Pair front = set().interpolate(0.0, 0.0);
    EXPECT_EQ(front.itd_frames, 0.0f);
}

TEST_F(Small_SphericalHeadHrtf, ResponsesAreSymmetricAboutTheMedianPlane)
{
    const SushiDSP::room::HrtfSet::Pair left = set().interpolate(K_PI / 3.0, 0.0);
    const SushiDSP::room::HrtfSet::Pair right = set().interpolate(-K_PI / 3.0, 0.0);
    EXPECT_LE(tap_ratio(left.left, right.right, K_INTERPOLATION_OPERATIONS), K_LIMIT);
    EXPECT_LE(tap_ratio(left.right, right.left, K_INTERPOLATION_OPERATIONS), K_LIMIT);
}

TEST_F(Small_SphericalHeadHrtf, DelaysAreOppositeAboutTheMedianPlane)
{
    const SushiDSP::room::HrtfSet::Pair left = set().interpolate(K_PI / 3.0, 0.0);
    const SushiDSP::room::HrtfSet::Pair right = set().interpolate(-K_PI / 3.0, 0.0);
    EXPECT_LE(delay_ratio(left.itd_frames, -right.itd_frames, K_INTERPOLATION_OPERATIONS), K_LIMIT);
}

TEST_F(Small_SphericalHeadHrtf, SourceToTheLeftArrivesAtTheRightEarLater)
{
    EXPECT_GT(set().interpolate(K_PI / 3.0, 0.0).itd_frames, 0.0f);
}

TEST_F(Small_SphericalHeadHrtf, SphericalHeadHrtfCannotTellFrontFromBack)
{
    // This is the model's documented limit and the reason the measured asset exists.
    const SushiDSP::room::HrtfSet::Pair front = set().interpolate(0.0, 0.0);
    const SushiDSP::room::HrtfSet::Pair back = set().interpolate(K_PI, 0.0);
    EXPECT_LE(tap_ratio(front.left, back.left, K_INTERPOLATION_OPERATIONS), K_LIMIT);
    EXPECT_LE(tap_ratio(front.right, back.right, K_INTERPOLATION_OPERATIONS), K_LIMIT);
}

TEST_F(Small_SphericalHeadHrtf, FrontAndBackHaveTheSameDelay)
{
    const SushiDSP::room::HrtfSet::Pair front = set().interpolate(0.0, 0.0);
    const SushiDSP::room::HrtfSet::Pair back = set().interpolate(K_PI, 0.0);
    EXPECT_LE(delay_ratio(front.itd_frames, back.itd_frames, K_INTERPOLATION_OPERATIONS), K_LIMIT);
}

TEST_F(Small_SphericalHeadHrtf, FarEarIsShadowedByFourDecibelsInTheFourKilohertzOctave)
{
    // The far ear sits at the sphere's bright spot. See tests/nodes/room/README.md.
    EXPECT_GE(side_level_db(4000.0), K_SHADOW_AT_FOUR_KILOHERTZ_DB);
}

TEST_F(Small_SphericalHeadHrtf, TwoEarsHearTheLowOctaveAtAboutTheSameLevel)
{
    EXPECT_LE(std::fabs(side_level_db(125.0)), K_LOW_OCTAVE_DIFFERENCE_DB);
}

TEST_P(Small_SphericalHeadHrtfShadow, ShadowGrowsFromOctaveToOctave)
{
    EXPECT_GT(side_level_db(GetParam().centre_hz), side_level_db(GetParam().below_hz));
}

INSTANTIATE_TEST_SUITE_P(Octaves, Small_SphericalHeadHrtfShadow, ::testing::ValuesIn(kOctaves),
                         octave_name);

TEST_F(Small_SphericalHeadHrtf, GridHoldsSixHundredAndFourteenDirections)
{
    EXPECT_EQ(set().direction_count(), 614u);
}

TEST_F(Small_SphericalHeadHrtf, EveryCellOfTheTenDegreeGridHasADirectionInTheSet)
{
    EXPECT_EQ(grid_cells_the_set_misses(), 0u);
}

TEST_F(Small_SphericalHeadHrtf, EverySeededDirectionLiesWithinHalfAWideCellDiagonalOfAGridDirection)
{
    EXPECT_LE(largest_gap_over_seeded_directions(),
              K_WIDEST_CELL_HALF_DIAGONAL_RAD + K_STORED_ANGLE_SLACK_RAD);
}

TEST_F(Small_SphericalHeadHrtf, EverySeededDirectionGivesFiniteResponses)
{
    EXPECT_TRUE(every_direction_is_finite());
}

TEST_F(Small_SphericalHeadHrtf, EverySeededDirectionGivesAResponseWithEnergy)
{
    EXPECT_GT(least_energy_over_directions(), 0.0);
}

TEST_P(Small_SphericalHeadHrtfDiffuse, DiffuseFieldCarriesEnergy)
{
    EXPECT_GT(energy(set().diffuse_field(GetParam())), K_MIN_DIFFUSE_ENERGY);
}

TEST_P(Small_SphericalHeadHrtfDiffuse, DiffuseFieldIsFinite)
{
    EXPECT_TRUE(all_finite(set().diffuse_field(GetParam())));
}

TEST_F(Small_SphericalHeadHrtf, SphericalHeadHrtfHearsTheSameDiffuseFieldAtBothEars)
{
    // The grid mirrors about the median plane. See tests/nodes/room/README.md.
    EXPECT_LE(tap_ratio(set().diffuse_field(0), set().diffuse_field(1), set().direction_count()),
              K_LIMIT);
}

TEST_P(Small_SphericalHeadHrtfDiffuse, DiffusePowerAtDirectCurrentIsTheSpheresOne)
{
    // The sphere's mean diffuse power is 1.8472, +2.67 dB. See tests/nodes/room/README.md.
    EXPECT_NEAR(power_at(set().diffuse_field(GetParam()), 0.0), 1.0, K_DC_POWER_TOLERANCE);
}

TEST_P(Small_SphericalHeadHrtfDiffuse, DiffusePowerAtFourKilohertzIsTheSpheres)
{
    EXPECT_NEAR(power_at(set().diffuse_field(GetParam()), 4000.0), 1.7765,
                K_FOUR_KILOHERTZ_POWER_TOLERANCE);
}

TEST_P(Small_SphericalHeadHrtfDiffuse, DiffuseEnergyIsTheSpheresMeanPower)
{
    EXPECT_NEAR(energy(set().diffuse_field(GetParam())), 1.847, K_DIFFUSE_ENERGY_TOLERANCE);
}

INSTANTIATE_TEST_SUITE_P(Ears, Small_SphericalHeadHrtfDiffuse,
                         ::testing::Values(0u, 1u), ear_name);
