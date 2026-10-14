/**************************************************************************/
/* test_power_amp_feedback.cpp                                            */
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
 * @file test_power_amp_feedback.cpp
 * @brief Nulls the feedback leg's DK solve against the same network solved by hand.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>
#include <complex>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/power_amp_feedback.hpp>

namespace
{
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief The rate the leg runs at inside the Mark IIC+, 48 kHz eight times oversampled.
    constexpr double K_RATE = 384000.0;

    /// @brief What the Mark IIC+'s inverter presents at the tail node.
    constexpr float K_TAIL_IMPEDANCE_OHMS = 1068.5f;

    /// @brief Measures the leg's gain by projecting its output onto the drive.
    /// @param leg The leg, left with its state at the end of the run.
    /// @param frequency_hz The drive frequency.
    /// @return The gain in decibels.
    double steady_gain_db(SushiDSP::PowerAmpFeedback& leg, double frequency_hz)
    {
        // A whole number of cycles keeps the single-bin projection free of leakage.
        const long long settle = static_cast<long long>(K_RATE);
        const double cycles = std::ceil(0.25 * frequency_hz);
        const long long measure =
            static_cast<long long>(std::llround(cycles * K_RATE / frequency_hz));

        double real_part = 0.0, imaginary_part = 0.0;
        for (long long i = 0; i < settle + measure; ++i)
        {
            const double phase = 2.0 * K_PI * frequency_hz * static_cast<double>(i) / K_RATE;
            const float y = leg.process_sample(static_cast<float>(std::sin(phase)));
            if (i >= settle)
            {
                real_part += y * std::sin(phase);
                imaginary_part += y * std::cos(phase);
            }
        }

        const double magnitude =
            2.0 * std::sqrt(real_part * real_part + imaginary_part * imaginary_part) /
            static_cast<double>(measure);
        return 20.0 * std::log10(magnitude);
    }

    /// @brief The same network's gain from the continuous-time ladder, for the null to aim at.
    ///
    /// The leg is a chain of two-terminal impedances, so its transfer is three divisions: the tap
    /// into the split node, the split node into the divider, and the divider through the coupling
    /// capacitor into the grid leak.
    /// @param frequency_hz The drive frequency.
    /// @param presence Presence position in [0, 1].
    /// @return The gain in decibels.
    double analytic_gain_db(double frequency_hz, double presence)
    {
        const SushiDSP::PowerAmpFeedbackComponents& c = SushiDSP::K_MARK2C_FEEDBACK;
        const std::complex<double> s(0.0, 2.0 * K_PI * frequency_hz);
        const auto capacitor = [s](double farads)
        {
            return 1.0 / (s * farads);
        };
        const auto ohms = [](float value)
        {
            return std::complex<double>(value, 0.0);
        };
        const auto parallel = [](std::complex<double> a, std::complex<double> b)
        {
            return 1.0 / (1.0 / a + 1.0 / b);
        };

        const std::complex<double> shaping =
            parallel(ohms(c.shaping_resistor_ohms), capacitor(c.shaping_capacitor_farads));
        const std::complex<double> ground_leg = parallel(
            ohms(c.divider_shunt_resistor_ohms), capacitor(c.divider_shunt_capacitor_farads));
        const std::complex<double> grid_branch =
            capacitor(c.coupling_capacitor_farads) + ohms(c.grid_leak_resistor_ohms);
        const std::complex<double> below_divider = parallel(ground_leg, grid_branch);
        const std::complex<double> divider_branch =
            ohms(c.divider_series_resistor_ohms) + below_divider;
        const std::complex<double> tail_branch(c.tail_feed_resistor_ohms + K_TAIL_IMPEDANCE_OHMS,
                                               0.0);
        const std::complex<double> split = parallel(tail_branch, divider_branch);

        const std::complex<double> to_split =
            split / (ohms(static_cast<float>(presence * c.presence_potentiometer_ohms)) +
                     ohms(c.tap_series_resistor_ohms) + shaping + split);
        const std::complex<double> to_divider = to_split * below_divider / divider_branch;
        const std::complex<double> to_grid =
            to_divider * ohms(c.grid_leak_resistor_ohms) / grid_branch;
        return 20.0 * std::log10(std::abs(to_grid));
    }
} // namespace

TEST(Unit_Dsp, PowerAmpFeedbackMatchesTheLadderSolvedByHand)
{
    for (const double presence : {0.0, 0.25, 0.5, 1.0})
    {
        for (const double frequency : {80.0, 600.0, 3000.0, 10000.0})
        {
            SushiDSP::PowerAmpFeedback leg(
                K_RATE, SushiDSP::K_MARK2C_FEEDBACK,
                SushiDSP::PowerAmpFeedbackTermination{K_TAIL_IMPEDANCE_OHMS});
            leg.set_presence(static_cast<float>(presence));
            EXPECT_NEAR(steady_gain_db(leg, frequency), analytic_gain_db(frequency, presence), 0.05)
                << "presence " << presence << " at " << frequency << " Hz";
        }
    }
}

TEST(Unit_Dsp, PowerAmpFeedbackOpeningPresenceRemovesMoreTrebleThanBass)
{
    SushiDSP::PowerAmpFeedback shut(K_RATE, SushiDSP::K_MARK2C_FEEDBACK,
                                    SushiDSP::PowerAmpFeedbackTermination{K_TAIL_IMPEDANCE_OHMS});
    SushiDSP::PowerAmpFeedback open(K_RATE, SushiDSP::K_MARK2C_FEEDBACK,
                                    SushiDSP::PowerAmpFeedbackTermination{K_TAIL_IMPEDANCE_OHMS});
    shut.set_presence(0.0f);
    open.set_presence(1.0f);

    const double bass_travel = steady_gain_db(shut, 80.0) - steady_gain_db(open, 80.0);
    SushiDSP::PowerAmpFeedback shut_hf(
        K_RATE, SushiDSP::K_MARK2C_FEEDBACK,
        SushiDSP::PowerAmpFeedbackTermination{K_TAIL_IMPEDANCE_OHMS});
    SushiDSP::PowerAmpFeedback open_hf(
        K_RATE, SushiDSP::K_MARK2C_FEEDBACK,
        SushiDSP::PowerAmpFeedbackTermination{K_TAIL_IMPEDANCE_OHMS});
    shut_hf.set_presence(0.0f);
    open_hf.set_presence(1.0f);
    const double treble_travel =
        steady_gain_db(shut_hf, 10000.0) - steady_gain_db(open_hf, 10000.0);

    // The traced network's own figures, from docs/agent/2026_09_20_V0_1_0_RELEASE/REPORT.md.
    EXPECT_NEAR(bass_travel, 14.15, 0.2);
    EXPECT_NEAR(treble_travel, 34.38, 0.2);
    EXPECT_GT(treble_travel, bass_travel + 15.0);
}
