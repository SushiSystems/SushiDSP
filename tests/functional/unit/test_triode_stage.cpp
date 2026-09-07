/**************************************************************************/
/* test_triode_stage.cpp                                                  */
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

#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/stages/TriodeStage.hpp"

namespace
{
    // The JCM800's V1a-shaped stage: a 10 kOhm source into a 1 MOhm grid leak, biased 1.5 V
    // below the cathode, gain 60, with a 194 / 115 V plate swing.
    constexpr float kSourceResistance = 10000.0f;
    constexpr float kGridLeakResistance = 1000000.0f;
    constexpr float kCathodeBias = -1.5f;
    constexpr float kGain = 60.0f;
    constexpr SushiDSP::PlateSwing kSwing{ 194.0f, 115.0f };
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Builds the reference stage every case below drives.
    SushiDSP::TriodeStage make_stage()
    {
        return SushiDSP::TriodeStage(kSourceResistance, kGridLeakResistance, kCathodeBias, kGain,
                                     SushiDSP::kTriode12AX7GridConduction, kSwing);
    }

    /// @brief Solves the stage's own grid node by bisection, independently of the Newton solver.
    /// @param input_voltage The DC input, referenced to the bias point.
    /// @return The grid's excursion |Vg - Vk| at the root of the KCL the header states.
    double bisected_grid_excursion(double input_voltage)
    {
        const auto& law = SushiDSP::kTriode12AX7GridConduction;
        auto residual = [&](double vg)
        {
            return (vg - (kCathodeBias + input_voltage)) / kSourceResistance
                 + (vg - kCathodeBias) / kGridLeakResistance
                 + law.scale_amps * std::log1p(std::exp((vg - law.threshold_volts) / law.transition_volts));
        };
        double low = -50.0;
        double high = 50.0;
        for (int step = 0; step < 200; ++step)
        {
            const double middle = 0.5 * (low + high);
            (residual(middle) > 0.0 ? high : low) = middle;
        }
        return std::fabs(0.5 * (low + high) - kCathodeBias);
    }

    /// @brief Holds a DC input on a fresh stage until the warm start has settled.
    /// @return The plate output at that input.
    float settled_plate(SushiDSP::TriodeStage& stage, float input_voltage)
    {
        float output = 0.0f;
        for (int i = 0; i < 64; ++i)
        {
            output = stage.process_sample(input_voltage);
        }
        return output;
    }
}

TEST(Unit_Dsp, TriodeStageIsDcStableForConstantInput)
{
    SushiDSP::TriodeStage stage = make_stage();

    float output = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        output = stage.process_sample(0.1f);
    }

    EXPECT_TRUE(std::isfinite(output));
}

TEST(Unit_Dsp, TriodeStageIsInvertingForSmallSignal)
{
    SushiDSP::TriodeStage stage = make_stage();
    EXPECT_LT(settled_plate(stage, 0.05f), 0.0f);
}

TEST(Unit_Dsp, TriodeStageGridFollowsTheSourceDividerBelowTheCathode)
{
    // Below the cathode the Norman law's current is nanoamperes, so the grid sits on the
    // Rg / (Rs + Rg) divider of the input: 3 V in is 2.9703 V of excursion.
    SushiDSP::TriodeStage stage = make_stage();
    settled_plate(stage, -3.0f);
    EXPECT_NEAR(stage.peak_grid_excursion_volts(), 3.0f * kGridLeakResistance / (kSourceResistance + kGridLeakResistance), 1e-3f);
}

TEST(Unit_Dsp, TriodeStageGridConductsOnTheNormanLaw)
{
    // The solver's root is the root of the KCL the header states, checked against an independent
    // bisection at three inputs: below the 0.5 V knee, on it, and well past it, where the
    // conducting slope of 150 kOhm against the 10 kOhm source pulls the grid 0.45 V off the
    // divider (8 V in: 7.554 V of excursion against 7.921 V linear).
    for (float input_voltage : { 0.1f, 1.5f, 3.0f, 8.0f })
    {
        SushiDSP::TriodeStage stage = make_stage();
        settled_plate(stage, input_voltage);
        EXPECT_NEAR(stage.peak_grid_excursion_volts(), bisected_grid_excursion(input_voltage), 2e-3f)
            << "input " << input_voltage;
    }
}

TEST(Unit_Dsp, TriodeStageResetReturnsToCathodeBiasWarmStart)
{
    SushiDSP::TriodeStage stage_a = make_stage();
    SushiDSP::TriodeStage stage_b = make_stage();

    stage_a.process_sample(1.0f);
    stage_a.reset();

    EXPECT_NEAR(stage_a.process_sample(0.01f), stage_b.process_sample(0.01f), 1e-4f);
}

TEST(Unit_Dsp, TriodeStagePlateSwingBoundsBothSidesAsymmetrically)
{
    SushiDSP::TriodeStage stage = make_stage();
    float positive_peak = 0.0f;
    float negative_peak = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float y = stage.process_sample(10.0f * static_cast<float>(std::sin(2.0 * kPi * 100.0 * i / 48000.0)));
        positive_peak = std::max(positive_peak, y);
        negative_peak = std::min(negative_peak, y);
    }

    // Cutoff side: 9.9 V of excursion is a linear plate of 594 V, and 194 * tanh(594 / 194) is
    // 193.15 V. Conduction side: the grid rises to 7.55 V of excursion, a linear plate of
    // -453 V, and -115 * tanh(453 / 115) is -114.99 V. The two sides differ by the swing alone.
    EXPECT_NEAR(positive_peak, 193.15f, 1.0f);
    EXPECT_LT(positive_peak, kSwing.headroom_to_supply_volts);
    EXPECT_NEAR(negative_peak, -114.99f, 0.5f);
    EXPECT_GT(negative_peak, -kSwing.headroom_to_bottom_volts);
}

TEST(Unit_Dsp, TriodeStageCutoffIsThePlateCeiling)
{
    // No solver fence holds the grid short of the ceiling: at -30 V in, the 5150's V5A stage
    // reaches its 90.742 V headroom to the supply, where a 4.136 V fence held it at 74.3 V
    // (docs/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md section 3).
    SushiDSP::TriodeStage v5a(671730.0f, 1000000.0f, -2.0f, 25.3f, SushiDSP::kTriode12AX7GridConduction,
                              SushiDSP::PlateSwing{ 90.742f, 134.001f });
    EXPECT_GT(settled_plate(v5a, -30.0f), 90.7f);
    EXPECT_LE(settled_plate(v5a, -30.0f), 90.742f);
}

TEST(Unit_Dsp, TriodeStageSmallSignalGainIsUnchangedByThePlateSwing)
{
    SushiDSP::TriodeStage stage = make_stage();
    float peak = 0.0f;
    for (int i = 0; i < 4800; ++i)
    {
        peak = std::max(peak, std::fabs(stage.process_sample(0.01f * static_cast<float>(std::sin(2.0 * kPi * 1000.0 * i / 48000.0)))));
    }

    // 0.01 V through a gain of 60 is 0.6 V, less the 1 % the Rs/Rg source divider takes.
    EXPECT_NEAR(peak, 0.6f, 0.012f);
}

TEST(Unit_Dsp, TriodeStageReportsThePeakGridExcursionSinceReset)
{
    SushiDSP::TriodeStage stage = make_stage();
    EXPECT_FLOAT_EQ(stage.peak_grid_excursion_volts(), 0.0f);
    for (int i = 0; i < 480; ++i)
    {
        stage.process_sample(-2.0f);
    }
    EXPECT_NEAR(stage.peak_grid_excursion_volts(), 1.98f, 0.01f);
    stage.reset();
    EXPECT_FLOAT_EQ(stage.peak_grid_excursion_volts(), 0.0f);
}

TEST(Unit_Dsp, TriodeStageGridLawIsTheTriode12AX7Model)
{
    // The stage's constants are read from Triode12AX7, so the two cannot drift apart.
    EXPECT_FLOAT_EQ(SushiDSP::kTriode12AX7GridConduction.scale_amps, static_cast<float>(SushiDSP::Triode12AX7::kGridCurrentScale));
    EXPECT_FLOAT_EQ(SushiDSP::kTriode12AX7GridConduction.threshold_volts, static_cast<float>(SushiDSP::Triode12AX7::kGridThresholdVolts));
    EXPECT_FLOAT_EQ(SushiDSP::kTriode12AX7GridConduction.transition_volts, static_cast<float>(SushiDSP::Triode12AX7::kGridTransitionVolts));
}
