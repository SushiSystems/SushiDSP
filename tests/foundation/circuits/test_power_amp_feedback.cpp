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
 * @file test_power_amp_feedback.cpp
 * @brief Nulls the feedback leg's DK solve against the same network solved by hand.
 * @author Mustafa Garip & Sushi Systems
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/power_amp_feedback.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>

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

    /// @brief Solves a four-unknown complex linear system by Gaussian elimination with partial pivoting.
    /// @param matrix The coefficients, consumed.
    /// @param right_hand_side The right-hand side, consumed.
    /// @return The solution.
    std::array<std::complex<double>, 4> solve_nodal(std::array<std::array<std::complex<double>, 4>, 4> matrix,
                                                    std::array<std::complex<double>, 4> right_hand_side)
    {
        for (std::size_t column = 0; column < 4; ++column)
        {
            std::size_t pivot = column;
            for (std::size_t row = column + 1; row < 4; ++row)
            {
                if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column]))
                {
                    pivot = row;
                }
            }
            std::swap(matrix[column], matrix[pivot]);
            std::swap(right_hand_side[column], right_hand_side[pivot]);
            for (std::size_t row = column + 1; row < 4; ++row)
            {
                const std::complex<double> factor = matrix[row][column] / matrix[column][column];
                for (std::size_t inner = column; inner < 4; ++inner)
                {
                    matrix[row][inner] -= factor * matrix[column][inner];
                }
                right_hand_side[row] -= factor * right_hand_side[column];
            }
        }
        std::array<std::complex<double>, 4> solution{};
        for (std::size_t row = 4; row-- > 0;)
        {
            std::complex<double> accumulator = right_hand_side[row];
            for (std::size_t inner = row + 1; inner < 4; ++inner)
            {
                accumulator -= matrix[row][inner] * solution[inner];
            }
            solution[row] = accumulator / matrix[row][row];
        }
        return solution;
    }

    /// @brief The same network's gain from its continuous-time nodal equations, for the null to aim at.
    ///
    /// The grid leak returns to the tail node, not to ground, so the four nodes (split, divider, grid, tail)
    /// are solved together. The rheostat is floored at the DK block's minimum section.
    /// @param frequency_hz The drive frequency.
    /// @param presence Presence position in [0, 1].
    /// @return The grid's gain in decibels.
    double analytic_gain_db(double frequency_hz, double presence)
    {
        constexpr std::size_t K_SPLIT = 0, K_DIVIDER = 1, K_GRID = 2, K_TAIL = 3;
        const SushiDSP::PowerAmpFeedbackComponents& c = SushiDSP::K_MARK2C_FEEDBACK;
        const std::complex<double> s(0.0, 2.0 * K_PI * frequency_hz);
        const std::complex<double> shaping_admittance =
            1.0 / static_cast<double>(c.shaping_resistor_ohms) + s * static_cast<double>(c.shaping_capacitor_farads);
        const double rheostat_ohms = std::max(SushiDSP::dk::K_MINIMUM_POT_OHMS,
                                              static_cast<double>(static_cast<float>(presence * c.presence_potentiometer_ohms)));
        const std::complex<double> source_admittance =
            1.0 / (rheostat_ohms + static_cast<double>(c.tap_series_resistor_ohms) + 1.0 / shaping_admittance);

        std::array<std::array<std::complex<double>, 4>, 4> admittance{};
        std::array<std::complex<double>, 4> drive{};
        const auto to_ground = [&admittance](std::size_t node, std::complex<double> value)
        {
            admittance[node][node] += value;
        };
        const auto between = [&admittance](std::size_t first, std::size_t second, std::complex<double> value)
        {
            admittance[first][first] += value;
            admittance[second][second] += value;
            admittance[first][second] -= value;
            admittance[second][first] -= value;
        };
        to_ground(K_SPLIT, source_admittance);
        drive[K_SPLIT] = source_admittance;
        between(K_SPLIT, K_TAIL, 1.0 / static_cast<double>(c.tail_feed_resistor_ohms));
        between(K_SPLIT, K_DIVIDER, 1.0 / static_cast<double>(c.divider_series_resistor_ohms));
        to_ground(K_DIVIDER, 1.0 / static_cast<double>(c.divider_shunt_resistor_ohms) +
                                 s * static_cast<double>(c.divider_shunt_capacitor_farads));
        between(K_DIVIDER, K_GRID, s * static_cast<double>(c.coupling_capacitor_farads));
        between(K_GRID, K_TAIL, 1.0 / static_cast<double>(c.grid_leak_resistor_ohms));
        to_ground(K_TAIL, 1.0 / static_cast<double>(K_TAIL_IMPEDANCE_OHMS));

        return 20.0 * std::log10(std::abs(solve_nodal(admittance, drive)[K_GRID]));
    }

    const double kU = TestSupport::unit_roundoff<float>();

    /// @brief The gain error in decibels of the measurement: the input and the output are floats, so the projection carries 8 u of relative error, and 20 / ln 10 turns it into decibels. The one-second settle leaves e^(-1 s / tau) of the start, with tau at most the sum of the resistors times the sum of the capacitors, 73 ms, which is 1e-6 and so adds 1e-5 dB.
    const double kMeasurementDb = 8.686 * 8.0 * kU + 1e-5;

    /// @brief The ladder's slope in decibels per neper of frequency, by a centred difference over a thousandth of a neper.
    double slope_db_per_neper(double frequency_hz, double presence)
    {
        constexpr double K_STEP = 1e-3;
        return (analytic_gain_db(frequency_hz * std::exp(K_STEP), presence) -
                analytic_gain_db(frequency_hz * std::exp(-K_STEP), presence)) /
               (2.0 * K_STEP);
    }

    // The bilinear transform maps the analog frequency f to tan(pi f / fs) fs / pi, which is f (1 + x^2 / 3 + ...) at x = pi f / fs, so the digital response at f is the analog response shifted by ln(1 + x^2 / 3) nepers; the factor 1.5 covers the next terms for x below 0.33.
    double warp_bound_db(double slope, double frequency_hz)
    {
        const double x = K_PI * frequency_hz / K_RATE;
        return std::fabs(slope) * x * x / 3.0 * 1.5;
    }

    /// @brief Holds one presence position and one drive frequency.
    struct LadderRow
    {
            const char* name;
            double presence;
            double frequency_hz;
    };

    const LadderRow kLadderRows[] = {
        {"PresenceZeroAt80Hz", 0.0, 80.0},       {"PresenceZeroAt600Hz", 0.0, 600.0},
        {"PresenceZeroAt3kHz", 0.0, 3000.0},     {"PresenceZeroAt10kHz", 0.0, 10000.0},
        {"PresenceQuarterAt80Hz", 0.25, 80.0},   {"PresenceQuarterAt600Hz", 0.25, 600.0},
        {"PresenceQuarterAt3kHz", 0.25, 3000.0}, {"PresenceQuarterAt10kHz", 0.25, 10000.0},
        {"PresenceHalfAt80Hz", 0.5, 80.0},       {"PresenceHalfAt600Hz", 0.5, 600.0},
        {"PresenceHalfAt3kHz", 0.5, 3000.0},     {"PresenceHalfAt10kHz", 0.5, 10000.0},
        {"PresenceFullAt80Hz", 1.0, 80.0},       {"PresenceFullAt600Hz", 1.0, 600.0},
        {"PresenceFullAt3kHz", 1.0, 3000.0},     {"PresenceFullAt10kHz", 1.0, 10000.0},
    };

    /// @brief Builds a leg with the Mark IIC+ values and the inverter's tail impedance.
    SushiDSP::PowerAmpFeedback make_leg()
    {
        return SushiDSP::PowerAmpFeedback(K_RATE, SushiDSP::K_MARK2C_FEEDBACK,
                                          SushiDSP::PowerAmpFeedbackTermination{K_TAIL_IMPEDANCE_OHMS});
    }

    /// @brief Builds a leg at 0 Hz, which the derivation refuses, and drops the exception.
    void construct_at_zero_hertz_and_swallow_the_refusal()
    {
        try
        {
            SushiDSP::PowerAmpFeedback leg(0.0, SushiDSP::K_MARK2C_FEEDBACK);
        }
        catch (const std::runtime_error&)
        {
        }
    }
} // namespace


class Small_PowerAmpFeedbackLadder : public ::testing::TestWithParam<LadderRow>
{
};

TEST_P(Small_PowerAmpFeedbackLadder, PowerAmpFeedbackMatchesTheLadderSolvedByHand)
{
    const LadderRow& row = GetParam();
    SushiDSP::PowerAmpFeedback leg = make_leg();
    leg.set_presence(static_cast<float>(row.presence));
    const double bound = warp_bound_db(slope_db_per_neper(row.frequency_hz, row.presence), row.frequency_hz) + kMeasurementDb;

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        steady_gain_db(leg, row.frequency_hz), analytic_gain_db(row.frequency_hz, row.presence), bound)));
}

INSTANTIATE_TEST_SUITE_P(Settings, Small_PowerAmpFeedbackLadder, ::testing::ValuesIn(kLadderRows),
                         [](const ::testing::TestParamInfo<LadderRow>& info) { return std::string(info.param.name); });

TEST(Small_PowerAmpFeedback, PowerAmpFeedbackThrowsWhenTheDerivationFails)
{
    EXPECT_THROW(SushiDSP::PowerAmpFeedback(0.0, SushiDSP::K_MARK2C_FEEDBACK), std::runtime_error);
}

TEST(Small_PowerAmpFeedback, PowerAmpFeedbackLogsOneErrorWhenTheDerivationFails)
{
    SushiDSP::LogCapture capture;

    construct_at_zero_hertz_and_swallow_the_refusal();

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "circuits");
    EXPECT_NE(lines[0].message.find("PowerAmpFeedback"), std::string::npos) << lines[0].message;
}
