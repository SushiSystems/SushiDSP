/****************************************************************/
/* test_triode_stage.cpp                                        */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_triode_stage.cpp
 * @brief Tests TriodeStage and the AntiderivativeAntialiaser it runs through.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/stages/AntiderivativeAntialiaser.hpp"
#include "SushiDSP/circuits/stages/TriodeStage.hpp"

namespace
{
    // These constants are the JCM800's V1a-shaped stage. See tests/functional/unit/README.md.
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

    /// @brief Builds the antialiaser of the reference stage.
    SushiDSP::AntiderivativeAntialiaser make_antialiaser()
    {
        return SushiDSP::AntiderivativeAntialiaser(kSourceResistance, kGridLeakResistance, kCathodeBias, kGain,
                                                   SushiDSP::kTriode12AX7GridConduction, kSwing);
    }

    /// @brief Solves the reference stage's grid node by bisection, signed: Vg - Vk at the root.
    double bisected_signed_excursion(double input_voltage)
    {
        return bisected_grid_excursion(input_voltage) * (input_voltage < 0.0 ? -1.0 : 1.0);
    }

    /// @brief The reference stage's plate output at one grid excursion, bound(-gain * u).
    double plate_at(double excursion)
    {
        return kSwing.bound(-static_cast<double>(kGain) * excursion);
    }

    /// @brief The composite transfer at one input, the bisected grid solve through the plate bound.
    double composite_transfer(double input_voltage)
    {
        return plate_at(bisected_signed_excursion(input_voltage));
    }

    /// @brief Integrates a function by Simpson's rule on the given number of even panels.
    template <typename Function>
    double simpson(Function&& function, double low, double high, int panels)
    {
        const double step = (high - low) / panels;
        double sum = function(low) + function(high);
        for (int i = 1; i < panels; ++i)
        {
            sum += (i % 2 == 1 ? 4.0 : 2.0) * function(low + i * step);
        }
        return sum * step / 3.0;
    }

    /// @brief The excursion at the centre of the reference stage's grid knee.
    constexpr double kKneeExcursion = SushiDSP::Triode12AX7::kGridThresholdVolts - kCathodeBias;
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
    // 3 V in is 2.9703 V of grid excursion. See tests/functional/unit/README.md.
    SushiDSP::TriodeStage stage = make_stage();
    settled_plate(stage, -3.0f);
    EXPECT_NEAR(stage.peak_grid_excursion_volts(), 3.0f * kGridLeakResistance / (kSourceResistance + kGridLeakResistance), 1e-3f);
}

TEST(Unit_Dsp, TriodeStageGridConductsOnTheNormanLaw)
{
    // The root is checked against an independent bisection. See tests/functional/unit/README.md.
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

    // The two sides differ by the swing alone. See tests/functional/unit/README.md.
    EXPECT_NEAR(positive_peak, 193.15f, 1.0f);
    EXPECT_LT(positive_peak, kSwing.headroom_to_supply_volts);
    EXPECT_NEAR(negative_peak, -114.99f, 0.5f);
    EXPECT_GT(negative_peak, -kSwing.headroom_to_bottom_volts);
}

TEST(Unit_Dsp, TriodeStageCutoffIsThePlateCeiling)
{
    // No solver fence holds the grid short of the ceiling. See tests/functional/unit/README.md.
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

TEST(Unit_Dsp, AntiderivativeAntialiaserInputMapInvertsTheGridSolve)
{
    // g(u) composed with the bisected solve is the identity. See tests/functional/unit/README.md.
    const SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
    for (double input_voltage : { -30.0, -3.0, -0.1, 0.1, 1.5, 2.0, 2.5, 3.0, 8.0, 30.0 })
    {
        EXPECT_NEAR(antialiaser.input_for_excursion(bisected_signed_excursion(input_voltage)), input_voltage, 1e-6)
            << input_voltage;
    }
}

TEST(Unit_Dsp, AntiderivativeAntialiaserAntiderivativeDifferentiatesBackToTheTransfer)
{
    // dG/du equals bound(-gain u) times dg/du everywhere. See tests/functional/unit/README.md.
    const SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
    const double transition = SushiDSP::Triode12AX7::kGridTransitionVolts;
    constexpr double kDelta = 1e-4;
    for (double excursion = kKneeExcursion - 40.0 * transition; excursion <= kKneeExcursion + 40.0 * transition;
         excursion += transition / 37.0)
    {
        const double numeric_slope
            = (antialiaser.antiderivative(excursion + kDelta) - antialiaser.antiderivative(excursion - kDelta)) / (2.0 * kDelta);
        const double input_slope
            = (antialiaser.input_for_excursion(excursion + kDelta) - antialiaser.input_for_excursion(excursion - kDelta)) / (2.0 * kDelta);
        EXPECT_NEAR(numeric_slope, plate_at(excursion) * input_slope, 1e-7 * kSwing.headroom_to_supply_volts * input_slope)
            << "excursion " << excursion;
    }
}

TEST(Unit_Dsp, AntiderivativeAntialiaserTransferMatchesThePlateBoundBetweenKnots)
{
    // Between knots only the Hermite slope error remains. See tests/functional/unit/README.md.
    const SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
    const double transition = SushiDSP::Triode12AX7::kGridTransitionVolts;
    const double half_step = transition / SushiDSP::AntiderivativeAntialiaser::kIntervalsPerTransition / 2.0;
    for (double excursion = kKneeExcursion - 24.0 * transition + half_step; excursion < kKneeExcursion + 24.0 * transition;
         excursion += transition / 32.0)
    {
        EXPECT_NEAR(antialiaser.transfer(excursion), plate_at(excursion), 1e-7 * kSwing.headroom_to_supply_volts)
            << "excursion " << excursion;
    }
}

TEST(Unit_Dsp, AntiderivativeAntialiaserDividedDifferenceIsTheMeanOfTheCompositeTransfer)
{
    // The divided difference of G is the mean transfer. See tests/functional/unit/README.md.
    const SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
    struct Interval
    {
        double low;
        double high;
    };
    for (const Interval& interval : { Interval{ -1.0, 1.0 }, Interval{ 0.0, 3.0 * kKneeExcursion },
                                      Interval{ -kKneeExcursion, 2.0 * kKneeExcursion }, Interval{ -30.0, 30.0 },
                                      Interval{ 2.0 * kKneeExcursion, 2.0 * kKneeExcursion + 0.01 } })
    {
        const double low_excursion = bisected_signed_excursion(interval.low);
        const double high_excursion = bisected_signed_excursion(interval.high);
        const double divided_difference
            = (antialiaser.antiderivative(high_excursion) - antialiaser.antiderivative(low_excursion))
              / (antialiaser.input_for_excursion(high_excursion) - antialiaser.input_for_excursion(low_excursion));
        const double mean = simpson(composite_transfer, interval.low, interval.high, 20000) / (interval.high - interval.low);
        EXPECT_NEAR(divided_difference, mean, 1e-6 * kSwing.headroom_to_supply_volts)
            << interval.low << " to " << interval.high;
    }
}

TEST(Unit_Dsp, AntiderivativeAntialiaserIsContinuousWhereConsecutiveSamplesNearlyCoincide)
{
    // Near-equal samples fall back to the midpoint transfer. See tests/functional/unit/README.md.
    SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
    const float excursion = static_cast<float>(bisected_signed_excursion(1.2 * kKneeExcursion));
    const float plate = static_cast<float>(plate_at(excursion));
    const float tolerance = 2.0f * std::abs(plate) * 1.2e-7f;

    antialiaser.process(excursion);
    EXPECT_NEAR(antialiaser.process(excursion), plate, tolerance);

    const float next = std::nextafter(excursion, 1e9f);
    antialiaser.reset();
    antialiaser.process(excursion);
    EXPECT_NEAR(antialiaser.process(next), static_cast<float>(plate_at(next)), tolerance);

    double previous = 0.0;
    bool first = true;
    double largest_step = 0.0;
    for (double difference = 0.2e-4; difference <= 5.0e-4; difference += 0.02e-4)
    {
        const double low = bisected_signed_excursion(1.2 * kKneeExcursion);
        const double high = bisected_signed_excursion(1.2 * kKneeExcursion + difference);
        antialiaser.reset();
        antialiaser.process(static_cast<float>(low));
        const double output = antialiaser.process(static_cast<float>(high));
        if (!first)
        {
            largest_step = std::max(largest_step, std::abs(output - previous));
        }
        previous = output;
        first = false;
    }
    EXPECT_LT(largest_step, 4.0 * tolerance);
}

TEST(Unit_Dsp, AntiderivativeAntialiaserResetDropsItsPreviousSample)
{
    // The first sample after reset() is the plain transfer. See tests/functional/unit/README.md.
    SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
    const float first = static_cast<float>(bisected_signed_excursion(0.5 * kKneeExcursion));
    const float second = static_cast<float>(bisected_signed_excursion(1.5 * kKneeExcursion));

    EXPECT_NEAR(antialiaser.process(first), static_cast<float>(plate_at(first)), 1e-4f);
    const float averaged = antialiaser.process(second);
    EXPECT_GT(std::abs(averaged - static_cast<float>(plate_at(second))), 1e-2f);

    antialiaser.reset();
    EXPECT_NEAR(antialiaser.process(second), static_cast<float>(plate_at(second)), 1e-4f);
}

TEST(Unit_Dsp, TriodeStageOutputIsTheAntialiaserOfItsOwnGridSolve)
{
    // The stage's output is its antialiaser's output. See tests/functional/unit/README.md.
    SushiDSP::TriodeStage stage = make_stage();
    SushiDSP::AntiderivativeAntialiaser antialiaser = make_antialiaser();
    for (int i = 0; i < 4000; ++i)
    {
        const float input = 6.0f * static_cast<float>(std::sin(2.0 * kPi * 1000.0 * i / 384000.0));
        const float stage_output = stage.process_sample(input);
        const float excursion = static_cast<float>(bisected_signed_excursion(input));
        EXPECT_NEAR(stage_output, antialiaser.process(excursion), 0.05f) << i;
    }
}
