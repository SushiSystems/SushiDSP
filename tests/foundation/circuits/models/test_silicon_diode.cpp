/**************************************************************************/
/* test_silicon_diode.cpp                                                 */
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
 * @file test_silicon_diode.cpp
 * @brief Tests the SiliconDiode model against hand-computed currents and derivatives.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/models/silicon_diode.hpp>

namespace
{
    // 1N914: Is = 2.52e-9 A, n = 1.752, Vt = 0.025864 V. See tests/foundation/circuits/models/README.md.
    constexpr double K_SATURATION = 2.52e-9;
    constexpr double K_SCALED_THERMAL_VOLTAGE = 1.752 * 0.025864;

    /// @brief Holds the knee v_max = 40 * n * Vt = 1.81254912 V, where the model leaves the exponential.
    constexpr double K_KNEE = 40.0 * K_SCALED_THERMAL_VOLTAGE;

    /// @brief Holds the distance either side of a knee at which the C1 checks read the slope.
    constexpr double K_KNEE_STEP = 1e-7;

    const double kU = TestSupport::unit_roundoff<double>();

    /// @brief Returns half a unit of the last printed digit of a reference printed with @p digits significant digits.
    double printed_digits_bound(double reference, int digits)
    {
        return 0.5 * std::pow(10.0, std::floor(std::log10(std::fabs(reference))) - (digits - 1));
    }

    /// @brief Returns the bound for two slopes read a step either side of a knee: the slope moves by step over n Vt of itself, plus the argument rounding.
    double knee_slope_bound(double slope)
    {
        return (K_KNEE_STEP / K_SCALED_THERMAL_VOLTAGE + 4.0 * 40.0 * kU) * slope;
    }

    /// @brief Holds one voltage and the name its row is reported under.
    struct Voltage
    {
            const char* name;
            double volts;
    };

    /// @brief Holds a reference value printed with a number of significant digits.
    struct Printed
    {
            double value;
            int digits;
    };

    /// @brief Holds one Shockley row: the diode current and conductance at a voltage, as the README prints them.
    struct ShockleyRow
    {
            const char* name;
            double volts;
            Printed current;
            Printed conductance;
    };

    const ShockleyRow kShockleyRows[] = {
        {"FourTenthsVolt", 0.4, {1.71792785943e-5, 12}, {3.79174244818e-4, 12}},
        {"SixTenthsVolt", 0.6, {1.41873699671e-3, 12}, {3.13092649696e-2, 12}},
        {"SevenTenthsVolt", 0.7, {1.2891979355e-2, 11}, {2.84504993167e-1, 12}},
    };

    const Voltage kDerivativeVoltages[] = {
        {"MinusThreeTenths", -0.3},
        {"Zero", 0.0},
        {"ThreeTenths", 0.3},
        {"FiftyFiveHundredths", 0.55},
        {"SixtyFiveHundredths", 0.65},
    };

    const Voltage kExtremeVoltages[] = {
        {"MinusSixty", -60.0}, {"MinusTwo", -2.0}, {"Two", 2.0}, {"Sixty", 60.0}, {"Thousand", 1000.0},
    };

    const Voltage kReverseExtremeVoltages[] = {
        {"MinusSixty", -60.0}, {"MinusTwo", -2.0}, {"MinusThousand", -1000.0},
    };

    const Voltage kJointVoltages[] = {
        {"MinusSixty", -60.0}, {"MinusHalf", -0.5}, {"Zero", 0.0},         {"FourTenths", 0.4},
        {"SixTenths", 0.6},    {"SevenTenths", 0.7}, {"AtTheKnee", K_KNEE}, {"Sixty", 60.0},
    };

    /// @brief Holds one antiparallel row: the pair's total current 2 Is sinh(w / n Vt) and conductance, printed to ten digits.
    struct PairRow
    {
            const char* name;
            double volts;
            double current;
            double conductance;
    };

    const PairRow kPairRows[] = {
        {"TwoTenths", 0.2, 2.080515143e-7, 4.592704262e-6},
        {"ThreeTenths", 0.3, 1.890822805e-6, 4.172752069e-5},
        {"ThirtyEightHundredths", 0.38, 1.105058316e-5, 2.438683551e-4},
        {"FourTenths", 0.4, 1.718179822e-5, 3.791742530e-4},
        {"Half", 0.5, 1.561297429e-4, 3.445528538e-3},
    };
    constexpr int K_PAIR_DIGITS = 10;

    const Voltage kAntiderivativeVoltages[] = {
        {"MinusTwoPointTwo", -2.2}, {"MinusOne", -1.0}, {"MinusThreeTenths", -0.3}, {"Zero", 0.0},
        {"ThreeTenths", 0.3},       {"EightTenths", 0.8}, {"OnePointSeven", 1.7},    {"TwoPointTwo", 2.2},
    };

    const Voltage kAntiderivativeJointVoltages[] = {
        {"MinusTwoPointFive", -2.5}, {"MinusSevenTenths", -0.7}, {"MinusFiveHundredths", -0.05}, {"Zero", 0.0},
        {"FiveHundredths", 0.05},    {"SevenTenths", 0.7},       {"TwoPointFive", 2.5},
    };

    const Voltage kKnees[] = {{"UpperKnee", K_KNEE}, {"LowerKnee", -K_KNEE}};

    /// @brief Holds one reverse-bias row: the current there and the bound it is held to.
    struct ReverseRow
    {
            const char* name;
            double volts;
            double current;
            double bound;
    };

    /// @brief Holds the conductance past the lower knee, Is / (n Vt) e^-40 = 2.36e-25 S.
    const double kReverseConductance = K_SATURATION / K_SCALED_THERMAL_VOLTAGE * std::exp(-40.0);

    /// @brief Returns the bound on the reverse current read against -Is: the tangent's departure from -Is, plus three roundings of Is.
    ///
    /// Past the lower knee the current is Is (e^-40 - 1) plus the conductance times the distance past the knee, so it departs from -Is by at most Is e^-40 plus that product.
    double reverse_saturation_bound(double volts)
    {
        const double past_the_knee = std::max(0.0, std::fabs(volts) - K_KNEE);
        return K_SATURATION * std::exp(-40.0) + kReverseConductance * past_the_knee + 3.0 * kU * K_SATURATION;
    }

    // At -0.5 V, inside the knee, the README prints 12 digits; past the knee the bound is reverse_saturation_bound.
    const ReverseRow kReverseRows[] = {
        {"MinusHalfVolt", -0.5, -2.51995932614e-9, printed_digits_bound(-2.51995932614e-9, 12)},
        {"MinusTwoVolts", -2.0, -K_SATURATION, reverse_saturation_bound(-2.0)},
        {"MinusTenVolts", -10.0, -K_SATURATION, reverse_saturation_bound(-10.0)},
        {"MinusThousandVolts", -1000.0, -K_SATURATION, reverse_saturation_bound(-1000.0)},
    };

    /// @brief Returns the bound on a centred difference of the current: its truncation h^2/6 I''', plus the rounding of the two currents over 2h.
    double current_difference_bound(const SushiDSP::SiliconDiode& diode, double voltage, double step)
    {
        const double third = diode.jacobian(voltage) / (K_SCALED_THERMAL_VOLTAGE * K_SCALED_THERMAL_VOLTAGE);
        const double argument = std::fabs(voltage) / K_SCALED_THERMAL_VOLTAGE;
        const double magnitude =
            std::fabs(diode.current(voltage + step)) + std::fabs(diode.current(voltage - step)) + 2.0 * K_SATURATION;
        return step * step / 6.0 * third + magnitude * kU * (3.0 * argument + 2.0) / (2.0 * step);
    }

    /// @brief Returns the bound on a centred difference of the antiderivative: the same two terms, the rounding scaled by the antiderivative.
    double antiderivative_difference_bound(const SushiDSP::SiliconDiode& diode, double voltage, double step)
    {
        const bool exponential = std::fabs(voltage) < K_KNEE;
        const double second =
            exponential ? (std::fabs(diode.current(voltage)) + K_SATURATION) / K_SCALED_THERMAL_VOLTAGE : 0.0;
        const double argument = std::min(std::fabs(voltage), K_KNEE) / K_SCALED_THERMAL_VOLTAGE;
        const double magnitude = std::fabs(diode.current_antiderivative(voltage + step)) +
                                 std::fabs(diode.current_antiderivative(voltage - step)) + K_SATURATION * K_KNEE;
        return step * step / 6.0 * second + magnitude * kU * (3.0 * argument + 4.0) / (2.0 * step);
    }

    std::string row_name(const ::testing::TestParamInfo<Voltage>& info)
    {
        return std::string(info.param.name);
    }
} // namespace

class Small_SiliconDiodeShockley : public ::testing::TestWithParam<ShockleyRow>
{
};

// The README prints each oracle to 11 or 12 digits; the model's own error, a few n u from the exponent, is far below half a unit of the last one.
TEST_P(Small_SiliconDiodeShockley, CurrentAndConductanceMatchTheShockleyLaw)
{
    const SushiDSP::SiliconDiode diode;
    const ShockleyRow& row = GetParam();

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        diode.current(row.volts), row.current.value, printed_digits_bound(row.current.value, row.current.digits))));
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(diode.jacobian(row.volts), row.conductance.value,
                                   printed_digits_bound(row.conductance.value, row.conductance.digits))));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_SiliconDiodeShockley, ::testing::ValuesIn(kShockleyRows),
                         [](const ::testing::TestParamInfo<ShockleyRow>& info) { return std::string(info.param.name); });

TEST(Small_SiliconDiode, SiliconDiodeCurrentIsZeroAtZeroVolts)
{
    const SushiDSP::SiliconDiode diode;
    EXPECT_EQ(diode.current(0.0), 0.0);
}

class Small_SiliconDiodeReverse : public ::testing::TestWithParam<ReverseRow>
{
};

TEST_P(Small_SiliconDiodeReverse, CurrentTakesItsRowValue)
{
    const SushiDSP::SiliconDiode diode;
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(diode.current(GetParam().volts), GetParam().current, GetParam().bound)));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_SiliconDiodeReverse, ::testing::ValuesIn(kReverseRows),
                         [](const ::testing::TestParamInfo<ReverseRow>& info) { return std::string(info.param.name); });

class Small_SiliconDiodeFarReverse : public ::testing::TestWithParam<Voltage>
{
};

// Beyond the lower knee the conductance is the constant Is / (n Vt) e^-40 = 2.36e-25 S: the same product of one division, one exponential and two multiplications, so at most five roundings from it.
TEST_P(Small_SiliconDiodeFarReverse, ConductanceVanishes)
{
    const SushiDSP::SiliconDiode diode;
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(
        diode.jacobian(GetParam().volts), kReverseConductance, 6)));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_SiliconDiodeFarReverse, ::testing::ValuesIn(kReverseExtremeVoltages), row_name);

class Small_SiliconDiodeJacobian : public ::testing::TestWithParam<Voltage>
{
};

TEST_P(Small_SiliconDiodeJacobian, MatchesACentredDifferenceOfTheCurrent)
{
    const SushiDSP::SiliconDiode diode;
    constexpr double step = 1e-6;
    const double voltage = GetParam().volts;
    const double numerical = (diode.current(voltage + step) - diode.current(voltage - step)) / (2.0 * step);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        diode.jacobian(voltage), numerical, current_difference_bound(diode, voltage, step))));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_SiliconDiodeJacobian, ::testing::ValuesIn(kDerivativeVoltages), row_name);

class Small_SiliconDiodeExtremes : public ::testing::TestWithParam<Voltage>
{
};

TEST_P(Small_SiliconDiodeExtremes, CurrentAndConductanceStayFinite)
{
    const SushiDSP::SiliconDiode diode;
    EXPECT_TRUE(std::isfinite(diode.current(GetParam().volts)));
    EXPECT_TRUE(std::isfinite(diode.jacobian(GetParam().volts)));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_SiliconDiodeExtremes, ::testing::ValuesIn(kExtremeVoltages), row_name);

TEST(Small_SiliconDiode, SiliconDiodeConductanceIsCOneAcrossTheUpperKnee)
{
    const SushiDSP::SiliconDiode diode;
    const double slope = diode.jacobian(K_KNEE);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        diode.jacobian(K_KNEE - K_KNEE_STEP), diode.jacobian(K_KNEE + K_KNEE_STEP), knee_slope_bound(slope))));
}

// Past the knee the current is the tangent line; the knee voltage is formed in two association orders, then a subtraction, a product and a sum: eight roundings.
TEST(Small_SiliconDiode, SiliconDiodeContinuesTheUpperKneeAsItsTangentLine)
{
    const SushiDSP::SiliconDiode diode;
    const double expected = diode.current(K_KNEE) + 0.5 * diode.jacobian(K_KNEE);

    EXPECT_TRUE(
        TestSupport::is_within(TestSupport::measure_relative_error<double>(diode.current(K_KNEE + 0.5), expected, 8)));
}

TEST(Small_SiliconDiode, SiliconDiodeCurrentIsCOneAcrossTheLowerKnee)
{
    const SushiDSP::SiliconDiode diode;
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(
        diode.current(-K_KNEE - K_KNEE_STEP), diode.current(-K_KNEE + K_KNEE_STEP), 4)));
}

TEST(Small_SiliconDiode, SiliconDiodeConductanceIsCOneAcrossTheLowerKnee)
{
    const SushiDSP::SiliconDiode diode;
    const double slope = diode.jacobian(-K_KNEE + K_KNEE_STEP);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(diode.jacobian(-K_KNEE - K_KNEE_STEP), slope, knee_slope_bound(slope))));
}

class Small_SiliconDiodeJoint : public ::testing::TestWithParam<Voltage>
{
};

// The joint form is the code the separate entry points call, so each value is identical.
TEST_P(Small_SiliconDiodeJoint, AgreesWithTheSeparateEntryPoints)
{
    const SushiDSP::SiliconDiode diode;
    double current = 0.0;
    double conductance = 0.0;
    diode.current_and_jacobian(GetParam().volts, current, conductance);
    EXPECT_EQ(current, diode.current(GetParam().volts));
    EXPECT_EQ(conductance, diode.jacobian(GetParam().volts));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_SiliconDiodeJoint, ::testing::ValuesIn(kJointVoltages), row_name);

class Small_SiliconDiodeAntiparallel : public ::testing::TestWithParam<PairRow>
{
};

// The pair adds a forward and a reverse diode with no cancellation, so the model's own error is far below half a unit of the tenth printed digit.
TEST_P(Small_SiliconDiodeAntiparallel, PairMatchesTwoTimesSaturationCurrentTimesSinh)
{
    const SushiDSP::SiliconDiode diode;
    const PairRow& row = GetParam();
    double forward_current = 0.0;
    double forward_conductance = 0.0;
    double reverse_current = 0.0;
    double reverse_conductance = 0.0;
    diode.current_and_jacobian(row.volts, forward_current, forward_conductance);
    diode.current_and_jacobian(-row.volts, reverse_current, reverse_conductance);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        forward_current - reverse_current, row.current, printed_digits_bound(row.current, K_PAIR_DIGITS))));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        forward_conductance + reverse_conductance, row.conductance,
        printed_digits_bound(row.conductance, K_PAIR_DIGITS))));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_SiliconDiodeAntiparallel, ::testing::ValuesIn(kPairRows),
                         [](const ::testing::TestParamInfo<PairRow>& info) { return std::string(info.param.name); });

TEST(Small_SiliconDiode, SiliconDiodeHonoursANonDefaultIdealityFactor)
{
    // n = 1 makes n*Vt = 0.025864 V. See tests/foundation/circuits/models/README.md.
    const SushiDSP::SiliconDiode ideal(2.52e-9, 1.0, 0.025864);
    const SushiDSP::SiliconDiode part;
    EXPECT_GT(ideal.current(0.6), 100.0 * part.current(0.6));
}

class Small_SiliconDiodeAntiderivative : public ::testing::TestWithParam<Voltage>
{
};

TEST_P(Small_SiliconDiodeAntiderivative, DifferentiatesBackToTheDiodeCurrent)
{
    const SushiDSP::SiliconDiode diode;
    constexpr double delta = 1.0e-7;
    const double voltage = GetParam().volts;
    const double slope =
        (diode.current_antiderivative(voltage + delta) - diode.current_antiderivative(voltage - delta)) / (2.0 * delta);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        slope, diode.current(voltage), antiderivative_difference_bound(diode, voltage, delta))));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_SiliconDiodeAntiderivative, ::testing::ValuesIn(kAntiderivativeVoltages),
                         row_name);

class Small_SiliconDiodeKnee : public ::testing::TestWithParam<Voltage>
{
};

// By the mean value theorem the step across [a, b] is at most (b - a) times the steepest current, where a and b are the edge plus and minus delta as stored: at 1.8 V they are 2.2e-7 of 2 delta off it, so the width is read from the stored points. Each antiderivative adds the rounding of the exponential side, (3 n + 4) u of itself at the exponent argument n = 40, and the other side is counted the same.
TEST_P(Small_SiliconDiodeKnee, AntiderivativeHasNoStepAcrossIt)
{
    const SushiDSP::SiliconDiode diode;
    constexpr double delta = 1.0e-9;
    const double edge = GetParam().volts;
    const double lower = edge - delta;
    const double upper = edge + delta;
    const double below = diode.current_antiderivative(lower);
    const double above = diode.current_antiderivative(upper);
    const double steepest = std::max(std::fabs(diode.current(lower)), std::fabs(diode.current(upper)));
    const double bound =
        (upper - lower) * steepest + (3.0 * 40.0 + 4.0) * kU * (std::fabs(below) + std::fabs(above));

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(above - below, 0.0, bound, 1.0)));
}

INSTANTIATE_TEST_SUITE_P(Edges, Small_SiliconDiodeKnee, ::testing::ValuesIn(kKnees), row_name);

class Small_SiliconDiodeJointAntiderivative : public ::testing::TestWithParam<Voltage>
{
};

TEST_P(Small_SiliconDiodeJointAntiderivative, AgreesWithTheSeparateEntryPoints)
{
    const SushiDSP::SiliconDiode diode;
    double current = 0.0;
    double antiderivative = 0.0;
    diode.current_and_antiderivative(GetParam().volts, current, antiderivative);
    EXPECT_EQ(current, diode.current(GetParam().volts));
    EXPECT_EQ(antiderivative, diode.current_antiderivative(GetParam().volts));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_SiliconDiodeJointAntiderivative, ::testing::ValuesIn(kAntiderivativeJointVoltages),
                         row_name);
