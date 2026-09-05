/**************************************************************************/
/* test_biquad.cpp                                                        */
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

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and it must remain the very first thing in the translation unit.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include "SushiDSP/math/Biquad.hpp"

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
    // Golden values from the analytic single-RC magnitude 1/sqrt(1+(f/fc)^2), which the
    // prewarped bilinear form reproduces exactly at the corner and closely below fs/8.
    // The point of the case is the slope: a one-pole must lose 6 dB per octave, not the 12 dB
    // a Butterworth biquad loses, or it would darken a Miller-limited stage twice as fast as
    // the physics does.
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
            const float x = static_cast<float>(std::sin(2.0 * M_PI * frequency * static_cast<double>(i) / sample_rate));
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

    // Compared only up to fs/8; above that the bilinear map's frequency warping pulls the
    // discrete response below the analytic one by design (0.55 dB at 55 kHz here), which is the
    // usual and desirable direction for an anti-aliasing role.
    for (const double frequency : { 1000.0, 13800.0, 27600.0 })
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
        const std::size_t measure = static_cast<std::size_t>(std::round(20.0 * sample_rate / frequency_hz));
        double peak = 0.0;
        for (std::size_t n = 0; n < settle + measure; ++n)
        {
            const float y = filter.process_sample(
                static_cast<float>(std::sin(2.0 * M_PI * frequency_hz * static_cast<double>(n) / sample_rate)));
            if (n >= settle)
            {
                peak = std::max(peak, std::abs(static_cast<double>(y)));
            }
        }
        return peak;
    }
}

TEST(Unit_Dsp, BiquadFirstOrderShelfPlacesItsCornersAndAsymptotesExactly)
{
    // DD-3 pre-emphasis (DR1+DR2 plan N4): zero 594.07 Hz, pole 3386.28 Hz, LF gain 33/47.
    //   LF gain  = 0.7021277                     (-3.0745 dB)
    //   HF gain  = 0.7021277 * (3386.28/594.07)  = 0.7021277 * 5.7001364 = 4.0022236
    //   at 1 kHz = 0.7021277 * sqrt((1+(1000/594.07)^2)/(1+(1000/3386.28)^2))
    //            = 0.7021277 * 1.8778100 = 1.31855
    // The discretised HF asymptote is tan-warped and is NOT 4.0022236 at Nyquist; 12 kHz is far
    // enough below Nyquist that the warping is under 1 %, which is what the tolerance allows.
    SushiDSP::Biquad shelf;
    shelf.set_first_order_shelf(48000.0, 594.07, 3386.28, 33.0 / 47.0);
    EXPECT_NEAR(biquad_magnitude(shelf, 20.0, 48000.0), 0.7021277, 2e-3);
    EXPECT_NEAR(biquad_magnitude(shelf, 1000.0, 48000.0), 1.3183700, 3e-3);
    EXPECT_NEAR(biquad_magnitude(shelf, 12000.0, 48000.0), 4.0021712, 4.0e-2);
}

TEST(Unit_Dsp, BiquadFirstOrderShelfPairCancelsExactly)
{
    // N4: pre-emphasis (zero 594.07, pole 3386.28, gain 33/47) followed by de-emphasis (the same
    // two corners swapped, gain 47/33) is the identity, because the numerator polynomial of one
    // is the denominator polynomial of the other and the scalars multiply to 1. Assert a null
    // below -120 dB on a broadband signal, which no tolerance-based response check would catch.
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
    for (const double frequency : { 30.0, 300.0, 3000.0, 15000.0 })
    {
        EXPECT_NEAR(biquad_magnitude(shelf, frequency, 48000.0), 2.0, 1e-3) << frequency;
    }
}
