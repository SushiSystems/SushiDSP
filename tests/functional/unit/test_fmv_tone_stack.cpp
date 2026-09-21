/**************************************************************************/
/* test_fmv_tone_stack.cpp                                                */
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

/**
 * @file test_fmv_tone_stack.cpp
 * @brief Nulls FmvToneStack against the jcm_fmv AC sweep and checks its knob extremes.
 * @author Mustafa Garip & Sushi Systems
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/FmvToneStack.hpp"
#include "measure/SpiceCsv.hpp"

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Measures a section's steady-state gain by projecting its output onto the drive.
    /// @param stack The section, left with its state at the end of the run.
    /// @param sample_rate The rate the section runs at, in hertz.
    /// @param frequency_hz The drive frequency.
    /// @return The gain in decibels.
    float steady_gain_db(SushiDSP::FmvToneStack& stack, double sample_rate, double frequency_hz)
    {
        // A whole number of cycles keeps the single-bin projection free of leakage.
        const long long settle = static_cast<long long>(sample_rate);
        const double cycles = std::ceil(0.25 * frequency_hz);
        const long long measure = static_cast<long long>(std::llround(cycles * sample_rate / frequency_hz));

        double real_part = 0.0, imaginary_part = 0.0;
        for (long long i = 0; i < settle + measure; ++i)
        {
            const double phase = 2.0 * kPi * frequency_hz * static_cast<double>(i) / sample_rate;
            const float y = stack.process_sample(static_cast<float>(std::sin(phase)));
            if (i >= settle)
            {
                real_part += y * std::sin(phase);
                imaginary_part += y * std::cos(phase);
            }
        }

        const double magnitude = 2.0 * std::sqrt(real_part * real_part + imaginary_part * imaginary_part)
                               / static_cast<double>(measure);
        return static_cast<float>(20.0 * std::log10(magnitude));
    }

    std::string spice_path(const std::string& block, const std::string& kind)
    {
        return std::string(SUSHIDSP_SPICE_DIR) + "/" + block + "/" + block + "_" + kind + ".csv";
    }
}

TEST(Unit_Dsp, FmvToneStackMatchesSpiceMidSweepWithinFiveHundredthsOfADecibel)
{
    const auto sweep = SushiDSP::measure::load_ac_csv(spice_path("jcm_fmv", "ac"));
    const float mid_positions[3] = { 0.1f, 0.5f, 0.9f };
    const double sample_rate = 192000.0;
    for (int setting = 0; setting < 3; ++setting)
    {
        SushiDSP::FmvToneStack stack(sample_rate, SushiDSP::kJcm2203ToneStack);
        stack.set_bass(0.5f);
        stack.set_treble(0.5f);
        stack.set_middle(mid_positions[setting]);
        for (std::size_t row = 0; row < sweep.frequency_hz.size(); ++row)
        {
            if (sweep.setting[row] != setting) continue;
            const double f = sweep.frequency_hz[row];
            if (f < 20.0 || f > 20000.0) continue;
            stack.reset();
            EXPECT_NEAR(steady_gain_db(stack, sample_rate, f), sweep.magnitude_db[row], 0.05)
                << "setting " << setting << " at " << f << " Hz";
        }
    }
}

TEST(Unit_Dsp, FmvToneStackMidScoopDeepensAsMiddleCloses)
{
    SushiDSP::FmvToneStack open(48000.0, SushiDSP::kJcm2203ToneStack);
    SushiDSP::FmvToneStack closed(48000.0, SushiDSP::kJcm2203ToneStack);
    open.set_middle(0.9f);
    closed.set_middle(0.1f);
    EXPECT_LT(steady_gain_db(closed, 48000.0, 1000.0), steady_gain_db(open, 48000.0, 1000.0) - 6.0f);
}

TEST(Unit_Dsp, FmvToneStackAtEveryKnobZeroIsFiniteAndQuiet)
{
    SushiDSP::FmvToneStack stack(48000.0, SushiDSP::kJcm2203ToneStack);
    stack.set_bass(0.0f);
    stack.set_middle(0.0f);
    stack.set_treble(0.0f);
    float peak = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float y = stack.process_sample(static_cast<float>(std::sin(2.0 * kPi * 500.0 * i / 48000.0)));
        ASSERT_TRUE(std::isfinite(y));
        peak = std::max(peak, std::fabs(y));
    }
    EXPECT_LT(20.0f * std::log10(std::max(peak, 1e-12f)), -60.0f);
}

TEST(Unit_Dsp, FmvToneStackIsStableAtEveryKnobExtreme)
{
    const float positions[2] = { 0.0f, 1.0f };
    for (float b : positions)
    {
        for (float m : positions)
        {
            for (float t : positions)
            {
                SushiDSP::FmvToneStack stack(48000.0, SushiDSP::kJcm2203ToneStack);
                stack.set_bass(b);
                stack.set_middle(m);
                stack.set_treble(t);
                float y = 0.0f;
                for (int i = 0; i < 48000; ++i) y = stack.process_sample(i == 0 ? 1.0f : 0.0f);
                EXPECT_LT(std::fabs(y), 1e-6f) << b << m << t;
            }
        }
    }
}

namespace
{
    /// @brief The hand-derived closed form FmvToneStack carried until 2026-09-20.
    ///
    /// Kept here as the reference the netlist must reproduce: it was nulled against the jcm_fmv
    /// SPICE sweep, so it is the one independent statement of the deck's unloaded response the
    /// repository holds. Its coefficients are the Yeh/TSC solution of the rheostat-wired stack
    /// with an ideal source and an open output.
    /// @param components The stack's component values.
    /// @param treble Treble position in [0, 1].
    /// @param bass Bass position in [0, 1].
    /// @param middle Middle position in [0, 1].
    /// @param frequency_hz The frequency to evaluate at.
    /// @return The magnitude in decibels.
    double closed_form_gain_db(const SushiDSP::FmvComponents& components,
                               double treble, double bass, double middle, double frequency_hz)
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
        const double b2 = t * (c1 * c2 * r1 * r4 + c1 * c3 * r1 * r4)
                        + l * (c1 * c2 * r1 * r2 + c1 * c2 * r2 * r4 + c1 * c3 * r2 * r4
                               + m * (c1 * c3 * r2 * r3 + c2 * c3 * r2 * r3))
                        + m * (c1 * c2 * r1 * r3 + c1 * c2 * r3 * r4 + c1 * c3 * r1 * r3 + c1 * c3 * r3 * r4);
        const double b3 = l * c1 * c2 * c3 * r2 * (t * r1 * r4 + m * (r1 * r3 + r3 * r4));
        const double a1 = c1 * r1 + c2 * r4 + c3 * r4 + l * (c1 * r2 + c2 * r2)
                        + m * (c1 * r3 + c2 * r3 + c3 * r3);
        const double a2 = c1 * c2 * r1 * r4 + c1 * c3 * r1 * r4
                        + l * (c1 * c2 * r1 * r2 + c1 * c2 * r2 * r4 + c1 * c3 * r2 * r4 + c2 * c3 * r2 * r4
                               + m * (c1 * c3 * r2 * r3 + c2 * c3 * r2 * r3))
                        + m * (c1 * c2 * r1 * r3 + c1 * c2 * r3 * r4 + c1 * c3 * r1 * r3 + c1 * c3 * r3 * r4);
        const double a3 = l * c1 * c2 * c3 * r2 * (r1 * r4 + m * (r1 * r3 + r3 * r4));

        const double w = 2.0 * kPi * frequency_hz;
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
}

TEST(Unit_Dsp, FmvToneStackNetlistReproducesTheClosedFormItReplaced)
{
    // 192 kHz keeps the bilinear warping of the closed form and of the DK discretisation well
    // inside the tolerance at 6 kHz, where a 48 kHz run would separate them by more than the
    // measurement's own error.
    const double sample_rate = 192000.0;
    const double treble_positions[3] = { 0.1, 0.5, 0.9 };
    const double bass_positions[2] = { 0.2, 0.8 };
    const double middle_positions[2] = { 0.1, 0.6 };
    const double frequencies[4] = { 82.0, 440.0, 2000.0, 6000.0 };

    for (double t : treble_positions)
    {
        for (double l : bass_positions)
        {
            for (double m : middle_positions)
            {
                SushiDSP::FmvToneStack stack(sample_rate, SushiDSP::kJcm2203ToneStack);
                stack.set_treble(static_cast<float>(t));
                stack.set_bass(static_cast<float>(l));
                stack.set_middle(static_cast<float>(m));
                for (double f : frequencies)
                {
                    stack.reset();
                    const double reference
                        = closed_form_gain_db(SushiDSP::kJcm2203ToneStack, t, l, m, f);
                    EXPECT_NEAR(steady_gain_db(stack, sample_rate, f), reference, 0.02)
                        << "treble " << t << " bass " << l << " middle " << m << " at " << f << " Hz";
                }
            }
        }
    }
}

TEST(Unit_Dsp, FmvToneStackLoadingCostIsTheDecksOwnAndNotOneFigure)
{
    // The same termination — V1A's 44 k plate impedance ahead of the deck and a 1 M volume pot
    // across it — costs the two decks different things, because the slope resistor sets how much
    // of a perturbation a 44 k source is: 33 k on the JCM against 100 k on the IIC+. So a model
    // cannot carry one loading figure for every amplifier, and terminating one deck says nothing
    // about another. An independent nodal solve of the same network predicts 3.89 dB of tilt for
    // the JCM's values and 0.87 dB for the IIC+'s.
    const double sample_rate = 192000.0;
    const SushiDSP::FmvSourceLoad termination{ 44.0e3f, 1.0e6f };

    // Dossier section 4.1: the Mark IIC+'s deck.
    const SushiDSP::FmvComponents mesa_components{
        250e-12f, 100e-9f, 47e-9f, 100e3f, 250e3f, 10e3f, 250e3f
    };

    struct Case
    {
        const char*                    name;
        const SushiDSP::FmvComponents& components;
        double                         expected_tilt_db;
    };
    const Case cases[2] = { { "JCM 2203", SushiDSP::kJcm2203ToneStack, 3.89 },
                            { "Mark IIC+", mesa_components, 0.87 } };

    for (const Case& deck : cases)
    {
        SushiDSP::FmvToneStack unloaded(sample_rate, deck.components);
        SushiDSP::FmvToneStack loaded(sample_rate, deck.components, termination);

        const double frequencies[3] = { 41.0, 1000.0, 10000.0 };
        double deviation_db[3]{};
        for (int index = 0; index < 3; ++index)
        {
            unloaded.reset();
            loaded.reset();
            deviation_db[index] = steady_gain_db(loaded, sample_rate, frequencies[index])
                                - steady_gain_db(unloaded, sample_rate, frequencies[index]);
            EXPECT_LT(deviation_db[index], 0.0)
                << deck.name << " must be attenuated at " << frequencies[index] << " Hz";
        }

        EXPECT_NEAR(std::fabs(deviation_db[2] - deviation_db[0]), deck.expected_tilt_db, 0.05)
            << deck.name << " tilt across 41 Hz to 10 kHz";
        EXPECT_GT(std::fabs(deviation_db[1]), 1.0) << deck.name << " level cost at 1 kHz";
    }
}
