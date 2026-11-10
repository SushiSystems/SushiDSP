/**************************************************************************/
/* test_behaviour.cpp                                                     */
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
 * @file test_behaviour.cpp
 * @brief Tests TriodeStage and the AntiderivativeAntialiaser it runs through.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/stages/antiderivative_antialiaser.hpp>
#include <SushiDSP/foundation/circuits/stages/power_tube_presets.hpp>
#include <SushiDSP/foundation/circuits/stages/triode_stage.hpp>

namespace
{
    const double kU = TestSupport::unit_roundoff<float>();
    const double kUDouble = TestSupport::unit_roundoff<double>();

    /// @brief The six constants of one stage and the name its rows are reported under.
    struct Circuit
    {
            const char* name;
            float source_resistance;
            float grid_leak_resistance;
            float cathode_bias;
            float gain;
            SushiDSP::GridConductionLaw law;
            SushiDSP::PlateSwing swing;
    };

    // These constants are the JCM800's V1a-shaped stage. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    constexpr Circuit K_JCM{"Jcm",
                            10000.0f,
                            1000000.0f,
                            -1.5f,
                            60.0f,
                            SushiDSP::K_TRIODE12_AX7_GRID_CONDUCTION,
                            SushiDSP::PlateSwing{194.0f, 115.0f}};

    /// @brief The 5150's V5A stage, whose plate reaches its supply headroom.
    constexpr Circuit K_V5A{"V5a",
                            671730.0f,
                            1000000.0f,
                            -2.0f,
                            25.3f,
                            SushiDSP::K_TRIODE12_AX7_GRID_CONDUCTION,
                            SushiDSP::PlateSwing{90.742f, 134.001f}};

    /// @brief The EL34 power stage, whose antialiaser tables the same identities must hold on.
    constexpr Circuit K_EL34{"El34",
                             33831.9f,
                             220000.0f,
                             -38.0f,
                             6.882361f,
                             SushiDSP::K_PENTODE_EL34_GRID_CONDUCTION,
                             SushiDSP::PlateSwing{25.5f, 280.2777f}};

    SushiDSP::TriodeStage make_stage(const Circuit& c = K_JCM)
    {
        return SushiDSP::TriodeStage(c.source_resistance, c.grid_leak_resistance, c.cathode_bias,
                                     c.gain, c.law, c.swing);
    }

    SushiDSP::AntiderivativeAntialiaser make_antialiaser(const Circuit& c = K_JCM)
    {
        return SushiDSP::AntiderivativeAntialiaser(c.source_resistance, c.grid_leak_resistance,
                                                   c.cathode_bias, c.gain, c.law, c.swing);
    }

    /// @brief The grid-leak divider Rg / (Rs + Rg) the grid follows while it draws no current.
    double divider(const Circuit& c)
    {
        return static_cast<double>(c.grid_leak_resistance) /
               (static_cast<double>(c.grid_leak_resistance) + c.source_resistance);
    }

    /// @brief The knee's centre as a grid excursion Vg - Vk.
    double knee(const Circuit& c)
    {
        return static_cast<double>(c.law.threshold_volts) - c.cathode_bias;
    }

    double largest_headroom(const Circuit& c)
    {
        return std::max(c.swing.headroom_to_supply_volts, c.swing.headroom_to_bottom_volts);
    }

    double smallest_headroom(const Circuit& c)
    {
        return std::min(c.swing.headroom_to_supply_volts, c.swing.headroom_to_bottom_volts);
    }

    /// @brief The conducting slope of the grid law times the source resistance: the volts the knee adds per volt of grid.
    double conduction_slope(const Circuit& c)
    {
        return static_cast<double>(c.source_resistance) * c.law.scale_amps / c.law.transition_volts;
    }

    /// @brief The softplus ln(1 + e^x) in double, written so that neither tail overflows.
    double softplus(double x)
    {
        return x > 0.0 ? x + std::log1p(std::exp(-x)) : std::log1p(std::exp(x));
    }

    /// @brief Solves the stage's own grid node by bisection in double, independently of the Newton solver.
    /// @return The signed excursion Vg - Vk at the root of the KCL the header states, for a DC input.
    double reference_excursion(const Circuit& c, double input)
    {
        double low = c.cathode_bias - 1000.0;
        double high = c.cathode_bias + 1000.0;
        for (int step = 0; step < 64; ++step)
        {
            const double middle = 0.5 * (low + high);
            const double residual =
                (middle - (c.cathode_bias + input)) / c.source_resistance +
                (middle - c.cathode_bias) / c.grid_leak_resistance +
                c.law.scale_amps *
                    softplus((middle - c.law.threshold_volts) / c.law.transition_volts);
            (residual > 0.0 ? high : low) = middle;
        }
        return 0.5 * (low + high) - c.cathode_bias;
    }

    /// @brief The plate output at one grid excursion, bound(-gain * u).
    double plate_at(const Circuit& c, double excursion)
    {
        return c.swing.bound(-static_cast<double>(c.gain) * excursion);
    }

    /// @brief The composite transfer at one input: the bisected grid solve through the plate bound.
    double composite_transfer(const Circuit& c, double input)
    {
        return plate_at(c, reference_excursion(c, input));
    }

    // The bisection stops at the spacing of doubles, so its residual terms (|Vg| + |Vs| + |bias|) / R each round at u_double, and dividing by the residual's slope (about 1 / Rs) leaves 16 u_double (|Vg| + |Vs| + |bias|) volts at the grid; the input map multiplies it by its slope, at most 1.1 at these circuits.
    double reference_grid_bound(const Circuit& c, double input, double excursion)
    {
        const double bias = std::fabs(c.cathode_bias);
        return 1.1 * 16.0 * kUDouble * (std::fabs(bias + excursion) + std::fabs(bias + input) + bias);
    }

    // The float solve rounds the source voltage, the grid voltage and the residual's divider terms once each, like the bisection, at u_float: 8 u (|bias| + |input| + |grid|) volts at the grid.
    double float_grid_bound(const Circuit& c, double input, double excursion)
    {
        const double bias = std::fabs(c.cathode_bias);
        return 8.0 * kU * (bias + std::fabs(input) + std::fabs(c.cathode_bias + excursion));
    }

    // Newton on the node equation f, convex and increasing, satisfies e(k+1) <= C e(k)^2 with C = max f'' / (2 min f'), f'' = scale sigma'(x) / transition^2 on the interval from the warm start (the bias) to the root and min f' = 1 / Rs + 1 / Rg. Four iterations from e0 leave (C e0)^16 / C; a C e0 of 1 or more makes the derivation void, and the bound is NaN so the check fails.
    double newton_bound(const Circuit& c, double excursion)
    {
        const double transition = c.law.transition_volts;
        const double left = std::min(0.0, excursion) + c.cathode_bias;
        const double right = std::max(0.0, excursion) + c.cathode_bias;
        double sigma_slope = 0.25;
        if (c.law.threshold_volts < left || c.law.threshold_volts > right)
        {
            const double nearest = c.law.threshold_volts > right ? right : left;
            const double x = std::fabs(nearest - c.law.threshold_volts) / transition;
            sigma_slope = std::exp(-x) / ((1.0 + std::exp(-x)) * (1.0 + std::exp(-x)));
        }
        const double ratio = c.law.scale_amps * sigma_slope / (transition * transition) /
                             (2.0 * (1.0 / c.source_resistance + 1.0 / c.grid_leak_resistance));
        const double scaled = ratio * std::fabs(excursion);
        return scaled >= 1.0 ? std::numeric_limits<double>::quiet_NaN()
                             : std::pow(scaled, 16.0) / ratio;
    }

    /// @brief The error in volts of the stage's grid for a DC input: reference, float rounding and Newton.
    double grid_error(const Circuit& c, double input)
    {
        const double excursion = reference_excursion(c, input);
        return reference_grid_bound(c, input, excursion) + float_grid_bound(c, input, excursion) +
               newton_bound(c, excursion);
    }

    // The plate is the grid error times the gain (the bound's slope is at most 1) plus 4 u |plate| for the tanh, the product and the cast.
    double plate_error(const Circuit& c, double input)
    {
        return c.gain * grid_error(c, input) + 4.0 * kU * std::fabs(composite_transfer(c, input));
    }

    // The tabulated plate antiderivative is a cubic Hermite over knots h = H / gain / 64 apart; its slope error is at most h^3 / (72 sqrt 3) times the fourth derivative of the antiderivative, at most 2 gain^3 / H^2, which is H / (64^3 * 62.4) of the headroom. The tabulated knee has knots T / 16 apart and a fourth derivative of at most (conduction slope) H / (4 T^3) with the bound's own derivatives counted, so its slope error is conduction slope * H / (16^3 * 62.4 * 4). Both are multiplied by their input slope.
    double table_slope_bound(const Circuit& c)
    {
        const double h = largest_headroom(c);
        const double linear_slope = 1.0 + static_cast<double>(c.source_resistance) / c.grid_leak_resistance;
        return h * (linear_slope / (262144.0 * 62.4) + conduction_slope(c) / (4096.0 * 62.4 * 4.0));
    }

    // The same tables' value error is h^4 / 384 times the fourth derivative: H^2 / (64^4 * 192 gain) for the plate, T conduction slope H / (16^4 * 192) for the knee.
    double table_value_bound(const Circuit& c)
    {
        const double h = largest_headroom(c);
        const double linear_slope = 1.0 + static_cast<double>(c.source_resistance) / c.grid_leak_resistance;
        return linear_slope * h * h / (16777216.0 * 192.0 * c.gain) +
               c.law.transition_volts * conduction_slope(c) * h / (65536.0 * 192.0);
    }

    /// @brief Holds a name and a DC input in volts for the rows that run the stage at one input.
    struct InputRow
    {
            const char* name;
            double input_volts;
    };

    /// @brief Holds an excursion given as a count of transition widths from the knee of one circuit.
    struct KneeRow
    {
            const char* name;
            const Circuit* circuit;
            double transitions;
    };

    /// @brief Holds an input interval for the divided-difference rows.
    struct IntervalRow
    {
            const char* name;
            double low;
            double high;
    };

    std::string row_name(const InputRow& row)
    {
        return row.name;
    }

    std::string row_name(const KneeRow& row)
    {
        return row.name;
    }

    std::string row_name(const IntervalRow& row)
    {
        return row.name;
    }

    /// @brief Names a parameterised case after its row.
    template <typename Row> std::string name_of(const ::testing::TestParamInfo<Row>& info)
    {
        return row_name(info.param);
    }

    const InputRow kNormanInputs[] = {{"BelowTheKnee", 0.1}, {"OnTheKnee", 1.5}, {"PastTheKnee", 3.0}, {"WellPastTheKnee", 8.0}};

    const InputRow kSwingInputs[] = {{"TowardTheSupply", -10.0}, {"TowardTheBottom", 10.0}};

    const InputRow kMapInputs[] = {{"MinusThirty", -30.0}, {"MinusThree", -3.0}, {"MinusOneTenth", -0.1},
                                   {"OneTenth", 0.1},      {"OnePointFive", 1.5}, {"Two", 2.0},
                                   {"TwoPointFive", 2.5},  {"Three", 3.0},        {"Eight", 8.0},
                                   {"Thirty", 30.0}};

    const KneeRow kSlopeRows[] = {
        {"JcmMinusForty", &K_JCM, -40.0}, {"JcmMinusFive", &K_JCM, -5.0}, {"JcmAtTheKnee", &K_JCM, 0.0},
        {"JcmPlusFive", &K_JCM, 5.0},     {"JcmPlusForty", &K_JCM, 40.0},  {"El34MinusForty", &K_EL34, -40.0},
        {"El34MinusFive", &K_EL34, -5.0}, {"El34AtTheKnee", &K_EL34, 0.0}, {"El34PlusFive", &K_EL34, 5.0},
        {"El34PlusForty", &K_EL34, 40.0},
    };

    const KneeRow kBetweenKnotRows[] = {
        {"MinusTwenty", &K_JCM, -20.0}, {"MinusThree", &K_JCM, -3.0}, {"AtTheKnee", &K_JCM, 0.0},
        {"PlusThree", &K_JCM, 3.0},     {"PlusTwenty", &K_JCM, 20.0},
    };

    const IntervalRow kIntervals[] = {
        {"InsideTheKnee", -1.0, 1.0},
        {"AcrossTheKnee", 0.0, 6.0},
        {"AcrossBothCeilings", -2.0, 4.0},
        {"WholeRange", -30.0, 30.0},
        {"NarrowPastTheKnee", 4.0, 4.01},
    };

    /// @brief Drives a stage to a DC input and returns its settled plate output.
    float settled_plate(SushiDSP::TriodeStage& stage, float input_voltage)
    {
        float output = 0.0f;
        for (int i = 0; i < 64; ++i)
        {
            output = stage.process_sample(input_voltage);
        }
        return output;
    }

    /// @brief A staircase of inputs from -6 V to +6 V and back in steps of 0.5 V, 49 samples, wide enough apart for the divided difference to be well conditioned.
    std::vector<float> staircase()
    {
        std::vector<float> inputs;
        for (int step = 0; step <= 24; ++step)
        {
            inputs.push_back(-6.0f + 0.5f * static_cast<float>(step));
        }
        for (int step = 23; step >= 0; --step)
        {
            inputs.push_back(-6.0f + 0.5f * static_cast<float>(step));
        }
        return inputs;
    }

    /// @brief Asserts that each stage output sits within its derived bound of the antialiaser's reading of the same grid solve.
    ///
    /// The divided difference over a step of at least 0.5 V turns a grid error e into at most 2 (H + 1.1 |out|) e / 0.5, plus the output's own 4 u |out|.
    void expect_outputs_follow_the_antialiaser(const std::vector<float>& inputs)
    {
        SushiDSP::TriodeStage stage = make_stage();
        SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
        for (const float input : inputs)
        {
            const float produced = stage.process_sample(input);
            const float reference =
                antialiaser.process(static_cast<float>(reference_excursion(K_JCM, input)));
            const double bound =
                2.0 * (largest_headroom(K_JCM) + 1.1 * std::fabs(reference)) *
                    grid_error(K_JCM, input) / 0.5 +
                4.0 * kU * std::fabs(reference);
            EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(produced, reference, bound)))
                << "input " << input;
        }
    }

    /// @brief Drives @p stage with a ramp from -50 V to +50 V, through cutoff and grid conduction.
    template <typename Stage> void drive_ramp_through_conduction(Stage& stage)
    {
        for (int sample = 0; sample <= 1000; ++sample)
        {
            static_cast<void>(stage.process_sample(-50.0f + 0.1f * static_cast<float>(sample)));
        }
    }

    /// @brief The antialiaser's second output for a pair of inputs that differ by @p difference, after a reset.
    double second_output(SushiDSP::AntiderivativeAntialiaser& antialiaser, double base, double difference)
    {
        antialiaser.reset();
        antialiaser.process(static_cast<float>(reference_excursion(K_JCM, base)));
        return antialiaser.process(static_cast<float>(reference_excursion(K_JCM, base + difference)));
    }
} // namespace

TEST(Small_TriodeStage, TriodeStageSettlesToOneOutputForConstantInput)
{
    SushiDSP::TriodeStage stage = make_stage();
    static_cast<void>(settled_plate(stage, 0.1f));
    static_cast<void>(settled_plate(stage, 0.1f));
    const float first = stage.process_sample(0.1f);
    const float second = stage.process_sample(0.1f);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(first, second, plate_error(K_JCM, 0.1))));
}

TEST(Small_TriodeStage, TriodeStageIsInvertingForSmallSignal)
{
    SushiDSP::TriodeStage stage = make_stage();
    EXPECT_LT(settled_plate(stage, 0.05f), 0.0f);
}

TEST(Small_TriodeStage, TriodeStageGridFollowsTheSourceDividerBelowTheCathode)
{
    // 3 V in is 2.9703 V of grid excursion. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    SushiDSP::TriodeStage stage = make_stage();
    static_cast<void>(settled_plate(stage, -3.0f));
    const double expected = 3.0 * divider(K_JCM);
    const double grid = K_JCM.cathode_bias - expected;
    const double tail_volts = K_JCM.source_resistance * K_JCM.law.scale_amps *
                              softplus((grid - K_JCM.law.threshold_volts) / K_JCM.law.transition_volts);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        stage.peak_grid_excursion_volts(), expected, grid_error(K_JCM, -3.0) + tail_volts)));
}

class Small_TriodeStageNorman : public ::testing::TestWithParam<InputRow>
{
};

TEST_P(Small_TriodeStageNorman, GridRootIsTheRootOfTheNormanNodeEquation)
{
    // The root is checked against an independent bisection. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    const double input = GetParam().input_volts;
    SushiDSP::TriodeStage stage = make_stage();
    static_cast<void>(settled_plate(stage, static_cast<float>(input)));

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        stage.peak_grid_excursion_volts(), std::fabs(reference_excursion(K_JCM, input)),
        grid_error(K_JCM, input))));
}

INSTANTIATE_TEST_SUITE_P(Inputs, Small_TriodeStageNorman, ::testing::ValuesIn(kNormanInputs), name_of<InputRow>);

TEST(Small_TriodeStage, TriodeStageResetReturnsToCathodeBiasWarmStart)
{
    SushiDSP::TriodeStage stage_a = make_stage();
    SushiDSP::TriodeStage stage_b = make_stage();

    stage_a.process_sample(1.0f);
    stage_a.reset();

    // Both stages run the same operations from the same state, so the outputs are the same float.
    EXPECT_EQ(stage_a.process_sample(0.01f), stage_b.process_sample(0.01f));
}

class Small_TriodeStageSwing : public ::testing::TestWithParam<InputRow>
{
};

TEST_P(Small_TriodeStageSwing, PlateSettlesToTheBoundedLinearAnswerInsideItsHeadroom)
{
    // The two sides differ by the swing alone. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    const double input = GetParam().input_volts;
    SushiDSP::TriodeStage stage = make_stage();

    const float plate = settled_plate(stage, static_cast<float>(input));

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        plate, composite_transfer(K_JCM, input), plate_error(K_JCM, input))));
    EXPECT_LT(plate, K_JCM.swing.headroom_to_supply_volts);
    EXPECT_GT(plate, -K_JCM.swing.headroom_to_bottom_volts);
}

INSTANTIATE_TEST_SUITE_P(Sides, Small_TriodeStageSwing, ::testing::ValuesIn(kSwingInputs), name_of<InputRow>);

TEST(Small_TriodeStage, TriodeStageCutoffIsThePlateCeiling)
{
    // No solver fence holds the grid short of the ceiling. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    SushiDSP::TriodeStage v5a = make_stage(K_V5A);
    const float plate = settled_plate(v5a, -30.0f);
    // The tanh sits 2 H e^(-2 z) under its ceiling at z = gain |excursion| / H.
    const double headroom = K_V5A.swing.headroom_to_supply_volts;
    const double z = K_V5A.gain * std::fabs(reference_excursion(K_V5A, -30.0)) / headroom;
    const double bound = plate_error(K_V5A, -30.0) + 2.0 * headroom * std::exp(-2.0 * z);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(plate, headroom, bound)));
    EXPECT_LE(plate, K_V5A.swing.headroom_to_supply_volts);
}

TEST(Small_TriodeStage, TriodeStageSmallSignalGainIsUnchangedByThePlateSwing)
{
    // 0.01 V through a gain of 60 is 0.6 V, less the 1 % the Rs/Rg source divider takes.
    SushiDSP::TriodeStage stage = make_stage();
    const double input = 0.01;
    const double linear = -K_JCM.gain * divider(K_JCM) * input;
    const double smallest = smallest_headroom(K_JCM);
    // tanh(z) differs from z by at most z^3 / 3, and the grid law adds a tail current of the knee's softplus below the cathode.
    const double tail_volts = K_JCM.source_resistance * K_JCM.law.scale_amps *
                              softplus((K_JCM.cathode_bias + input * divider(K_JCM) - K_JCM.law.threshold_volts) /
                                       K_JCM.law.transition_volts);
    const double bound = plate_error(K_JCM, input) + std::pow(std::fabs(linear), 3.0) / (3.0 * smallest * smallest) +
                         K_JCM.gain * tail_volts;

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(settled_plate(stage, static_cast<float>(input)), linear, bound)));
}

TEST(Small_TriodeStage, TriodeStagePeakGridExcursionKeepsTheLargestSinceReset)
{
    SushiDSP::TriodeStage stage = make_stage();
    static_cast<void>(settled_plate(stage, -2.0f));
    static_cast<void>(settled_plate(stage, -0.5f));
    const double expected = 2.0 * divider(K_JCM);
    const double grid = K_JCM.cathode_bias - expected;
    const double tail_volts = K_JCM.source_resistance * K_JCM.law.scale_amps *
                              softplus((grid - K_JCM.law.threshold_volts) / K_JCM.law.transition_volts);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        stage.peak_grid_excursion_volts(), expected, grid_error(K_JCM, -2.0) + tail_volts)));
}

TEST(Small_TriodeStage, TriodeStageResetClearsThePeakGridExcursion)
{
    SushiDSP::TriodeStage stage = make_stage();
    static_cast<void>(settled_plate(stage, -2.0f));
    ASSERT_GT(stage.peak_grid_excursion_volts(), 0.0f);

    stage.reset();

    EXPECT_EQ(stage.peak_grid_excursion_volts(), 0.0f);
}

TEST(Small_TriodeStage, TriodeStageGridLawIsTheTriode12AX7Model)
{
    // The stage's constants are read from Triode12AX7, so the two cannot drift apart.
    EXPECT_FLOAT_EQ(SushiDSP::K_TRIODE12_AX7_GRID_CONDUCTION.scale_amps,
                    static_cast<float>(SushiDSP::Triode12AX7::K_GRID_CURRENT_SCALE));
    EXPECT_FLOAT_EQ(SushiDSP::K_TRIODE12_AX7_GRID_CONDUCTION.threshold_volts,
                    static_cast<float>(SushiDSP::Triode12AX7::K_GRID_THRESHOLD_VOLTS));
    EXPECT_FLOAT_EQ(SushiDSP::K_TRIODE12_AX7_GRID_CONDUCTION.transition_volts,
                    static_cast<float>(SushiDSP::Triode12AX7::K_GRID_TRANSITION_VOLTS));
}

class Small_TriodeStageInputMap : public ::testing::TestWithParam<InputRow>
{
};

TEST_P(Small_TriodeStageInputMap, AntiderivativeAntialiaserInputMapInvertsTheGridSolve)
{
    // g(u) composed with the bisected solve is the identity. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    const double input = GetParam().input_volts;
    const SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
    const double excursion = reference_excursion(K_JCM, input);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        antialiaser.input_for_excursion(excursion), input, reference_grid_bound(K_JCM, input, excursion))));
}

INSTANTIATE_TEST_SUITE_P(Inputs, Small_TriodeStageInputMap, ::testing::ValuesIn(kMapInputs), name_of<InputRow>);

class Small_TriodeStageDerivative : public ::testing::TestWithParam<KneeRow>
{
};

TEST_P(Small_TriodeStageDerivative, AntiderivativeAntialiaserAntiderivativeDifferentiatesBackToTheTransfer)
{
    // dG/du equals bound(-gain u) times dg/du everywhere. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    const Circuit& c = *GetParam().circuit;
    const SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser(c);
    const double excursion = knee(c) + GetParam().transitions * c.law.transition_volts;
    constexpr double K_DELTA = 1e-4;
    const double numeric_slope = (antialiaser.antiderivative(excursion + K_DELTA) -
                                  antialiaser.antiderivative(excursion - K_DELTA)) /
                                 (2.0 * K_DELTA);
    const double input_slope = (antialiaser.input_for_excursion(excursion + K_DELTA) -
                                antialiaser.input_for_excursion(excursion - K_DELTA)) /
                               (2.0 * K_DELTA);
    // A centred difference truncates at delta^2 / 6 of the third derivative of G, here at most 4 * 0.77 gain^2 / H_min times the input slope; its rounding is 8 u_double over the two values of G, over 2 delta.
    const double values = std::fabs(antialiaser.antiderivative(excursion + K_DELTA)) +
                          std::fabs(antialiaser.antiderivative(excursion - K_DELTA));
    const double numeric_bound = K_DELTA * K_DELTA / 6.0 * 4.0 * 0.77 * c.gain * c.gain / smallest_headroom(c) * input_slope +
                                 8.0 * kUDouble * values / (2.0 * K_DELTA);
    const double rounding_of_slope = largest_headroom(c) * 8.0 * kUDouble * 2.0 * std::fabs(excursion) / (2.0 * K_DELTA);
    const double bound = numeric_bound + rounding_of_slope + table_slope_bound(c);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        numeric_slope, plate_at(c, excursion) * input_slope, bound)))
        << "excursion " << excursion;
}

INSTANTIATE_TEST_SUITE_P(Knee, Small_TriodeStageDerivative, ::testing::ValuesIn(kSlopeRows), name_of<KneeRow>);

class Small_TriodeStageBetweenKnots : public ::testing::TestWithParam<KneeRow>
{
};

TEST_P(Small_TriodeStageBetweenKnots, AntiderivativeAntialiaserTransferMatchesThePlateBoundBetweenKnots)
{
    // Between knots only the Hermite slope error remains. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    const Circuit& c = *GetParam().circuit;
    const SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser(c);
    const double half_step =
        c.law.transition_volts / SushiDSP::AntiderivativeAntialiaser::K_INTERVALS_PER_TRANSITION / 2.0;
    const double excursion = knee(c) + GetParam().transitions * c.law.transition_volts + half_step;

    // The transfer is the table's slope over the input slope, which is at least 1.
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        antialiaser.transfer(excursion), plate_at(c, excursion), table_slope_bound(c))));
}

INSTANTIATE_TEST_SUITE_P(Knee, Small_TriodeStageBetweenKnots, ::testing::ValuesIn(kBetweenKnotRows), name_of<KneeRow>);

class Small_TriodeStageDividedDifference : public ::testing::TestWithParam<IntervalRow>
{
};

TEST_P(Small_TriodeStageDividedDifference, AntiderivativeAntialiaserDividedDifferenceIsTheMeanOfTheCompositeTransfer)
{
    // The divided difference of G is the mean transfer. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    const IntervalRow& interval = GetParam();
    const SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
    const double low_excursion = reference_excursion(K_JCM, interval.low);
    const double high_excursion = reference_excursion(K_JCM, interval.high);
    const double input_difference = antialiaser.input_for_excursion(high_excursion) -
                                    antialiaser.input_for_excursion(low_excursion);
    const double divided_difference =
        (antialiaser.antiderivative(high_excursion) - antialiaser.antiderivative(low_excursion)) / input_difference;
    constexpr int K_PANELS = 20000;
    const double step = (interval.high - interval.low) / K_PANELS;
    double sum = composite_transfer(K_JCM, interval.low) + composite_transfer(K_JCM, interval.high);
    for (int i = 1; i < K_PANELS; ++i)
    {
        sum += (i % 2 == 1 ? 4.0 : 2.0) * composite_transfer(K_JCM, interval.low + i * step);
    }
    const double mean = sum * step / 3.0 / (interval.high - interval.low);
    // Simpson's rule errs by step^4 / 180 times the fourth derivative of the transfer, at most H / T^4 in the input where the knee is T wide; the two table values err by table_value_bound each, over the input difference; the bisected grids add the reference bound times the transfer's slope gain.
    const double simpson_bound = std::pow(step, 4.0) / 180.0 * largest_headroom(K_JCM) /
                                 std::pow(K_JCM.law.transition_volts, 4.0);
    const double table_bound = 2.0 * table_value_bound(K_JCM) / std::fabs(input_difference);
    const double grid_bound = K_JCM.gain * (reference_grid_bound(K_JCM, interval.low, low_excursion) +
                                            reference_grid_bound(K_JCM, interval.high, high_excursion));

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(divided_difference, mean, simpson_bound + table_bound + grid_bound)));
}

INSTANTIATE_TEST_SUITE_P(Intervals, Small_TriodeStageDividedDifference, ::testing::ValuesIn(kIntervals),
                         name_of<IntervalRow>);

TEST(Small_TriodeStage, AntiderivativeAntialiaserRepeatedExcursionReturnsThePlateBound)
{
    // Near-equal samples fall back to the midpoint transfer. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
    const float excursion = static_cast<float>(reference_excursion(K_JCM, 1.2 * knee(K_JCM)));
    const double plate = plate_at(K_JCM, excursion);

    antialiaser.process(excursion);
    const float second = antialiaser.process(excursion);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(second, plate, table_slope_bound(K_JCM) + kU * std::fabs(plate))));
}

TEST(Small_TriodeStage, AntiderivativeAntialiaserSamplesOneUlpApartReturnThePlateBound)
{
    SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
    const float excursion = static_cast<float>(reference_excursion(K_JCM, 1.2 * knee(K_JCM)));
    const float next = std::nextafter(excursion, 1e9f);
    const double plate = plate_at(K_JCM, next);
    // The midpoint is half an ulp from the second sample, and the plate moves at most gain per volt.
    const double ulp = static_cast<double>(next) - excursion;

    antialiaser.process(excursion);
    const float second = antialiaser.process(next);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        second, plate, table_slope_bound(K_JCM) + kU * std::fabs(plate) + K_JCM.gain * ulp)));
}

TEST(Small_TriodeStage, AntiderivativeAntialiaserOutputIsContinuousAcrossTheFallbackThreshold)
{
    // Two input differences 1 % either side of the threshold give outputs that differ by the plate's own movement, the branches' 1e-7 V agreement the header promises and the output's rounding.
    SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
    const double base = 1.2 * knee(K_JCM);
    const double threshold = SushiDSP::AntiderivativeAntialiaser::K_MINIMUM_INPUT_DIFFERENCE_VOLTS;

    const double under = second_output(antialiaser, base, 0.99 * threshold);
    const double over = second_output(antialiaser, base, 1.01 * threshold);

    // The inputs differ by 2 % of the threshold; the grid follows at most the same, and the plate moves at most gain per volt of grid. Each excursion is a float, so add gain ulps of the excursion for both.
    const double movement = K_JCM.gain * 0.02 * threshold;
    const double quantisation = 2.0 * K_JCM.gain * kU * std::fabs(reference_excursion(K_JCM, base));
    const double bound = 1e-7 + movement + quantisation + 2.0 * kU * std::fabs(over);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(over, under, bound)));
}

TEST(Small_TriodeStage, TriodeStageOutputIsTheAntialiaserOfItsOwnGridSolve)
{
    // The stage's output is its antialiaser's output. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    expect_outputs_follow_the_antialiaser(staircase());
}

TEST(Small_TriodeStage, TriodeStageHandsOverTheNewtonSolvesItRan)
{
    SushiDSP::TriodeStage stage = make_stage();
    drive_ramp_through_conduction(stage);
    SushiDSP::NewtonRaphsonSolver::Tally tally;

    stage.take_newton_tally(tally);

    EXPECT_GT(tally.solves, 0U);
}

TEST(Small_TriodeStage, TriodeStageClearsItsNewtonSolvesOnceItHandsThemOver)
{
    SushiDSP::TriodeStage stage = make_stage();
    drive_ramp_through_conduction(stage);
    SushiDSP::NewtonRaphsonSolver::Tally first;
    stage.take_newton_tally(first);
    SushiDSP::NewtonRaphsonSolver::Tally second;

    stage.take_newton_tally(second);

    EXPECT_EQ(second.solves, 0U);
}
