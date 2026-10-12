/**************************************************************************/
/* test_biquad.cpp                                                        */
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
 * @file test_biquad.cpp
 * @brief Tests the Biquad filter designs against their analytic responses.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include "SushiDSP/math/biquad.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Unit_Dsp, BiquadLowPassPassesDcAtUnityGain)
{
    SushiDSP::Biquad filter;
    filter.set_low_pass(48000.0, 1000.0, 0.707);

    float output = 0.0f;
    for (int i = 0; i < 10000; ++i)
    {
        output = filter.process_sample(1.0f);
    }

    EXPECT_NEAR(output, 1.0f, 1e-3f);
}

TEST(Unit_Dsp, BiquadLowPassAttenuatesWellAboveCutoff)
{
    SushiDSP::Biquad filter;
    constexpr double sample_rate = 48000.0;
    constexpr double cutoff = 500.0;
    filter.set_low_pass(sample_rate, cutoff, 0.707);

    const double test_freq = 10000.0;
    float max_output = 0.0f;
    for (int i = 0; i < 2000; ++i)
    {
        const float input = static_cast<float>(std::sin(2.0 * M_PI * test_freq * i / sample_rate));
        const float output = filter.process_sample(input);
        if (i > 1000)
        {
            max_output = std::max(max_output, std::abs(output));
        }
    }

    EXPECT_LT(max_output, 0.1f);
}

TEST(Unit_Dsp, BiquadHighPassBlocksDcCompletely)
{
    SushiDSP::Biquad filter;
    filter.set_high_pass(48000.0, 1000.0, 0.707);

    float output = 0.0f;
    for (int i = 0; i < 10000; ++i)
    {
        output = filter.process_sample(1.0f);
    }

    EXPECT_NEAR(output, 0.0f, 1e-3f);
}

TEST(Unit_Dsp, BiquadHighPassPassesWellAboveCutoffAtUnityGain)
{
    SushiDSP::Biquad filter;
    constexpr double sample_rate = 48000.0;
    constexpr double cutoff = 500.0;
    filter.set_high_pass(sample_rate, cutoff, 0.707);

    const double test_freq = 10000.0;
    float max_output = 0.0f;
    for (int i = 0; i < 2000; ++i)
    {
        const float input = static_cast<float>(std::sin(2.0 * M_PI * test_freq * i / sample_rate));
        const float output = filter.process_sample(input);
        if (i > 1000)
        {
            max_output = std::max(max_output, std::abs(output));
        }
    }

    EXPECT_NEAR(max_output, 1.0f, 0.1f);
}

TEST(Unit_Dsp, BiquadLowShelfBoostsDcByApproximatelyGainDb)
{
    SushiDSP::Biquad filter;
    filter.set_low_shelf(48000.0, 200.0, 6.0, 0.707);

    float output = 0.0f;
    for (int i = 0; i < 10000; ++i)
    {
        output = filter.process_sample(1.0f);
    }

    const float expected_linear_gain = std::pow(10.0f, 6.0f / 20.0f);
    EXPECT_NEAR(output, expected_linear_gain, 0.05f);
}

TEST(Unit_Dsp, BiquadLowShelfLeavesHighFrequenciesNearUnity)
{
    SushiDSP::Biquad filter;
    constexpr double sample_rate = 48000.0;
    filter.set_low_shelf(sample_rate, 200.0, 6.0, 0.707);

    const double test_freq = 10000.0;
    float max_output = 0.0f;
    for (int i = 0; i < 2000; ++i)
    {
        const float input = static_cast<float>(std::sin(2.0 * M_PI * test_freq * i / sample_rate));
        const float output = filter.process_sample(input);
        if (i > 1000)
        {
            max_output = std::max(max_output, std::abs(output));
        }
    }

    EXPECT_NEAR(max_output, 1.0f, 0.1f);
}

TEST(Unit_Dsp, BiquadHighShelfBoostsHighFrequencyByApproximatelyGainDb)
{
    SushiDSP::Biquad filter;
    constexpr double sample_rate = 48000.0;
    filter.set_high_shelf(sample_rate, 2000.0, 6.0, 0.707);

    const double test_freq = 15000.0;
    float max_output = 0.0f;
    for (int i = 0; i < 2000; ++i)
    {
        const float input = static_cast<float>(std::sin(2.0 * M_PI * test_freq * i / sample_rate));
        const float output = filter.process_sample(input);
        if (i > 1000)
        {
            max_output = std::max(max_output, std::abs(output));
        }
    }

    const float expected_linear_gain = std::pow(10.0f, 6.0f / 20.0f);
    EXPECT_NEAR(max_output, expected_linear_gain, 0.15f);
}

TEST(Unit_Dsp, BiquadPeakingBoostsCenterFrequency)
{
    SushiDSP::Biquad filter;
    constexpr double sample_rate = 48000.0;
    constexpr double center = 1000.0;
    filter.set_peaking(sample_rate, center, 9.0, 1.0);

    float max_output = 0.0f;
    for (int i = 0; i < 3000; ++i)
    {
        const float input = static_cast<float>(std::sin(2.0 * M_PI * center * i / sample_rate));
        const float output = filter.process_sample(input);
        if (i > 2000)
        {
            max_output = std::max(max_output, std::abs(output));
        }
    }

    const float expected_linear_gain = std::pow(10.0f, 9.0f / 20.0f);
    EXPECT_NEAR(max_output, expected_linear_gain, 0.1f);
}

TEST(Unit_Dsp, BiquadPeakingLeavesFarFrequenciesNearUnity)
{
    SushiDSP::Biquad filter;
    constexpr double sample_rate = 48000.0;
    filter.set_peaking(sample_rate, 1000.0, 9.0, 1.0);

    const double test_freq = 50.0;
    float max_output = 0.0f;
    for (int i = 0; i < 6000; ++i)
    {
        const float input = static_cast<float>(std::sin(2.0 * M_PI * test_freq * i / sample_rate));
        const float output = filter.process_sample(input);
        if (i > 5000)
        {
            max_output = std::max(max_output, std::abs(output));
        }
    }

    EXPECT_NEAR(max_output, 1.0f, 0.15f);
}

TEST(Unit_Dsp, BiquadResetClearsState)
{
    SushiDSP::Biquad filter;
    filter.set_low_pass(48000.0, 1000.0, 0.707);

    for (int i = 0; i < 100; ++i)
    {
        filter.process_sample(1.0f);
    }
    filter.reset();

    const float output = filter.process_sample(0.0f);
    EXPECT_NEAR(output, 0.0f, 1e-3f);
}

TEST(Unit_Dsp, BiquadOnePoleLowPassMatchesTheAnalyticSingleRcResponse)
{
    // A one-pole loses 6 dB per octave, not 12. See tests/functional/unit/README.md.
    constexpr double sample_rate = 384000.0;
    constexpr double corner = 13800.0;
    SushiDSP::Biquad filter;
    filter.set_one_pole_low_pass(sample_rate, corner);

    auto gain_db_at = [&](double frequency)
    {
        filter.reset();
        const std::size_t length = 200000;
        double input_power = 0.0, output_power = 0.0;
        for (std::size_t i = 0; i < length; ++i)
        {
            const float x = static_cast<float>(
                std::sin(2.0 * M_PI * frequency * static_cast<double>(i) / sample_rate));
            const float y = filter.process_sample(x);
            if (i >= length / 2)
            {
                input_power += static_cast<double>(x) * x;
                output_power += static_cast<double>(y) * y;
            }
        }
        return 10.0 * std::log10(output_power / input_power);
    };

    auto analytic_db = [&](double frequency)
    {
        return -10.0 * std::log10(1.0 + (frequency / corner) * (frequency / corner));
    };

    // Bilinear warping pulls the response down above fs/8. See tests/functional/unit/README.md.
    for (const double frequency : {1000.0, 13800.0, 27600.0})
    {
        EXPECT_NEAR(gain_db_at(frequency), analytic_db(frequency), 0.15) << frequency << " Hz";
    }
    EXPECT_NEAR(gain_db_at(13800.0), -3.0, 0.1);
    EXPECT_NEAR(gain_db_at(27600.0) - gain_db_at(13800.0), -4.1, 0.3);
}

namespace
{
    /// @brief Magnitude response of a configured Biquad at one frequency, by driving it with a
    /// long sine and reading the settled peak.
    /// @param filter The filter, already configured; reset before use.
    /// @param frequency_hz Probe frequency.
    /// @param sample_rate Sample rate the filter was configured at.
    /// @return Peak output amplitude for a unit-amplitude input.
    double biquad_magnitude(SushiDSP::Biquad& filter, double frequency_hz, double sample_rate)
    {
        filter.reset();
        const std::size_t settle = 200000;
        const std::size_t measure =
            static_cast<std::size_t>(std::round(20.0 * sample_rate / frequency_hz));
        double peak = 0.0;
        for (std::size_t n = 0; n < settle + measure; ++n)
        {
            const float y = filter.process_sample(static_cast<float>(
                std::sin(2.0 * M_PI * frequency_hz * static_cast<double>(n) / sample_rate)));
            if (n >= settle)
            {
                peak = std::max(peak, std::abs(static_cast<double>(y)));
            }
        }
        return peak;
    }
} // namespace

TEST(Unit_Dsp, BiquadFirstOrderShelfPlacesItsCornersAndAsymptotesExactly)
{
    // The goldens are the post-warp readings at 96 kHz. See tests/functional/unit/README.md.
    SushiDSP::Biquad shelf;
    shelf.set_first_order_shelf(96000.0, 594.07, 3386.28, 33.0 / 47.0);
    EXPECT_NEAR(biquad_magnitude(shelf, 20.0, 96000.0), 0.7025132, 2e-3);
    EXPECT_NEAR(biquad_magnitude(shelf, 1000.0, 96000.0), 1.3184111, 3e-3);
    EXPECT_NEAR(biquad_magnitude(shelf, 6000.0, 96000.0), 3.5023974, 3e-2);
}

TEST(Unit_Dsp, BiquadFirstOrderShelfPairCancelsExactly)
{
    // The shelf pair cancels to a null below -120 dB. See tests/functional/unit/README.md.
    SushiDSP::Biquad pre, de;
    pre.set_first_order_shelf(48000.0, 594.07, 3386.28, 33.0 / 47.0);
    de.set_first_order_shelf(48000.0, 3386.28, 594.07, 47.0 / 33.0);
    double signal_power = 0.0;
    double error_power = 0.0;
    std::uint32_t state = 22695477u;
    for (std::size_t n = 0; n < 200000; ++n)
    {
        state = state * 1103515245u + 12345u;
        const float input = 0.5f * (static_cast<float>(state >> 16) / 32768.0f - 1.0f);
        const float output = de.process_sample(pre.process_sample(input));
        if (n >= 4096)
        {
            signal_power += static_cast<double>(input) * input;
            error_power += static_cast<double>(output - input) * (output - input);
        }
    }
    EXPECT_LE(10.0 * std::log10(error_power / signal_power), -120.0);
}

TEST(Unit_Dsp, BiquadFirstOrderShelfWithEqualCornersIsAFlatGain)
{
    // A degenerate but reachable configuration: zero == pole cancels, leaving a plain gain.
    SushiDSP::Biquad shelf;
    shelf.set_first_order_shelf(48000.0, 1000.0, 1000.0, 2.0);
    for (const double frequency : {30.0, 300.0, 3000.0, 15000.0})
    {
        EXPECT_NEAR(biquad_magnitude(shelf, frequency, 48000.0), 2.0, 1e-3) << frequency;
    }
}

namespace
{
    /// @brief Measures a filter's steady-state gain by projecting its output onto the drive.
    /// @param filter The filter, reset before the run and left with its state after it.
    /// @param frequency_hz The drive frequency in hertz.
    /// @param sample_rate The rate the filter runs at, in hertz.
    /// @return The magnitude response at that frequency.
    double biquad_projected_magnitude(SushiDSP::Biquad& filter, double frequency_hz,
                                      double sample_rate)
    {
        filter.reset();
        const long long settle = static_cast<long long>(sample_rate);
        // A whole number of cycles keeps the single-bin projection free of leakage.
        const double cycles = std::ceil(0.25 * frequency_hz);
        const long long measure =
            static_cast<long long>(std::llround(cycles * sample_rate / frequency_hz));

        double real_part = 0.0;
        double imaginary_part = 0.0;
        for (long long n = 0; n < settle + measure; ++n)
        {
            const double phase = 2.0 * M_PI * frequency_hz * static_cast<double>(n) / sample_rate;
            const float y = filter.process_sample(static_cast<float>(std::sin(phase)));
            if (n >= settle)
            {
                real_part += y * std::sin(phase);
                imaginary_part += y * std::cos(phase);
            }
        }

        return 2.0 * std::sqrt(real_part * real_part + imaginary_part * imaginary_part) /
               static_cast<double>(measure);
    }
} // namespace

TEST(Unit_Dsp, BiquadFirstOrderHighPassIsMinusThreeDecibelsAtItsCorner)
{
    // A single RC reads -0.04 dB a decade above its corner. See tests/functional/unit/README.md.
    SushiDSP::Biquad filter;
    filter.set_first_order_high_pass(48000.0, 100.0);

    const double at_corner = 20.0 * std::log10(biquad_projected_magnitude(filter, 100.0, 48000.0));
    const double a_decade_up =
        20.0 * std::log10(biquad_projected_magnitude(filter, 1000.0, 48000.0));

    EXPECT_NEAR(at_corner, -3.01, 0.1);
    EXPECT_NEAR(a_decade_up, -0.04, 0.1);
}

TEST(Unit_Dsp, BiquadFirstOrderHighPassStaysCleanAtLowCornersAtHighRates)
{
    // The one-pole's real pole stays inside the unit circle. See tests/functional/unit/README.md.
    constexpr double sample_rate = 384000.0;
    for (const double cutoff_hz : {2.0, 5.0, 7.23, 10.58})
    {
        SushiDSP::Biquad filter;
        filter.set_first_order_high_pass(sample_rate, cutoff_hz);

        const double measured =
            20.0 * std::log10(biquad_projected_magnitude(filter, 1000.0, sample_rate));
        const double analytic =
            20.0 * std::log10(1.0 / std::sqrt(1.0 + (cutoff_hz / 1000.0) * (cutoff_hz / 1000.0)));
        EXPECT_NEAR(measured, analytic, 0.01) << cutoff_hz;

        filter.reset();
        const float first = filter.process_sample(1.0f);
        float last = first;
        for (long long n = 1; n < static_cast<long long>(2.0 * sample_rate); ++n)
        {
            last = filter.process_sample(1.0f);
        }
        EXPECT_LT(std::abs(last), 1e-4f * std::abs(first)) << cutoff_hz;
    }
}

TEST(Unit_Dsp, BiquadSetCoefficientsLoadsThemVerbatim)
{
    // b = {0.5, 0.5, 0}, a = {0, 0} is a two-tap average whose impulse response is 0.5, 0.5, 0.
    SushiDSP::Biquad average;
    average.set_coefficients(0.5, 0.5, 0.0, 0.0, 0.0);
    EXPECT_FLOAT_EQ(average.process_sample(1.0f), 0.5f);
    EXPECT_FLOAT_EQ(average.process_sample(0.0f), 0.5f);
    EXPECT_FLOAT_EQ(average.process_sample(0.0f), 0.0f);

    // The impulse response is 1, 1/2, 1/4, 1/8. See tests/functional/unit/README.md.
    SushiDSP::Biquad one_pole;
    one_pole.set_coefficients(1.0, 0.0, 0.0, -0.5, 0.0);
    EXPECT_FLOAT_EQ(one_pole.process_sample(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(one_pole.process_sample(0.0f), 0.5f);
    EXPECT_FLOAT_EQ(one_pole.process_sample(0.0f), 0.25f);
    EXPECT_FLOAT_EQ(one_pole.process_sample(0.0f), 0.125f);
}
