/**************************************************************************/
/* test_triode12_ax7.cpp                                                  */
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
 * @file test_triode12_ax7.cpp
 * @brief Tests the Triode12AX7 model against hand-computed currents and derivatives.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/models/triode12_ax7.hpp>

namespace
{
    using Currents = std::array<double, 2>;
    using Jacobian = std::array<std::array<double, 2>, 2>;

    constexpr std::size_t K_GRID = 0;
    constexpr std::size_t K_PLATE = 1;

    /// @brief Holds the digits the README prints its Koren oracle to.
    constexpr int K_ORACLE_DIGITS = 15;

    /// @brief Holds the rounding budget of the model's longest chain (root, argument, softplus, power, products), counted generously.
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
            double vgk;
            double vpk;
    };

    /// @brief Holds one Koren row: an operating point and the hand-solved plate current and conductances of README.md.
    struct KorenRow
    {
            const char* name;
            Currents voltages;
            double ip;
            double dip_dvgk;
            double dip_dvpk;
    };

    const KorenRow kKorenRows[] = {
        {"QuiescentPoint", {-1.5, 250.0}, 1.99187388684622e-3, 2.45547036171309e-3, 2.58169362128740e-5},
        {"TowardCutOff", {-2.0, 100.0}, 4.40941496483551e-8, 3.64418892389800e-7, 7.69341309293717e-9},
        {"GridAtTheCathode", {0.0, 300.0}, 8.78910615164942e-3, 4.08295444882820e-3, 4.10158287076973e-5},
    };

    /// @brief Holds one grid-law row: the grid voltage and the current and conductance README.md prints.
    struct GridRow
    {
            const char* name;
            double vgk;
            double ig;
            double dig;
    };

    const GridRow kGridRows[] = {
        {"MinusOnePointFive", -1.5, 1.61959548076714e-12, 1.07972944614873e-11},
        {"AtTheThreshold", 0.5, 6.93147180559945e-7, 3.33333333333333e-6},
        {"OneVolt", 1.0, 3.36838574941280e-6, 6.43703202889193e-6},
    };

    const Point kDifferencePoints[] = {
        {"MinusEightAt5", -8.0, 5.0},   {"MinusThreeAt50", -3.0, 50.0},   {"MinusOneAt150", -1.0, 150.0},
        {"ZeroAt300", 0.0, 300.0},      {"OneAt400", 1.0, 400.0},         {"MinusEightAt400", -8.0, 400.0},
        {"OneAt5", 1.0, 5.0},           {"QuiescentPoint", -1.5, 250.0},  {"TowardCutOff", -2.0, 100.0},
    };

    /// @brief Holds a plate voltage at or below zero and the name its row is reported under.
    struct PlateVoltage
    {
            const char* name;
            double volts;
    };

    const PlateVoltage kCutOffVoltages[] = {{"MinusTen", -10.0}, {"MinusOneThousandth", -1e-3}, {"Zero", 0.0}};

    const PlateVoltage kNearZeroVoltages[] = {
        {"OneThousandth", 1e-3}, {"OneTenThousandth", 1e-4}, {"OneHundredThousandth", 1e-5},
    };

    const Point kJointPoints[] = {
        {"CutOffPlate", -1.5, -10.0},   {"LowPlate", -8.0, 5.0},   {"MidPlate", -3.0, 100.0},
        {"Quiescent", -1.5, 250.0},     {"GridAtCathode", 0.0, 250.0}, {"GridConducting", 1.0, 400.0},
        {"ZeroPlate", 0.0, 0.0},        {"DeepGrid", -8.0, 400.0},
    };

    const Point kAbsurdPoints[] = {
        {"GridNegativeHugePlateNegativeHuge", -1e4, -1e4}, {"GridNegativeHugePlateZero", -1e4, 0.0},
        {"GridNegativeHugePlatePositiveHuge", -1e4, 1e4},  {"GridMinusFiftyPlateNegativeHuge", -50.0, -1e4},
        {"GridMinusFiftyPlateZero", -50.0, 0.0},           {"GridMinusFiftyPlatePositiveHuge", -50.0, 1e4},
        {"GridPlusFiftyPlateNegativeHuge", 50.0, -1e4},    {"GridPlusFiftyPlateZero", 50.0, 0.0},
        {"GridPlusFiftyPlatePositiveHuge", 50.0, 1e4},     {"GridPositiveHugePlateNegativeHuge", 1e4, -1e4},
        {"GridPositiveHugePlateZero", 1e4, 0.0},           {"GridPositiveHugePlatePositiveHuge", 1e4, 1e4},
    };

    std::string point_name(const ::testing::TestParamInfo<Point>& info)
    {
        return std::string(info.param.name);
    }

    std::string plate_name(const ::testing::TestParamInfo<PlateVoltage>& info)
    {
        return std::string(info.param.name);
    }

    Currents voltages_of(const Point& point)
    {
        return {point.vgk, point.vpk};
    }

    Currents currents_at(const SushiDSP::Triode12AX7& triode, const Currents& voltages)
    {
        Currents currents{};
        triode.current(voltages, currents);
        return currents;
    }

    Jacobian jacobian_at(const SushiDSP::Triode12AX7& triode, const Currents& voltages)
    {
        Jacobian jacobian{};
        triode.jacobian(voltages, jacobian);
        return jacobian;
    }

    /// @brief Returns the centred difference of one branch current with respect to one input, over the given step.
    double centred_difference(const SushiDSP::Triode12AX7& triode, Currents voltages, std::size_t output,
                              std::size_t input, double step)
    {
        Currents down = voltages;
        voltages[input] += step;
        down[input] -= step;
        return (currents_at(triode, voltages)[output] - currents_at(triode, down)[output]) / (2.0 * step);
    }

    /// @brief Measures one Jacobian entry against its centred difference.
    ///
    /// The truncation of the centred difference is estimated as the gap between the differences over the step and over twice the step, which shrinks fourfold with the step; it is taken three times over. The rounding term is the two currents' rounding over twice the step.
    TestSupport::ErrorRatio difference_error(const SushiDSP::Triode12AX7& triode, const Currents& voltages,
                                             std::size_t output, std::size_t input)
    {
        const double fine = centred_difference(triode, voltages, output, input, K_STEP);
        const double coarse = centred_difference(triode, voltages, output, input, 2.0 * K_STEP);
        Currents up = voltages;
        up[input] += K_STEP;
        const double magnitude = std::fabs(currents_at(triode, up)[output]) + std::fabs(currents_at(triode, voltages)[output]);
        const double bound = 3.0 * std::fabs(coarse - fine) + K_ORACLE_OPERATIONS * kU * magnitude / K_STEP;
        return TestSupport::measure_error(jacobian_at(triode, voltages)[output][input], fine, bound);
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

class Small_Triode12Ax7Koren : public ::testing::TestWithParam<KorenRow>
{
};

// The README prints each oracle to 15 digits; the bound is half a unit of the last one plus the model's own rounding.
TEST_P(Small_Triode12Ax7Koren, PlateCurrentAndConductancesMatchKorensLaw)
{
    const SushiDSP::Triode12AX7 triode;
    const KorenRow& row = GetParam();
    const Jacobian jacobian = jacobian_at(triode, row.voltages);

    EXPECT_TRUE(TestSupport::is_within(against_oracle(currents_at(triode, row.voltages)[K_PLATE], row.ip)));
    EXPECT_TRUE(TestSupport::is_within(against_oracle(jacobian[K_PLATE][0], row.dip_dvgk)));
    EXPECT_TRUE(TestSupport::is_within(against_oracle(jacobian[K_PLATE][1], row.dip_dvpk)));
}

TEST_P(Small_Triode12Ax7Koren, GridCurrentIsIndependentOfThePlateVoltage)
{
    const SushiDSP::Triode12AX7 triode;
    EXPECT_EQ(jacobian_at(triode, GetParam().voltages)[K_GRID][1], 0.0);
}

INSTANTIATE_TEST_SUITE_P(Points, Small_Triode12Ax7Koren, ::testing::ValuesIn(kKorenRows),
                         [](const ::testing::TestParamInfo<KorenRow>& info) { return std::string(info.param.name); });

class Small_Triode12Ax7Grid : public ::testing::TestWithParam<GridRow>
{
};

// Ig = 1e-6 ln(1 + exp((Vgk - 0.5) / 0.15)), its slope the logistic of the same argument; the oracle is README.md.
TEST_P(Small_Triode12Ax7Grid, CurrentAndConductanceFollowNormansSoftLaw)
{
    const SushiDSP::Triode12AX7 triode;
    const GridRow& row = GetParam();
    const Currents voltages{row.vgk, 250.0};

    EXPECT_TRUE(TestSupport::is_within(against_oracle(currents_at(triode, voltages)[K_GRID], row.ig)));
    EXPECT_TRUE(TestSupport::is_within(against_oracle(jacobian_at(triode, voltages)[K_GRID][0], row.dig)));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_Triode12Ax7Grid, ::testing::ValuesIn(kGridRows),
                         [](const ::testing::TestParamInfo<GridRow>& info) { return std::string(info.param.name); });

class Small_Triode12Ax7Difference : public ::testing::TestWithParam<Point>
{
};

TEST_P(Small_Triode12Ax7Difference, JacobianMatchesCentredDifferencesOfTheCurrents)
{
    const SushiDSP::Triode12AX7 triode;
    const Currents voltages = voltages_of(GetParam());

    EXPECT_TRUE(TestSupport::is_within(difference_error(triode, voltages, K_GRID, 0)));
    EXPECT_TRUE(TestSupport::is_within(difference_error(triode, voltages, K_PLATE, 0)));
    EXPECT_TRUE(TestSupport::is_within(difference_error(triode, voltages, K_PLATE, 1)));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_Triode12Ax7Difference, ::testing::ValuesIn(kDifferencePoints), point_name);

class Small_Triode12Ax7CutOff : public ::testing::TestWithParam<PlateVoltage>
{
};

TEST_P(Small_Triode12Ax7CutOff, PlateCurrentAndConductancesAreExactlyZero)
{
    const SushiDSP::Triode12AX7 triode;
    const Currents voltages{-2.0, GetParam().volts};
    const Jacobian jacobian = jacobian_at(triode, voltages);

    EXPECT_EQ(currents_at(triode, voltages)[K_PLATE], 0.0);
    EXPECT_EQ(jacobian[K_PLATE][0], 0.0);
    EXPECT_EQ(jacobian[K_PLATE][1], 0.0);
}

INSTANTIATE_TEST_SUITE_P(Voltages, Small_Triode12Ax7CutOff, ::testing::ValuesIn(kCutOffVoltages), plate_name);

class Small_Triode12Ax7NearZero : public ::testing::TestWithParam<PlateVoltage>
{
};

// At Vgk = 0 the softplus argument is kp / mu = 6 for any plate voltage, so E1 <= Vpk s / kp with s = ln(1 + e^6) <= 6.01. Ip, gm and gp then stay under the power law of that E1 and go to zero with Vpk: the definition is C1 at Vpk = 0. Each bound carries the rounding budget of the model.
TEST_P(Small_Triode12Ax7NearZero, PlateCurrentAndConductancesStayWithinTheirPowerLaw)
{
    using Model = SushiDSP::Triode12AX7;
    const Model triode;
    constexpr double s_limit = 6.01;
    const double margin = 1.0 + K_ORACLE_OPERATIONS * kU;
    const double vpk = GetParam().volts;
    const double e1_limit = vpk * s_limit / Model::K_KP;
    const double slope = 2.0 * Model::K_KX * std::pow(e1_limit, Model::K_KX - 1.0) / Model::K_KG1;
    const Currents voltages{0.0, vpk};
    const Jacobian jacobian = jacobian_at(triode, voltages);

    EXPECT_GE(currents_at(triode, voltages)[K_PLATE], 0.0);
    EXPECT_LE(currents_at(triode, voltages)[K_PLATE], margin * 2.0 * std::pow(e1_limit, Model::K_KX) / Model::K_KG1);
    EXPECT_GE(jacobian[K_PLATE][0], 0.0);
    EXPECT_LE(jacobian[K_PLATE][0], margin * slope * vpk / std::sqrt(Model::K_KVB));
    EXPECT_GE(jacobian[K_PLATE][1], 0.0);
    EXPECT_LE(jacobian[K_PLATE][1], margin * slope * s_limit / Model::K_KP);
}

INSTANTIATE_TEST_SUITE_P(Voltages, Small_Triode12Ax7NearZero, ::testing::ValuesIn(kNearZeroVoltages), plate_name);

class Small_Triode12Ax7Joint : public ::testing::TestWithParam<Point>
{
};

// current() and jacobian() forward to the joint form, so the values are identical, and the grid current stays independent of the plate.
TEST_P(Small_Triode12Ax7Joint, AgreesWithTheSeparateEntryPoints)
{
    const SushiDSP::Triode12AX7 triode;
    const Currents voltages = voltages_of(GetParam());
    Currents joint_currents{};
    Jacobian joint_jacobian{};
    triode.current_and_jacobian(voltages, joint_currents, joint_jacobian);

    EXPECT_EQ(joint_currents, currents_at(triode, voltages));
    EXPECT_EQ(joint_jacobian, jacobian_at(triode, voltages));
    EXPECT_EQ(joint_jacobian[K_GRID][1], 0.0);
}

INSTANTIATE_TEST_SUITE_P(Points, Small_Triode12Ax7Joint, ::testing::ValuesIn(kJointPoints), point_name);

class Small_Triode12Ax7Absurd : public ::testing::TestWithParam<Point>
{
};

TEST_P(Small_Triode12Ax7Absurd, CurrentsAndConductancesStayFinite)
{
    const SushiDSP::Triode12AX7 triode;
    const Currents voltages = voltages_of(GetParam());

    EXPECT_TRUE(all_finite(currents_at(triode, voltages)));
    EXPECT_TRUE(all_finite(jacobian_at(triode, voltages)));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_Triode12Ax7Absurd, ::testing::ValuesIn(kAbsurdPoints), point_name);
