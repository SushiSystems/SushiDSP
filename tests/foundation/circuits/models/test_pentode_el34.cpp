/**************************************************************************/
/* test_pentode_el34.cpp                                                  */
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
 * @file test_pentode_el34.cpp
 * @brief Tests the PentodeEL34 model against hand-computed currents and derivatives.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/models/pentode_el34.hpp>

namespace
{
    using Currents = std::array<double, 3>;
    using Jacobian = std::array<std::array<double, 3>, 3>;

    constexpr std::size_t K_GRID = 0;
    constexpr std::size_t K_SCREEN = 1;
    constexpr std::size_t K_PLATE = 2;

    /// @brief Holds the digits the README prints its Koren oracle to.
    constexpr int K_ORACLE_DIGITS = 15;

    /// @brief Holds the rounding budget of the model's longest chain (argument, softplus, power, arctangent, products), counted generously.
    constexpr double K_ORACLE_OPERATIONS = 64.0;

    /// @brief Holds the step of the centred differences, in volts.
    constexpr double K_STEP = 1e-5;

    const double kU = TestSupport::unit_roundoff<double>();

    /// @brief Returns the bound for a reference printed with K_ORACLE_DIGITS digits: half a unit of the last one plus the model's own rounding.
    double oracle_bound(double reference)
    {
        if (reference == 0.0)
        {
            return 0.0;
        }
        const double printed = 0.5 * std::pow(10.0, std::floor(std::log10(std::fabs(reference))) - (K_ORACLE_DIGITS - 1));
        return printed + K_ORACLE_OPERATIONS * kU * std::fabs(reference);
    }

    TestSupport::ErrorRatio against_oracle(double computed, double reference)
    {
        return TestSupport::measure_error(computed, reference, oracle_bound(reference));
    }

    /// @brief Holds one operating point and the name its row is reported under.
    struct Point
    {
            const char* name;
            double vg1k;
            double vg2k;
            double vpk;
    };

    /// @brief Holds one Koren row: an operating point and the hand-solved currents and conductances of README.md.
    struct KorenRow
    {
            const char* name;
            Currents voltages;
            double ip;
            double dip_dvg1k;
            double dip_dvg2k;
            double dip_dvpk;
            double ig2;
            double dig2_dvg1k;
            double dig2_dvg2k;
    };

    // At the idle point z < 0, so the screen draws nothing and its whole row is zero.
    const KorenRow kKorenRows[] = {
        {"IdleAtMinus37", {-37.0, 400.0, 400.0}, 3.34149261300818e-2, 4.98316907313410e-3, 5.73718514953930e-4,
         3.30555615548759e-6, 0.0, 0.0, 0.0},
        {"MinusTwentyAt300", {-20.0, 300.0, 250.0}, 7.92947981392609e-2, 1.04281593600714e-2, 1.05203721563143e-3,
         2.04537709631731e-5, 3.23652408855726e-3, 6.00779783938442e-4, 5.46163439944038e-5},
        {"MinusFiveHardOn", {-5.0, 400.0, 100.0}, 4.31496731894557e-1, 1.83712705886168e-2, 1.68594235250184e-3,
         7.33338162266083e-4, 2.32789367120296e-2, 1.00200640630040e-3, 9.10914914818548e-5},
    };

    /// @brief Holds one grid-law row: the grid voltage and the current and conductance README.md prints.
    struct GridRow
    {
            const char* name;
            double vg1k;
            double ig1;
            double dig1;
    };

    const GridRow kGridRows[] = {
        {"MinusOnePointFive", -1.5, 1.61959548076714e-12, 1.07972944614873e-11},
        {"AtTheThreshold", 0.5, 6.93147180559945e-7, 3.33333333333333e-6},
        {"OneVolt", 1.0, 3.36838574941280e-6, 6.43703202889193e-6},
    };

    // None sits within a step of the screen kink z = Vg2k / mu + Vg1k = 0, where a centred difference averages two slopes.
    const Point kDifferencePoints[] = {
        {"MinusFortyFiveAt50", -45.0, 50.0, 20.0},     {"MinusThirtyAt200", -30.0, 200.0, 450.0},
        {"MinusFifteenAt400", -15.0, 400.0, 250.0},    {"MinusFiveAt50", -5.0, 50.0, 100.0},
        {"ZeroAt200", 0.0, 200.0, 20.0},               {"MinusTwentyAt300", -20.0, 300.0, 250.0},
        {"IdleAtMinus37", -37.0, 400.0, 400.0},        {"MinusFiveAt400", -5.0, 400.0, 100.0},
    };

    /// @brief Holds one offset from the screen kink, in volts of grid bias.
    struct KinkOffset
    {
            const char* name;
            double volts;
    };

    const KinkOffset kKinkOffsets[] = {
        {"MinusOneHundredth", -1e-2}, {"MinusTenThousandth", -1e-4}, {"MinusMillionth", -1e-6},
        {"PlusMillionth", 1e-6},      {"PlusTenThousandth", 1e-4},    {"PlusOneHundredth", 1e-2},
    };

    const Point kJointPoints[] = {
        {"DeepCutOff", -45.0, 50.0, 20.0},     {"ScreenFloor", -37.0, 1e-4, 100.0}, {"Idle", -37.0, 400.0, 400.0},
        {"MidBias", -20.0, 300.0, 250.0},      {"HardOn", -5.0, 400.0, 100.0},      {"ZeroBiasLowScreen", 0.0, 50.0, 20.0},
        {"ZeroBiasHighScreen", 0.0, 400.0, 400.0}, {"ScreenFloorHardOn", -5.0, 1e-4, 250.0},
    };

    const Point kAbsurdPoints[] = {
        {"AllNegativeHuge", -1e4, -1e4, -1e4}, {"AllPositiveHuge", 1e4, 1e4, 1e4},
        {"GridNegativeHuge", -1e4, 400.0, 400.0}, {"GridPositiveHuge", 1e4, 400.0, 400.0},
        {"GridMinusFifty", -50.0, 1.0, 0.0},   {"GridPlusFifty", 50.0, 1e-4, 1e4},
        {"ScreenNegativeHuge", -50.0, -1e4, 400.0}, {"ScreenPositiveHuge", -50.0, 1e4, 400.0},
        {"ScreenMinusOne", 0.0, -1.0, 0.0},    {"ScreenZero", 0.0, 0.0, 0.0},
        {"ScreenTiny", 0.0, 1e-4, 0.0},        {"ScreenOne", 0.0, 1.0, 100.0},
        {"PlateNegativeHuge", -50.0, 400.0, -1e4}, {"PlatePositiveHuge", -50.0, 400.0, 1e4},
        {"HugeGridTinyScreen", -1e4, 1e-4, 1e4}, {"HugePlateNegativeScreen", 1e4, -1.0, -1e4},
    };

    std::string point_name(const ::testing::TestParamInfo<Point>& info)
    {
        return std::string(info.param.name);
    }

    Currents voltages_of(const Point& point)
    {
        return {point.vg1k, point.vg2k, point.vpk};
    }

    Currents currents_at(const SushiDSP::PentodeEL34& pentode, const Currents& voltages)
    {
        Currents currents{};
        pentode.current(voltages, currents);
        return currents;
    }

    Jacobian jacobian_at(const SushiDSP::PentodeEL34& pentode, const Currents& voltages)
    {
        Jacobian jacobian{};
        pentode.jacobian(voltages, jacobian);
        return jacobian;
    }

    /// @brief Returns the centred difference of one branch current with respect to one input, over the given step.
    double centred_difference(const SushiDSP::PentodeEL34& pentode, Currents voltages, std::size_t output,
                              std::size_t input, double step)
    {
        Currents down = voltages;
        voltages[input] += step;
        down[input] -= step;
        return (currents_at(pentode, voltages)[output] - currents_at(pentode, down)[output]) / (2.0 * step);
    }

    /// @brief Measures one Jacobian entry against its centred difference.
    ///
    /// The truncation of the centred difference is estimated as the gap between the differences over the step and over twice the step, which shrinks fourfold with the step; it is taken three times over. The rounding term is the two currents' rounding over twice the step.
    TestSupport::ErrorRatio difference_error(const SushiDSP::PentodeEL34& pentode, const Currents& voltages,
                                             std::size_t output, std::size_t input)
    {
        const double fine = centred_difference(pentode, voltages, output, input, K_STEP);
        const double coarse = centred_difference(pentode, voltages, output, input, 2.0 * K_STEP);
        Currents up = voltages;
        up[input] += K_STEP;
        const double magnitude = std::fabs(currents_at(pentode, up)[output]) + std::fabs(currents_at(pentode, voltages)[output]);
        const double bound = 3.0 * std::fabs(coarse - fine) + K_ORACLE_OPERATIONS * kU * magnitude / K_STEP;
        return TestSupport::measure_error(jacobian_at(pentode, voltages)[output][input], fine, bound);
    }

    /// @brief Returns the largest grid-bias excursion z can reach for a requested offset from the kink: the offset plus the rounding of forming z from a 400 V screen.
    double kink_z_limit(double offset)
    {
        return std::fabs(offset) + 8.0 * kU * 400.0 / SushiDSP::PentodeEL34::K_MU;
    }

    bool all_finite(const Currents& values)
    {
        return std::all_of(values.begin(), values.end(), [](double value) { return std::isfinite(value); });
    }

    bool all_finite(const Jacobian& values)
    {
        return std::all_of(values.begin(), values.end(), [](const Currents& row) { return all_finite(row); });
    }
} // namespace

class Small_PentodeEl34Koren : public ::testing::TestWithParam<KorenRow>
{
};

// The README prints each oracle to 15 digits; the bound is half a unit of the last one plus the model's own rounding.
TEST_P(Small_PentodeEl34Koren, CurrentsAndConductancesMatchKorensLaw)
{
    const SushiDSP::PentodeEL34 pentode;
    const KorenRow& row = GetParam();
    const Currents currents = currents_at(pentode, row.voltages);
    const Jacobian jacobian = jacobian_at(pentode, row.voltages);

    EXPECT_TRUE(TestSupport::is_within(against_oracle(currents[K_PLATE], row.ip)));
    EXPECT_TRUE(TestSupport::is_within(against_oracle(currents[K_SCREEN], row.ig2)));
    EXPECT_TRUE(TestSupport::is_within(against_oracle(jacobian[K_PLATE][0], row.dip_dvg1k)));
    EXPECT_TRUE(TestSupport::is_within(against_oracle(jacobian[K_PLATE][1], row.dip_dvg2k)));
    EXPECT_TRUE(TestSupport::is_within(against_oracle(jacobian[K_PLATE][2], row.dip_dvpk)));
    EXPECT_TRUE(TestSupport::is_within(against_oracle(jacobian[K_SCREEN][0], row.dig2_dvg1k)));
    EXPECT_TRUE(TestSupport::is_within(against_oracle(jacobian[K_SCREEN][1], row.dig2_dvg2k)));
}

TEST_P(Small_PentodeEl34Koren, StructuralZerosAreExact)
{
    const SushiDSP::PentodeEL34 pentode;
    const Jacobian jacobian = jacobian_at(pentode, GetParam().voltages);

    EXPECT_EQ(jacobian[K_SCREEN][2], 0.0);
    EXPECT_EQ(jacobian[K_GRID][1], 0.0);
    EXPECT_EQ(jacobian[K_GRID][2], 0.0);
}

INSTANTIATE_TEST_SUITE_P(Points, Small_PentodeEl34Koren, ::testing::ValuesIn(kKorenRows),
                         [](const ::testing::TestParamInfo<KorenRow>& info) { return std::string(info.param.name); });

class Small_PentodeEl34Grid : public ::testing::TestWithParam<GridRow>
{
};

// Ig1 = 1e-6 ln(1 + exp((Vg1k - 0.5) / 0.15)), its slope the logistic of the same argument; the oracle is README.md.
TEST_P(Small_PentodeEl34Grid, CurrentAndConductanceFollowNormansSoftLaw)
{
    const SushiDSP::PentodeEL34 pentode;
    const GridRow& row = GetParam();
    const Currents voltages{row.vg1k, 400.0, 400.0};

    EXPECT_TRUE(TestSupport::is_within(against_oracle(currents_at(pentode, voltages)[K_GRID], row.ig1)));
    EXPECT_TRUE(TestSupport::is_within(against_oracle(jacobian_at(pentode, voltages)[K_GRID][0], row.dig1)));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_PentodeEl34Grid, ::testing::ValuesIn(kGridRows),
                         [](const ::testing::TestParamInfo<GridRow>& info) { return std::string(info.param.name); });

class Small_PentodeEl34Difference : public ::testing::TestWithParam<Point>
{
};

TEST_P(Small_PentodeEl34Difference, JacobianMatchesCentredDifferencesOfTheCurrents)
{
    const SushiDSP::PentodeEL34 pentode;
    const Currents voltages = voltages_of(GetParam());

    EXPECT_TRUE(TestSupport::is_within(difference_error(pentode, voltages, K_GRID, 0)));
    EXPECT_TRUE(TestSupport::is_within(difference_error(pentode, voltages, K_SCREEN, 0)));
    EXPECT_TRUE(TestSupport::is_within(difference_error(pentode, voltages, K_SCREEN, 1)));
    EXPECT_TRUE(TestSupport::is_within(difference_error(pentode, voltages, K_PLATE, 0)));
    EXPECT_TRUE(TestSupport::is_within(difference_error(pentode, voltages, K_PLATE, 1)));
    EXPECT_TRUE(TestSupport::is_within(difference_error(pentode, voltages, K_PLATE, 2)));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_PentodeEl34Difference, ::testing::ValuesIn(kDifferencePoints), point_name);

class Small_PentodeEl34Kink : public ::testing::TestWithParam<KinkOffset>
{
};

// kx > 1, so z^kx and kx z^(kx-1) vanish continuously at z = 0. Near the kink z lies within |offset| of zero, so the screen current stays under |z|^kx / kg2 and its slopes under kx |z|^(kx-1) / kg2 (and a mu-th of it for Vg2k).
TEST_P(Small_PentodeEl34Kink, ScreenCurrentAndSlopesStayWithinTheirPowerLaw)
{
    const SushiDSP::PentodeEL34 pentode;
    constexpr double screen = 400.0;
    const double kink = -screen / SushiDSP::PentodeEL34::K_MU;
    const Currents voltages{kink + GetParam().volts, screen, 400.0};
    const double limit = kink_z_limit(GetParam().volts);
    const double slope_limit = SushiDSP::PentodeEL34::K_KX * std::pow(limit, SushiDSP::PentodeEL34::K_KX - 1.0) /
                               SushiDSP::PentodeEL34::K_KG2;
    const Currents currents = currents_at(pentode, voltages);
    const Jacobian jacobian = jacobian_at(pentode, voltages);

    EXPECT_GE(currents[K_SCREEN], 0.0);
    EXPECT_LE(currents[K_SCREEN], std::pow(limit, SushiDSP::PentodeEL34::K_KX) / SushiDSP::PentodeEL34::K_KG2);
    EXPECT_GE(jacobian[K_SCREEN][0], 0.0);
    EXPECT_LE(jacobian[K_SCREEN][0], slope_limit);
    EXPECT_GE(jacobian[K_SCREEN][1], 0.0);
    EXPECT_LE(jacobian[K_SCREEN][1], slope_limit / SushiDSP::PentodeEL34::K_MU);
}

INSTANTIATE_TEST_SUITE_P(Offsets, Small_PentodeEl34Kink, ::testing::ValuesIn(kKinkOffsets),
                         [](const ::testing::TestParamInfo<KinkOffset>& info) { return std::string(info.param.name); });

TEST(Small_PentodeEl34, PentodeEL34ScreenDrawsExactlyNothingAtTheKink)
{
    const SushiDSP::PentodeEL34 pentode;
    const Currents voltages{-400.0 / SushiDSP::PentodeEL34::K_MU, 400.0, 400.0};
    const Currents currents = currents_at(pentode, voltages);
    const Jacobian jacobian = jacobian_at(pentode, voltages);

    EXPECT_EQ(currents[K_SCREEN], 0.0);
    EXPECT_EQ(jacobian[K_SCREEN][0], 0.0);
    EXPECT_EQ(jacobian[K_SCREEN][1], 0.0);
}

// Ip is a grid-only term times atan(Vpk / kvb), so quadrupling Vpk scales it by atan(400 / 24) / atan(100 / 24) = 1.13152342150114. The grid term is the same product in both, so the ratio costs two arctangents, two divisions, two products and one quotient.
TEST(Small_PentodeEl34, PentodeEL34PlateCurrentKneesOverWithPlateVoltage)
{
    const SushiDSP::PentodeEL34 pentode;
    const Currents high = currents_at(pentode, {-20.0, 300.0, 400.0});
    const Currents low = currents_at(pentode, {-20.0, 300.0, 100.0});

    EXPECT_TRUE(TestSupport::is_within(against_oracle(high[K_PLATE] / low[K_PLATE], 1.13152342150114)));
}

TEST(Small_PentodeEl34, PentodeEL34PlateVoltageLeavesTheGridAndScreenCurrentsUnchanged)
{
    const SushiDSP::PentodeEL34 pentode;
    const Currents high = currents_at(pentode, {-20.0, 300.0, 400.0});
    const Currents low = currents_at(pentode, {-20.0, 300.0, 100.0});

    EXPECT_EQ(high[K_SCREEN], low[K_SCREEN]);
    EXPECT_EQ(high[K_GRID], low[K_GRID]);
}

class Small_PentodeEl34Joint : public ::testing::TestWithParam<Point>
{
};

// current() and jacobian() forward to the joint form, so the values are identical, and the three structural zeros stay zero.
TEST_P(Small_PentodeEl34Joint, AgreesWithTheSeparateEntryPoints)
{
    const SushiDSP::PentodeEL34 pentode;
    const Currents voltages = voltages_of(GetParam());
    Currents joint_currents{};
    Jacobian joint_jacobian{};
    pentode.current_and_jacobian(voltages, joint_currents, joint_jacobian);

    EXPECT_EQ(joint_currents, currents_at(pentode, voltages));
    EXPECT_EQ(joint_jacobian, jacobian_at(pentode, voltages));
    EXPECT_EQ(joint_jacobian[K_GRID][1], 0.0);
    EXPECT_EQ(joint_jacobian[K_GRID][2], 0.0);
    EXPECT_EQ(joint_jacobian[K_SCREEN][2], 0.0);
}

INSTANTIATE_TEST_SUITE_P(Points, Small_PentodeEl34Joint, ::testing::ValuesIn(kJointPoints), point_name);

class Small_PentodeEl34Absurd : public ::testing::TestWithParam<Point>
{
};

TEST_P(Small_PentodeEl34Absurd, CurrentsAndConductancesStayFinite)
{
    const SushiDSP::PentodeEL34 pentode;
    const Currents voltages = voltages_of(GetParam());

    EXPECT_TRUE(all_finite(currents_at(pentode, voltages)));
    EXPECT_TRUE(all_finite(jacobian_at(pentode, voltages)));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_PentodeEl34Absurd, ::testing::ValuesIn(kAbsurdPoints), point_name);
