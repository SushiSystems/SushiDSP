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
 * @brief Tests the saturation, grid conduction and push-pull handover of PentodeStage.
 * @author Mustafa Garip
 */

#include <cmath>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/stages/pentode_stage.hpp>

namespace
{
    constexpr double K_PI = 3.14159265358979323846;

    const double kU = TestSupport::unit_roundoff<float>();

    /// @brief A reference pentode for exercising the stage itself.
    ///
    /// The figures are the Philips EL34 datasheet's class B push-pull point, which is a real
    /// operating point rather than any product's tuning. A brick's test that reached into an
    /// amplifier module would fail whenever that amplifier was recalibrated.
    constexpr SushiDSP::PowerTubePreset K_REFERENCE_PENTODE{
        33831.9f,
        220000.0f,
        -38.0f,
        6.882361f,
        SushiDSP::K_PENTODE_EL34_GRID_CONDUCTION,
        SushiDSP::PlateSwing{25.5f, 280.2777f}};

    /// @brief Builds a stage from a preset's fields with another cathode bias.
    SushiDSP::PentodeStage stage_with_bias(float bias_volts)
    {
        return SushiDSP::PentodeStage(K_REFERENCE_PENTODE.source_resistance_ohms,
                                      K_REFERENCE_PENTODE.grid_leak_resistance_ohms, bias_volts,
                                      K_REFERENCE_PENTODE.gain, K_REFERENCE_PENTODE.grid_conduction,
                                      K_REFERENCE_PENTODE.plate_swing);
    }

    /// @brief The stage's settled output for a DC input.
    float settled(SushiDSP::PentodeStage& stage, float input, int samples = 100)
    {
        float output = 0.0f;
        for (int i = 0; i < samples; ++i)
        {
            output = stage.process_sample(input);
        }
        return output;
    }

    /// @brief The softplus ln(1 + e^x) in double, written so that neither tail overflows.
    double softplus(double x)
    {
        return x > 0.0 ? x + std::log1p(std::exp(-x)) : std::log1p(std::exp(x));
    }

    /// @brief The grid voltage the node equation settles to, solved by bisection in double.
    ///
    /// The residual is increasing in the grid voltage, so the bisection brackets the one root.
    /// The interval [-1000, 1000] holds every root of the inputs below, and 200 halvings reach the
    /// spacing of doubles.
    double reference_grid_volts(float bias_volts, double input_volts)
    {
        const SushiDSP::PowerTubePreset& tube = K_REFERENCE_PENTODE;
        const double source = static_cast<double>(bias_volts) + input_volts;
        double low = -1000.0;
        double high = 1000.0;
        for (int iteration = 0; iteration < 200; ++iteration)
        {
            const double middle = 0.5 * (low + high);
            const double residual =
                (middle - source) / tube.source_resistance_ohms +
                (middle - bias_volts) / tube.grid_leak_resistance_ohms +
                tube.grid_conduction.scale_amps *
                    softplus((middle - tube.grid_conduction.threshold_volts) /
                             tube.grid_conduction.transition_volts);
            (residual > 0.0 ? high : low) = middle;
        }
        return 0.5 * (low + high);
    }

    /// @brief The plate voltage the settled stage should read, from the reference grid solve.
    double reference_plate_volts(float bias_volts, double input_volts)
    {
        const double excursion = reference_grid_volts(bias_volts, input_volts) - bias_volts;
        return K_REFERENCE_PENTODE.plate_swing.bound(-static_cast<double>(K_REFERENCE_PENTODE.gain) *
                                                     excursion);
    }

    // The float grid root is the rounded zero of a residual whose terms carry u relative error: the divider currents are at most (|bias| + |input| + |grid|) / Rs against a slope of about 1 / Rs, and the source voltage, the grid voltage and their difference each round once, so 8 u (|bias| + |input| + |grid|) holds the grid; the plate multiplies it by the gain (the bound's slope is at most 1) and adds its own 4 u |plate| for the tanh, the product and the cast.
    double plate_bound(float bias_volts, double input_volts, double plate_volts)
    {
        const double grid = reference_grid_volts(bias_volts, input_volts);
        const double grid_error =
            8.0 * kU * (std::fabs(bias_volts) + std::fabs(input_volts) + std::fabs(grid));
        return K_REFERENCE_PENTODE.gain * grid_error + 4.0 * kU * std::fabs(plate_volts);
    }

    /// @brief Holds one DC input for the rows that compare a settled output with its reference.
    struct DcRow
    {
            const char* name;
            float input_volts;
    };

    /// @brief Holds one saturation row: an input far past a side, and that side's signed headroom.
    struct SaturationRow
    {
            const char* name;
            float input_volts;
            double signed_headroom_volts;
    };

    const SaturationRow kSaturationRows[] = {
        {"TowardTheSupply", -400.0f, K_REFERENCE_PENTODE.plate_swing.headroom_to_supply_volts},
        {"TowardTheBottom", 400.0f, -K_REFERENCE_PENTODE.plate_swing.headroom_to_bottom_volts},
    };

    const DcRow kSmallInputs[] = {{"Positive", 0.1f}, {"Negative", -0.1f}};

    /// @brief The sine the preset constructor test feeds both stages: 256 samples at 440 Hz and 48 kHz.
    std::vector<float> loud_sine()
    {
        std::vector<float> samples;
        for (int i = 0; i < 256; ++i)
        {
            samples.push_back(
                40.0f * static_cast<float>(std::sin(2.0 * K_PI * 440.0 * i / 48000.0)));
        }
        return samples;
    }

    /// @brief The outputs of a stage for a list of inputs.
    std::vector<float> run(SushiDSP::PentodeStage& stage, const std::vector<float>& inputs)
    {
        std::vector<float> outputs;
        for (const float input : inputs)
        {
            outputs.push_back(stage.process_sample(input));
        }
        return outputs;
    }

    /// @brief Drives @p stage with a ramp from -50 V to +50 V, through cutoff and grid conduction.
    template <typename Stage> void drive_ramp_through_conduction(Stage& stage)
    {
        for (int sample = 0; sample <= 1000; ++sample)
        {
            static_cast<void>(stage.process_sample(-50.0f + 0.1f * static_cast<float>(sample)));
        }
    }

    /// @brief The even part of the push-pull composite at a drive: (c(u) + c(-u)) / 2 with c(u) = a(u) - b(-u).
    float even_part_of_pair(float bias_a, float bias_b, float drive)
    {
        SushiDSP::PentodeStage a_up = stage_with_bias(bias_a);
        SushiDSP::PentodeStage b_down = stage_with_bias(bias_b);
        SushiDSP::PentodeStage a_down = stage_with_bias(bias_a);
        SushiDSP::PentodeStage b_up = stage_with_bias(bias_b);
        const float forward = settled(a_up, drive, 64) - settled(b_down, -drive, 64);
        const float backward = settled(a_down, -drive, 64) - settled(b_up, drive, 64);
        return 0.5f * (forward + backward);
    }
} // namespace

TEST(Small_PentodeStage, PentodeStageSettlesToOneOutputForConstantInput)
{
    // Two successive settled outputs differ by at most the plate image of one grid rounding.
    SushiDSP::PentodeStage stage(K_REFERENCE_PENTODE);
    static_cast<void>(settled(stage, 1.0f, 1000));
    const float first = stage.process_sample(1.0f);
    const float second = stage.process_sample(1.0f);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(first, second, plate_bound(-38.0f, 1.0, second))));
}

class Small_PentodeStageSaturation : public ::testing::TestWithParam<SaturationRow>
{
};

TEST_P(Small_PentodeStageSaturation, PlateReachesItsOwnHeadroomAndNeverPastIt)
{
    // The plate is bounded by the tube's swing on each side. See tests/foundation/circuits/stages/test_pentode_stage/README.md.
    const SaturationRow& row = GetParam();
    SushiDSP::PentodeStage stage(K_REFERENCE_PENTODE);
    const double headroom = std::fabs(row.signed_headroom_volts);

    const float plate = settled(stage, row.input_volts);

    EXPECT_LE(std::fabs(plate), headroom);
    EXPECT_EQ(std::signbit(plate), std::signbit(row.signed_headroom_volts));
    // The solver's grid bounds keep the tanh argument at 10 or more on either side, where the ceiling sits 2 H e^(-20) inside the headroom.
    const double ceiling_gap = 2.0 * headroom * std::exp(-20.0) + 4.0 * kU * headroom;
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(plate, row.signed_headroom_volts, ceiling_gap)));
}

INSTANTIATE_TEST_SUITE_P(Sides, Small_PentodeStageSaturation, ::testing::ValuesIn(kSaturationRows),
                         [](const ::testing::TestParamInfo<SaturationRow>& info)
                         { return std::string(info.param.name); });

class Small_PentodeStageSmallInput : public ::testing::TestWithParam<DcRow>
{
};

TEST_P(Small_PentodeStageSmallInput, PlateIsTheGridDividerTimesTheGainThroughTheBound)
{
    // The grid sits at 0.867 of the input inside the window. See tests/foundation/circuits/stages/test_pentode_stage/README.md.
    const double input = GetParam().input_volts;
    SushiDSP::PentodeStage stage(K_REFERENCE_PENTODE);
    const double grid_divider =
        static_cast<double>(K_REFERENCE_PENTODE.grid_leak_resistance_ohms) /
        (static_cast<double>(K_REFERENCE_PENTODE.grid_leak_resistance_ohms) +
         K_REFERENCE_PENTODE.source_resistance_ohms);
    const double expected = K_REFERENCE_PENTODE.plate_swing.bound(
        -static_cast<double>(K_REFERENCE_PENTODE.gain) * grid_divider * input);

    const float plate = settled(stage, GetParam().input_volts, 50);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        plate, expected, plate_bound(K_REFERENCE_PENTODE.cathode_bias_volts, input, expected))));
}

INSTANTIATE_TEST_SUITE_P(Signs, Small_PentodeStageSmallInput, ::testing::ValuesIn(kSmallInputs),
                         [](const ::testing::TestParamInfo<DcRow>& info)
                         { return std::string(info.param.name); });

TEST(Small_PentodeStage, PentodeStageGridStaysCleanAcrossTheWholeBiasWindow)
{
    // The grid law is referenced to the cathode. See tests/foundation/circuits/stages/test_pentode_stage/README.md.
    SushiDSP::PentodeStage inside(K_REFERENCE_PENTODE);
    const double grid_divider =
        static_cast<double>(K_REFERENCE_PENTODE.grid_leak_resistance_ohms) /
        (static_cast<double>(K_REFERENCE_PENTODE.grid_leak_resistance_ohms) +
         K_REFERENCE_PENTODE.source_resistance_ohms);
    const double through_tanh = K_REFERENCE_PENTODE.plate_swing.bound(
        -static_cast<double>(K_REFERENCE_PENTODE.gain) * grid_divider * 30.0);

    const float plate = settled(inside, 30.0f);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        plate, through_tanh,
        plate_bound(K_REFERENCE_PENTODE.cathode_bias_volts, 30.0, through_tanh))));
}

TEST(Small_PentodeStage, PentodeStageGridConductsOnceTheInputCarriesItPastTheCathode)
{
    // A +30 V swing on a -20 V grid crosses the window and the plate is held short of the linear answer.
    SushiDSP::PentodeStage crossed = stage_with_bias(-20.0f);
    const double grid_divider =
        static_cast<double>(K_REFERENCE_PENTODE.grid_leak_resistance_ohms) /
        (static_cast<double>(K_REFERENCE_PENTODE.grid_leak_resistance_ohms) +
         K_REFERENCE_PENTODE.source_resistance_ohms);
    const double through_tanh = K_REFERENCE_PENTODE.plate_swing.bound(
        -static_cast<double>(K_REFERENCE_PENTODE.gain) * grid_divider * 30.0);
    const double expected = reference_plate_volts(-20.0f, 30.0);

    const float plate = settled(crossed, 30.0f);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(plate, expected, plate_bound(-20.0f, 30.0, expected))));
    EXPECT_GT(static_cast<double>(plate) - through_tanh,
              plate_bound(-20.0f, 30.0, expected) + plate_bound(-20.0f, 30.0, through_tanh));
}

TEST(Small_PentodeStage, PentodeStageResetClearsWarmStartState)
{
    SushiDSP::PentodeStage stage_a(K_REFERENCE_PENTODE);
    SushiDSP::PentodeStage stage_b(K_REFERENCE_PENTODE);

    stage_a.process_sample(100.0f);
    stage_a.reset();

    // Both stages run the same operations from the same state, so the outputs are the same float.
    EXPECT_EQ(stage_a.process_sample(0.5f), stage_b.process_sample(0.5f));
}

TEST(Small_PentodeStage, PentodeStagesOfEqualBiasGiveEqualOutputs)
{
    // Two halves of one pair share no state: equal bias and equal input give the same float.
    SushiDSP::PentodeStage a = stage_with_bias(-38.0f);
    SushiDSP::PentodeStage b = stage_with_bias(-38.0f);

    EXPECT_EQ(settled(a, 30.0f, 64), settled(b, 30.0f, 64));
}

TEST(Small_PentodeStage, PentodePairBiasDifferenceMovesTheHandover)
{
    // The composite is odd while both grids stay in window. See tests/foundation/circuits/stages/test_pentode_stage/README.md.
    const double drive = 60.0;
    const double expected =
        0.5 * ((reference_plate_volts(-38.0f, drive) - reference_plate_volts(-25.0f, -drive)) +
               (reference_plate_volts(-38.0f, -drive) - reference_plate_volts(-25.0f, drive)));
    const double bound =
        2.0 * (plate_bound(-38.0f, drive, reference_plate_volts(-38.0f, drive)) +
               plate_bound(-25.0f, drive, reference_plate_volts(-25.0f, drive)));

    const float even_part = even_part_of_pair(-38.0f, -25.0f, static_cast<float>(drive));

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(even_part, expected, bound)));
    EXPECT_GT(std::fabs(expected), 16.0 * bound);
}

TEST(Small_PentodeStage, PentodeStagePresetConstructorMatchesTheSixArgumentOne)
{
    SushiDSP::PentodeStage from_preset(K_REFERENCE_PENTODE);
    SushiDSP::PentodeStage from_fields = stage_with_bias(K_REFERENCE_PENTODE.cathode_bias_volts);
    const std::vector<float> inputs = loud_sine();

    EXPECT_EQ(run(from_preset, inputs), run(from_fields, inputs));
}

TEST(Small_PentodeStage, PentodeStageHandsOverTheNewtonSolvesItRan)
{
    SushiDSP::PentodeStage stage(K_REFERENCE_PENTODE);
    drive_ramp_through_conduction(stage);
    SushiDSP::NewtonRaphsonSolver::Tally tally;

    stage.take_newton_tally(tally);

    EXPECT_GT(tally.solves, 0U);
}

TEST(Small_PentodeStage, PentodeStageClearsItsNewtonSolvesOnceItHandsThemOver)
{
    SushiDSP::PentodeStage stage(K_REFERENCE_PENTODE);
    drive_ramp_through_conduction(stage);
    SushiDSP::NewtonRaphsonSolver::Tally first;
    stage.take_newton_tally(first);
    SushiDSP::NewtonRaphsonSolver::Tally second;

    stage.take_newton_tally(second);

    EXPECT_EQ(second.solves, 0U);
}
