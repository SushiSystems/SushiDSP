/**************************************************************************/
/* test_fmv_tone_stack.cpp                                                */
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
 * @file test_fmv_tone_stack.cpp
 * @brief Nulls FmvToneStack against the jcm_fmv AC sweep and checks its knob extremes.
 * @author Mustafa Garip & Sushi Systems
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/circuits/fmv_tone_stack.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/foundation/spice_reference/spice_csv.hpp>

namespace
{
    constexpr double K_PI = 3.14159265358979323846;

    const double kU = TestSupport::unit_roundoff<float>();
    const double kUDouble = TestSupport::unit_roundoff<double>();

    /// @brief The gain error in decibels of the measurement itself: the input and the output are floats, so the projection carries 8 u of relative error, and 20 / ln 10 turns a relative error into decibels. The one-second settle leaves e^(-1 s / tau) of the start, with tau at most the sum of the resistors times the sum of the capacitors, 28.7 ms, which is 1e-15 and below this.
    const double kMeasurementDb = 8.686 * 8.0 * kU;

    /// @brief The rounding of the six digits the SPICE sweep prints, in decibels.
    constexpr double K_SPICE_PRINT_DB = 1e-6;

    /// @brief The characterised distance of the committed LTspice sweep from the exact solution of its own netlist, in decibels; not derived.
    ///
    /// An independent nodal solve of docs/reference/spice/jcm_fmv/jcm_fmv.asc, in double, differs from the sweep by up to 0.0142 dB
    /// (middle at one tenth; 0.0046 and 0.0027 at the other two) and agrees with the stack to its own warp and measurement bound,
    /// so the distance belongs to the sweep's solver, not to the stack. It does not follow the printed frequency by a shift of the
    /// axis. The value is that largest distance, 0.0142 dB, rounded up to 0.015 dB.
    constexpr double K_SPICE_SOLVER_DB = 0.015;

    /// @brief The tone stack's level at 1 kHz for the cost of a source and a load is above this, in decibels; the dossier section 4.1 gives it for both decks.
    constexpr double K_MINIMUM_LEVEL_COST_DB = 1.0;

    /// @brief The quiet floor at every knob at zero, in decibels, as the DA spec states it.
    constexpr double K_QUIET_FLOOR_DB = -60.0;

    /// @brief Measures a section's steady-state gain by projecting its output onto the drive.
    /// @param stack The section, left with its state at the end of the run.
    /// @param sample_rate The rate the section runs at, in hertz.
    /// @param frequency_hz The drive frequency.
    /// @return The gain in decibels.
    double steady_gain_db(SushiDSP::FmvToneStack& stack, double sample_rate, double frequency_hz)
    {
        // A whole number of cycles keeps the single-bin projection free of leakage.
        const long long settle = static_cast<long long>(sample_rate);
        const double cycles = std::ceil(0.25 * frequency_hz);
        const long long measure = static_cast<long long>(std::llround(cycles * sample_rate / frequency_hz));

        double real_part = 0.0, imaginary_part = 0.0;
        for (long long i = 0; i < settle + measure; ++i)
        {
            const double phase = 2.0 * K_PI * frequency_hz * static_cast<double>(i) / sample_rate;
            const float y = stack.process_sample(static_cast<float>(std::sin(phase)));
            if (i >= settle)
            {
                real_part += y * std::sin(phase);
                imaginary_part += y * std::cos(phase);
            }
        }

        const double magnitude =
            2.0 * std::sqrt(real_part * real_part + imaginary_part * imaginary_part) / static_cast<double>(measure);
        return 20.0 * std::log10(magnitude);
    }

    std::string spice_path(const std::string& block, const std::string& kind)
    {
        return std::string(SUSHIDSP_SPICE_DIR) + "/" + block + "/" + block + "_" + kind + ".csv";
    }

    /// @brief Returns the gain of the hand-derived closed form of the tone stack.
    /// @param components The stack's component values.
    /// @param treble Treble position in [0, 1].
    /// @param bass Bass position in [0, 1].
    /// @param middle Middle position in [0, 1].
    /// @param frequency_hz The frequency to evaluate at.
    /// @return The magnitude in decibels.
    /// @see tests/foundation/circuits/README.md
    double closed_form_gain_db(const SushiDSP::FmvComponents& components, double treble, double bass, double middle,
                               double frequency_hz)
    {
        const double c1 = components.treble_capacitor_farads;
        const double c2 = components.bass_capacitor_farads;
        const double c3 = components.middle_capacitor_farads;
        const double r1 = components.treble_potentiometer_ohms;
        const double r2 = components.bass_potentiometer_ohms;
        const double r3 = components.middle_potentiometer_ohms;
        const double r4 = components.slope_resistor_ohms;
        const double t = treble, l = bass, m = middle;

        const double b1 = t * c1 * r1 + l * (c1 * r2 + c2 * r2) + m * (c1 * r3 + c2 * r3 + c3 * r3);
        const double b2 = t * (c1 * c2 * r1 * r4 + c1 * c3 * r1 * r4) +
                          l * (c1 * c2 * r1 * r2 + c1 * c2 * r2 * r4 + c1 * c3 * r2 * r4 +
                               m * (c1 * c3 * r2 * r3 + c2 * c3 * r2 * r3)) +
                          m * (c1 * c2 * r1 * r3 + c1 * c2 * r3 * r4 + c1 * c3 * r1 * r3 + c1 * c3 * r3 * r4);
        const double b3 = l * c1 * c2 * c3 * r2 * (t * r1 * r4 + m * (r1 * r3 + r3 * r4));
        const double a1 = c1 * r1 + c2 * r4 + c3 * r4 + l * (c1 * r2 + c2 * r2) + m * (c1 * r3 + c2 * r3 + c3 * r3);
        const double a2 = c1 * c2 * r1 * r4 + c1 * c3 * r1 * r4 +
                          l * (c1 * c2 * r1 * r2 + c1 * c2 * r2 * r4 + c1 * c3 * r2 * r4 + c2 * c3 * r2 * r4 +
                               m * (c1 * c3 * r2 * r3 + c2 * c3 * r2 * r3)) +
                          m * (c1 * c2 * r1 * r3 + c1 * c2 * r3 * r4 + c1 * c3 * r1 * r3 + c1 * c3 * r3 * r4);
        const double a3 = l * c1 * c2 * c3 * r2 * (r1 * r4 + m * (r1 * r3 + r3 * r4));

        const double w = 2.0 * K_PI * frequency_hz;
        // H(jw) with s = jw: the numerator has no constant term and the denominator's is 1.
        const double num_real = -b2 * w * w;
        const double num_imaginary = b1 * w - b3 * w * w * w;
        const double den_real = 1.0 - a2 * w * w;
        const double den_imaginary = a1 * w - a3 * w * w * w;
        const double den_squared = den_real * den_real + den_imaginary * den_imaginary;
        const double real_part = (num_real * den_real + num_imaginary * den_imaginary) / den_squared;
        const double imaginary_part = (num_imaginary * den_real - num_real * den_imaginary) / den_squared;
        return 20.0 * std::log10(std::sqrt(real_part * real_part + imaginary_part * imaginary_part));
    }

    /// @brief The closed form's slope in decibels per neper of frequency, by a centred difference over a thousandth of a neper.
    double slope_db_per_neper(const SushiDSP::FmvComponents& components, double treble, double bass, double middle,
                              double frequency_hz)
    {
        constexpr double K_STEP = 1e-3;
        return (closed_form_gain_db(components, treble, bass, middle, frequency_hz * std::exp(K_STEP)) -
                closed_form_gain_db(components, treble, bass, middle, frequency_hz * std::exp(-K_STEP))) /
               (2.0 * K_STEP);
    }

    // The bilinear transform maps the analog frequency f to tan(pi f / fs) fs / pi, which is f (1 + x^2 / 3 + ...) at x = pi f / fs, so the digital response at f is the analog response shifted by ln(1 + x^2 / 3) nepers; the factor 1.5 covers the next terms for x below 0.33.
    double warp_bound_db(double slope, double frequency_hz, double sample_rate)
    {
        const double x = K_PI * frequency_hz / sample_rate;
        return std::fabs(slope) * x * x / 3.0 * 1.5;
    }

    /// @brief Holds the three knob positions of one row.
    struct Knobs
    {
            const char* name;
            float treble;
            float bass;
            float middle;
    };

    const Knobs kClosedFormKnobs[] = {
        {"TrebleLowBassLowMiddleLow", 0.1f, 0.2f, 0.1f},   {"TrebleLowBassLowMiddleMid", 0.1f, 0.2f, 0.6f},
        {"TrebleLowBassHighMiddleLow", 0.1f, 0.8f, 0.1f},  {"TrebleLowBassHighMiddleMid", 0.1f, 0.8f, 0.6f},
        {"TrebleMidBassLowMiddleLow", 0.5f, 0.2f, 0.1f},   {"TrebleMidBassLowMiddleMid", 0.5f, 0.2f, 0.6f},
        {"TrebleMidBassHighMiddleLow", 0.5f, 0.8f, 0.1f},  {"TrebleMidBassHighMiddleMid", 0.5f, 0.8f, 0.6f},
        {"TrebleHighBassLowMiddleLow", 0.9f, 0.2f, 0.1f},  {"TrebleHighBassLowMiddleMid", 0.9f, 0.2f, 0.6f},
        {"TrebleHighBassHighMiddleLow", 0.9f, 0.8f, 0.1f}, {"TrebleHighBassHighMiddleMid", 0.9f, 0.8f, 0.6f},
    };

    const Knobs kExtremeKnobs[] = {
        {"AllAtZero", 0.0f, 0.0f, 0.0f}, {"OnlyMiddleAtOne", 0.0f, 0.0f, 1.0f}, {"OnlyBassAtOne", 0.0f, 1.0f, 0.0f},
        {"BassAndMiddleAtOne", 0.0f, 1.0f, 1.0f}, {"OnlyTrebleAtOne", 1.0f, 0.0f, 0.0f},
        {"TrebleAndMiddleAtOne", 1.0f, 0.0f, 1.0f}, {"TrebleAndBassAtOne", 1.0f, 1.0f, 0.0f},
        {"AllAtOne", 1.0f, 1.0f, 1.0f},
    };

    const Knobs kSpiceMiddles[] = {{"MiddleAtOneTenth", 0.5f, 0.5f, 0.1f},
                                   {"MiddleAtHalf", 0.5f, 0.5f, 0.5f},
                                   {"MiddleAtNineTenths", 0.5f, 0.5f, 0.9f}};

    std::string knob_name(const ::testing::TestParamInfo<Knobs>& info)
    {
        return std::string(info.param.name);
    }

    /// @brief Builds a stack with the JCM 2203 values at a row's knob positions.
    SushiDSP::FmvToneStack stack_at(double sample_rate, const Knobs& knobs)
    {
        SushiDSP::FmvToneStack stack(sample_rate, SushiDSP::K_JCM2203_TONE_STACK);
        stack.set_treble(knobs.treble);
        stack.set_bass(knobs.bass);
        stack.set_middle(knobs.middle);
        return stack;
    }

    /// @brief The peak of the stack's response to a 500 Hz sine at unit amplitude, and whether every sample was finite.
    struct SineResponse
    {
            float peak;
            bool all_finite;
    };

    /// @brief Runs one second of a 500 Hz sine through the stack.
    SineResponse respond_to_sine(SushiDSP::FmvToneStack& stack)
    {
        SineResponse response{0.0f, true};
        for (int i = 0; i < 48000; ++i)
        {
            const float y = stack.process_sample(static_cast<float>(std::sin(2.0 * K_PI * 500.0 * i / 48000.0)));
            response.all_finite = response.all_finite && std::isfinite(y);
            response.peak = std::max(response.peak, std::fabs(y));
        }
        return response;
    }

    /// @brief The impulse response's peak and the sample one second after the impulse.
    struct ImpulseTail
    {
            float peak;
            float last;
    };

    /// @brief Runs a unit impulse and one second of silence through the stack.
    ImpulseTail respond_to_impulse(SushiDSP::FmvToneStack& stack)
    {
        ImpulseTail tail{0.0f, 0.0f};
        for (int i = 0; i < 48000; ++i)
        {
            tail.last = stack.process_sample(i == 0 ? 1.0f : 0.0f);
            tail.peak = std::max(tail.peak, std::fabs(tail.last));
        }
        return tail;
    }

    /// @brief Asserts that the stack at the row's knobs matches the closed form at four frequencies.
    void expect_matches_closed_form(const Knobs& knobs)
    {
        constexpr double K_SAMPLE_RATE = 192000.0;
        const double frequencies[4] = {82.0, 440.0, 2000.0, 6000.0};
        SushiDSP::FmvToneStack stack = stack_at(K_SAMPLE_RATE, knobs);
        for (const double f : frequencies)
        {
            stack.reset();
            const double reference = closed_form_gain_db(SushiDSP::K_JCM2203_TONE_STACK, knobs.treble, knobs.bass,
                                                         knobs.middle, f);
            const double slope = slope_db_per_neper(SushiDSP::K_JCM2203_TONE_STACK, knobs.treble, knobs.bass,
                                                    knobs.middle, f);
            EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
                steady_gain_db(stack, K_SAMPLE_RATE, f), reference,
                warp_bound_db(slope, f, K_SAMPLE_RATE) + kMeasurementDb)))
                << "at " << f << " Hz";
        }
    }

    /// @brief Asserts that the stack at the row's middle position matches the SPICE sweep at every point from 20 Hz to 20 kHz.
    ///
    /// The slope for the warp bound is the sweep's own, read between the neighbouring points.
    void expect_matches_the_spice_sweep(int setting, const Knobs& knobs)
    {
        constexpr double K_SAMPLE_RATE = 192000.0;
        const auto sweep = SushiDSP::measure::load_ac_csv(spice_path("jcm_fmv", "ac"));
        std::vector<double> frequencies;
        std::vector<double> levels;
        for (std::size_t row = 0; row < sweep.frequency_hz.size(); ++row)
        {
            if (sweep.setting[row] == setting && sweep.frequency_hz[row] >= 20.0 && sweep.frequency_hz[row] <= 20000.0)
            {
                frequencies.push_back(sweep.frequency_hz[row]);
                levels.push_back(sweep.magnitude_db[row]);
            }
        }
        ASSERT_GT(frequencies.size(), 3u);
        SushiDSP::FmvToneStack stack = stack_at(K_SAMPLE_RATE, knobs);
        for (std::size_t index = 0; index < frequencies.size(); ++index)
        {
            const std::size_t low = index == 0 ? 0 : index - 1;
            const std::size_t high = index + 1 == frequencies.size() ? index : index + 1;
            const double slope = (levels[high] - levels[low]) / std::log(frequencies[high] / frequencies[low]);
            stack.reset();
            EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
                steady_gain_db(stack, K_SAMPLE_RATE, frequencies[index]), levels[index],
                warp_bound_db(slope, frequencies[index], K_SAMPLE_RATE) + kMeasurementDb + K_SPICE_PRINT_DB +
                    K_SPICE_SOLVER_DB)))
                << "setting " << setting << " at " << frequencies[index] << " Hz";
        }
    }

    /// @brief The component values of the Mark IIC+ deck, dossier section 4.1.
    constexpr SushiDSP::FmvComponents K_MESA_COMPONENTS{250e-12f, 100e-9f, 47e-9f, 100e3f, 250e3f, 10e3f, 250e3f};

    /// @brief The termination the loading rows share: V1A's 44 k plate impedance ahead of the deck and a 1 M volume pot across it.
    constexpr SushiDSP::FmvSourceLoad K_TERMINATION{44.0e3f, 1.0e6f};

    /// @brief Holds one deck, the tilt an independent nodal solve of its loaded network predicts, and its name.
    struct Deck
    {
            const char* name;
            const SushiDSP::FmvComponents* components;
            double expected_tilt_db;
    };

    const Deck kDecks[] = {{"Jcm2203", &SushiDSP::K_JCM2203_TONE_STACK, 3.89}, {"MarkIICPlus", &K_MESA_COMPONENTS, 0.87}};

    /// @brief Holds one deck and one probe frequency.
    struct DeckFrequency
    {
            const char* name;
            const Deck* deck;
            double frequency_hz;
    };

    const DeckFrequency kDeckFrequencies[] = {
        {"Jcm2203At41Hz", &kDecks[0], 41.0},       {"Jcm2203At1kHz", &kDecks[0], 1000.0},
        {"Jcm2203At10kHz", &kDecks[0], 10000.0},   {"MarkIICPlusAt41Hz", &kDecks[1], 41.0},
        {"MarkIICPlusAt1kHz", &kDecks[1], 1000.0}, {"MarkIICPlusAt10kHz", &kDecks[1], 10000.0},
    };

    /// @brief The loaded stack's gain minus the unloaded stack's at one frequency, at 192 kHz, in decibels.
    double loading_deviation_db(const SushiDSP::FmvComponents& components, double frequency_hz)
    {
        constexpr double K_SAMPLE_RATE = 192000.0;
        SushiDSP::FmvToneStack unloaded(K_SAMPLE_RATE, components);
        SushiDSP::FmvToneStack loaded(K_SAMPLE_RATE, components, K_TERMINATION);
        return steady_gain_db(loaded, K_SAMPLE_RATE, frequency_hz) - steady_gain_db(unloaded, K_SAMPLE_RATE, frequency_hz);
    }

    // The warp shifts each of the two stacks' levels by its slope times ln(1 + x^2 / 3); the slope of a level is read from the stack itself over a 5 % step, and the deviation is the difference of the two, so the bound is the sum of the two shifts.
    double loading_warp_bound_db(const SushiDSP::FmvComponents& components, double frequency_hz)
    {
        constexpr double K_SAMPLE_RATE = 192000.0;
        double total = 0.0;
        for (const bool with_termination : {false, true})
        {
            SushiDSP::FmvToneStack above = with_termination ? SushiDSP::FmvToneStack(K_SAMPLE_RATE, components, K_TERMINATION)
                                                            : SushiDSP::FmvToneStack(K_SAMPLE_RATE, components);
            SushiDSP::FmvToneStack below = with_termination ? SushiDSP::FmvToneStack(K_SAMPLE_RATE, components, K_TERMINATION)
                                                            : SushiDSP::FmvToneStack(K_SAMPLE_RATE, components);
            const double slope = (steady_gain_db(above, K_SAMPLE_RATE, frequency_hz * 1.05) -
                                  steady_gain_db(below, K_SAMPLE_RATE, frequency_hz / 1.05)) /
                                 (2.0 * std::log(1.05));
            total += warp_bound_db(slope, frequency_hz, K_SAMPLE_RATE);
        }
        return total + 2.0 * kMeasurementDb;
    }

    /// @brief Builds a stack at 0 Hz, which the derivation refuses, and drops the exception.
    void construct_at_zero_hertz_and_swallow_the_refusal()
    {
        try
        {
            SushiDSP::FmvToneStack stack(0.0, SushiDSP::K_JCM2203_TONE_STACK);
        }
        catch (const std::runtime_error&)
        {
        }
    }

    /// @brief Runs 512 samples of a ramp through @p unit, the way an audio block drives it.
    template <typename Unit> void process_ramp_block(Unit& unit)
    {
        for (int sample = 0; sample < 512; ++sample)
        {
            static_cast<void>(unit.process_sample(static_cast<float>(sample % 64) / 64.0f - 0.5f));
        }
    }
} // namespace

class Medium_FmvToneStackSpice : public ::testing::TestWithParam<int>
{
};

TEST_P(Medium_FmvToneStackSpice, FmvToneStackMatchesTheSpiceMidSweepWithinItsWarpBound)
{
    expect_matches_the_spice_sweep(GetParam(), kSpiceMiddles[GetParam()]);
}

INSTANTIATE_TEST_SUITE_P(Settings, Medium_FmvToneStackSpice, ::testing::Values(0, 1, 2),
                         [](const ::testing::TestParamInfo<int>& info) { return std::string(kSpiceMiddles[info.param].name); });

TEST(Small_FmvToneStack, FmvToneStackMidScoopDeepensAsMiddleCloses)
{
    constexpr double K_SAMPLE_RATE = 48000.0;
    SushiDSP::FmvToneStack open(K_SAMPLE_RATE, SushiDSP::K_JCM2203_TONE_STACK);
    SushiDSP::FmvToneStack closed(K_SAMPLE_RATE, SushiDSP::K_JCM2203_TONE_STACK);
    open.set_middle(0.9f);
    closed.set_middle(0.1f);
    // The closed form gives the scoop's depth; each measured level is within its warp bound of it.
    const double expected_gap = closed_form_gain_db(SushiDSP::K_JCM2203_TONE_STACK, 0.5, 0.5, 0.9, 1000.0) -
                                closed_form_gain_db(SushiDSP::K_JCM2203_TONE_STACK, 0.5, 0.5, 0.1, 1000.0);
    const double bound = warp_bound_db(slope_db_per_neper(SushiDSP::K_JCM2203_TONE_STACK, 0.5, 0.5, 0.9, 1000.0), 1000.0, K_SAMPLE_RATE) +
                         warp_bound_db(slope_db_per_neper(SushiDSP::K_JCM2203_TONE_STACK, 0.5, 0.5, 0.1, 1000.0), 1000.0, K_SAMPLE_RATE) +
                         2.0 * kMeasurementDb;

    const double measured_gap = steady_gain_db(open, K_SAMPLE_RATE, 1000.0) - steady_gain_db(closed, K_SAMPLE_RATE, 1000.0);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(measured_gap, expected_gap, bound)));
    EXPECT_GT(expected_gap, 6.0);
}

TEST(Small_FmvToneStack, FmvToneStackAtEveryKnobZeroIsFinite)
{
    SushiDSP::FmvToneStack stack = stack_at(48000.0, kExtremeKnobs[0]);

    EXPECT_TRUE(respond_to_sine(stack).all_finite);
}

TEST(Small_FmvToneStack, FmvToneStackAtEveryKnobZeroIsQuiet)
{
    SushiDSP::FmvToneStack stack = stack_at(48000.0, kExtremeKnobs[0]);

    const float peak = respond_to_sine(stack).peak;

    EXPECT_LT(20.0 * std::log10(std::max(static_cast<double>(peak), 1e-12)), K_QUIET_FLOOR_DB);
}

class Small_FmvToneStackExtremes : public ::testing::TestWithParam<Knobs>
{
};

TEST_P(Small_FmvToneStackExtremes, FmvToneStackImpulseResponseDecaysToNothingWithinOneSecond)
{
    // The slowest time constant of an RC network is at most the sum of its resistors times the sum of its capacitors, 28.7 ms here, so one second leaves e^(-34.8) of the peak, plus the double rounding of the state.
    SushiDSP::FmvToneStack stack = stack_at(48000.0, GetParam());
    const double slowest_seconds = (220e3 + 1e6 + 25e3 + 33e3) * (470e-12 + 22e-9 + 22e-9);

    const ImpulseTail tail = respond_to_impulse(stack);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        tail.last, 0.0, tail.peak * (std::exp(-1.0 / slowest_seconds) + 16.0 * kUDouble))));
}

INSTANTIATE_TEST_SUITE_P(Corners, Small_FmvToneStackExtremes, ::testing::ValuesIn(kExtremeKnobs), knob_name);

class Small_FmvToneStackClosedForm : public ::testing::TestWithParam<Knobs>
{
};

TEST_P(Small_FmvToneStackClosedForm, FmvToneStackNetlistReproducesTheClosedFormItReplaced)
{
    // 192 kHz keeps bilinear warping inside the tolerance. See tests/foundation/circuits/README.md.
    expect_matches_closed_form(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Knobs, Small_FmvToneStackClosedForm, ::testing::ValuesIn(kClosedFormKnobs), knob_name);

class Small_FmvToneStackLoading : public ::testing::TestWithParam<DeckFrequency>
{
};

TEST_P(Small_FmvToneStackLoading, FmvToneStackLoadingAttenuatesTheDeck)
{
    // The same termination costs each deck a different tilt. See tests/foundation/circuits/README.md.
    const DeckFrequency& row = GetParam();

    EXPECT_LT(loading_deviation_db(*row.deck->components, row.frequency_hz), 0.0);
}

INSTANTIATE_TEST_SUITE_P(Probes, Small_FmvToneStackLoading, ::testing::ValuesIn(kDeckFrequencies),
                         [](const ::testing::TestParamInfo<DeckFrequency>& info) { return std::string(info.param.name); });

class Small_FmvToneStackLoadingDeck : public ::testing::TestWithParam<Deck>
{
};

TEST_P(Small_FmvToneStackLoadingDeck, FmvToneStackLoadingTiltIsTheDecksOwn)
{
    const Deck& deck = GetParam();
    const double tilt = std::fabs(loading_deviation_db(*deck.components, 10000.0) -
                                  loading_deviation_db(*deck.components, 41.0));
    const double bound = loading_warp_bound_db(*deck.components, 10000.0) + loading_warp_bound_db(*deck.components, 41.0);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(tilt, deck.expected_tilt_db, bound)));
}

TEST_P(Small_FmvToneStackLoadingDeck, FmvToneStackLoadingCostsMoreThanOneDecibelAtOneKilohertz)
{
    EXPECT_GT(std::fabs(loading_deviation_db(*GetParam().components, 1000.0)), K_MINIMUM_LEVEL_COST_DB);
}

INSTANTIATE_TEST_SUITE_P(Decks, Small_FmvToneStackLoadingDeck, ::testing::ValuesIn(kDecks),
                         [](const ::testing::TestParamInfo<Deck>& info) { return std::string(info.param.name); });

TEST(Small_FmvToneStack, FmvToneStackThrowsWhenTheDerivationFails)
{
    EXPECT_THROW(SushiDSP::FmvToneStack(0.0, SushiDSP::K_JCM2203_TONE_STACK), std::runtime_error);
}

TEST(Small_FmvToneStack, FmvToneStackLogsOneErrorWhenTheDerivationFails)
{
    SushiDSP::LogCapture capture;

    construct_at_zero_hertz_and_swallow_the_refusal();

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "circuits");
    EXPECT_NE(lines[0].message.find("FmvToneStack"), std::string::npos) << lines[0].message;
}

TEST(Medium_FmvToneStack, ProcessSamplePerformsNoHeapAllocation)
{
    SushiDSP::FmvToneStack stack(48000.0, SushiDSP::K_JCM2203_TONE_STACK);
    stack.set_bass(0.3f);
    stack.set_middle(0.6f);
    stack.set_treble(0.9f);
    process_ramp_block(stack);
    SushiDSP::test::AllocationTrap trap;

    trap.arm();
    process_ramp_block(stack);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
