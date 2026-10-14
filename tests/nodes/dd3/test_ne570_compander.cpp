/**************************************************************************/
/* test_ne570_compander.cpp                                               */
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
 * @file test_ne570_compander.cpp
 * @brief Tests for the NE570 compander model.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <SushiDSP/nodes/dd3/ne570_compander.hpp>
#include <SushiDSP/foundation/signal_measure/spectrum.hpp>
#include <SushiDSP/foundation/signal_measure/thd.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    /// @brief Settled peak of a compander driven by a sine of the given peak amplitude.
    /// @param half Which half to build.
    /// @param input_peak_volts Peak of the drive signal.
    /// @param frequency_hz Drive frequency.
    /// @return Peak output in volts, measured over the last 20 cycles of a 500 ms run.
    double settled_peak(SushiDSP::dd3::CompanderHalf half, double input_peak_volts,
                        double frequency_hz)
    {
        SushiDSP::dd3::Ne570Compander compander(half);
        compander.prepare(48000.0);
        const std::size_t settle = 24000; // 500 ms, 73 time constants
        const std::size_t measure =
            static_cast<std::size_t>(std::round(20.0 * 48000.0 / frequency_hz));
        double peak = 0.0;
        for (std::size_t n = 0; n < settle + measure; ++n)
        {
            const float y = compander.process_sample(
                static_cast<float>(input_peak_volts * std::sin(2.0 * M_PI * frequency_hz *
                                                               static_cast<double>(n) / 48000.0)));
            if (n >= settle)
            {
                peak = std::max(peak, std::abs(static_cast<double>(y)));
            }
        }
        return peak;
    }
} // namespace

TEST(Unit_Dsp, Ne570CompressorFollowsTheTwoToOneLaw)
{
    // V_out = sqrt(V_ref * V_in) with V_ref = 6.6125 V. See tests/nodes/dd3/README.md.
    struct Point
    {
            double input;
            double output;
    };
    const Point points[4] = {
        {0.066125, 0.66125}, {0.66125, 2.0910639}, {6.6125, 6.6125}, {26.45, 13.225}};
    for (const Point& point : points)
    {
        EXPECT_NEAR(settled_peak(SushiDSP::dd3::CompanderHalf::Compressor, point.input, 1000.0),
                    point.output, 0.02 * point.output)
            << point.input;
    }
}

TEST(Unit_Dsp, Ne570ExpanderFollowsTheOneToTwoLaw)
{
    // V_out = V_in^2 / V_ref. See tests/nodes/dd3/README.md.
    struct Point
    {
            double input;
            double output;
    };
    const Point points[4] = {
        {0.66125, 0.066125}, {2.0910639, 0.66125}, {6.6125, 6.6125}, {13.225, 26.45}};
    for (const Point& point : points)
    {
        EXPECT_NEAR(settled_peak(SushiDSP::dd3::CompanderHalf::Expander, point.input, 1000.0),
                    point.output, 0.02 * point.output)
            << point.input;
    }
}

TEST(Unit_Dsp, Ne570RoundTripIsTheIdentityInTheStaticLimit)
{
    // The two halves are exact inverses on a steady tone: expander(compressor(x)) = x.
    for (const double input : {0.066125, 0.66125, 2.0910639, 6.6125, 13.225})
    {
        SushiDSP::dd3::Ne570Compander compressor(SushiDSP::dd3::CompanderHalf::Compressor);
        SushiDSP::dd3::Ne570Compander expander(SushiDSP::dd3::CompanderHalf::Expander);
        compressor.prepare(48000.0);
        expander.prepare(48000.0);
        double peak = 0.0;
        for (std::size_t n = 0; n < 24960; ++n)
        {
            const float y = expander.process_sample(compressor.process_sample(static_cast<float>(
                input * std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(n) / 48000.0))));
            if (n >= 24000)
            {
                peak = std::max(peak, std::abs(static_cast<double>(y)));
            }
        }
        EXPECT_NEAR(peak, input, 0.03 * input) << input;
    }
}

TEST(Unit_Dsp, Ne570EnvelopeReachesSixtyThreePercentAtSixPointEightMilliseconds)
{
    // tau = 10k * 0.68u = 6.8 ms, 326.4 samples at 48 kHz. See tests/nodes/dd3/README.md.
    SushiDSP::dd3::Ne570Compander expander(SushiDSP::dd3::CompanderHalf::Expander);
    expander.prepare(48000.0);
    const double target = 1.0; // a 1 V DC step reads 1 V * pi/2 rectified...
    std::size_t crossing = 0;
    for (std::size_t n = 0; n < 4800; ++n)
    {
        expander.process_sample(static_cast<float>(target));
        // The 63.21 % point is 0.6321206 * pi/2 = 0.9929407. See tests/nodes/dd3/README.md.
        if (crossing == 0 && expander.envelope_volts() >= 0.6321206 * M_PI / 2.0)
        {
            crossing = n + 1;
        }
    }
    EXPECT_NEAR(static_cast<double>(crossing), 326.4, 3.3); // +/- 1 %
    EXPECT_NEAR(SushiDSP::dd3::Ne570Compander::K_TIME_CONSTANT_SECONDS, 6.8e-3, 1e-12);
    EXPECT_NEAR(SushiDSP::dd3::Ne570Compander::K_RIPPLE_CORNER_HZ, 23.4051, 1e-3);
}

TEST(Unit_Dsp, Ne570CompressorGainSaturatesAtTheEnvelopeFloor)
{
    // Below 6.6125e-4 V the gain stops at 1e4. See tests/nodes/dd3/README.md.
    const double tiny = 1e-9;
    const double output = settled_peak(SushiDSP::dd3::CompanderHalf::Compressor, tiny, 1000.0);
    EXPECT_NEAR(output / tiny, 1.0e4, 1.0e2);
}

TEST(Unit_Dsp, Ne570RectifierRippleReachesTheSignalAtLowFrequencies)
{
    // The third harmonic has a floor, not a ceiling. See tests/nodes/dd3/README.md.
    SushiDSP::dd3::Ne570Compander compressor(SushiDSP::dd3::CompanderHalf::Compressor);
    compressor.prepare(48000.0);
    const double f0 =
        SushiDSP::measure::bin_exact_frequency(50.0, 48000.0, SushiDSP::measure::K_FFT_SIZE);
    std::vector<float> output(48000u + SushiDSP::measure::K_FFT_SIZE, 0.0f);
    for (std::size_t n = 0; n < output.size(); ++n)
    {
        output[n] = compressor.process_sample(static_cast<float>(
            6.6125 * std::sin(2.0 * M_PI * f0 * static_cast<double>(n) / 48000.0)));
    }
    const auto result = SushiDSP::measure::thd(output, f0, 48000.0, 5u);
    std::printf("[ measure ] NE570 compressor at 50 Hz / V_ref: THD %.3f %%, H3 %.2f dB\n",
                result.thd_percent, result.per_harmonic_db[2]);
    EXPECT_GT(result.per_harmonic_db[2], -40.0);
}

TEST(Unit_Dsp, Ne570RectifierDoesNotFoldItsOwnHarmonicsOntoTheCarrier)
{
    // The -70 dB bound covers the comb's tail from bin 4. See tests/nodes/dd3/README.md.
    SushiDSP::dd3::Ne570Compander compressor(SushiDSP::dd3::CompanderHalf::Compressor);
    compressor.prepare(48000.0);
    const double f0 =
        SushiDSP::measure::bin_exact_frequency(8000.0, 48000.0, SushiDSP::measure::K_FFT_SIZE);
    std::vector<float> output(48000u + SushiDSP::measure::K_FFT_SIZE, 0.0f);
    for (std::size_t n = 0; n < output.size(); ++n)
    {
        output[n] = compressor.process_sample(
            static_cast<float>(2.0 * std::sin(2.0 * M_PI * f0 * static_cast<double>(n) / 48000.0)));
    }
    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(output, 48000u);
    const std::uint32_t centre = SushiDSP::measure::bin_of(f0, 48000.0);
    const double carrier = SushiDSP::measure::peak_near(spectrum, centre);
    // A bin-exact tone's Blackman-Harris main lobe ends at +-3 bins, so the search starts at 4.
    double worst = -300.0;
    for (int offset = -12; offset <= 12; ++offset)
    {
        if (std::abs(offset) < 4)
        {
            continue;
        }
        worst = std::max(
            worst, spectrum[static_cast<std::size_t>(static_cast<int>(centre) + offset)] - carrier);
    }
    std::printf("[ measure ] NE570 rectifier fold at 48000/6: worst carrier sideband %.2f dB\n",
                worst);
    EXPECT_LE(worst, -70.0);
}

TEST(Unit_Dsp, Ne570HalvesDoNotShareState)
{
    // The expander is open-loop; instances share no state. See tests/nodes/dd3/README.md.
    SushiDSP::dd3::Ne570Compander first(SushiDSP::dd3::CompanderHalf::Expander);
    SushiDSP::dd3::Ne570Compander second(SushiDSP::dd3::CompanderHalf::Expander);
    first.prepare(48000.0);
    second.prepare(48000.0);
    for (std::size_t n = 0; n < 1000; ++n)
    {
        first.process_sample(1.0f);
        second.process_sample(0.0f);
    }
    EXPECT_GT(first.envelope_volts(), 1.0);
    EXPECT_NEAR(second.envelope_volts(), 0.0, 1e-9);
}
