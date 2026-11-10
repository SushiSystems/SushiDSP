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
#include <vector>

#include <SushiDSP/nodes/dd3/ne570_compander.hpp>
#include <SushiDSP/foundation/signal_measure/spectrum.hpp>
#include <SushiDSP/foundation/signal_measure/thd.hpp>

#include "support/tolerance.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    using SushiDSP::dd3::CompanderHalf;
    using SushiDSP::dd3::Ne570Compander;

    /// @brief The reference level the published check points give, in volts.
    constexpr double K_PUBLISHED_REFERENCE_VOLTS = 6.6125;

    /// @brief The sample rate of every test here.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief The probe frequency of the static-law tests: 48 samples a cycle, so the sampled peak
    /// is the true peak.
    constexpr double K_LAW_HZ = 1000.0;

    /// @brief Unit roundoff of the float that carries the signal.
    constexpr double K_FLOAT_ROUNDOFF = TestSupport::unit_roundoff<float>();

    /// @brief How many times the envelope ripple's relative size the peak of a half may sit from the
    /// static law: once for the ripple in the envelope and once for the ripple it puts in the gain
    /// the envelope controls.
    constexpr double K_RIPPLE_PASSES = 2.0;

    /// @brief How far above the ideal third harmonic of the rectifier ripple the 50 Hz compressor
    /// tone must read, in dB below the carrier. Characterized from the first version of the test;
    /// the first-order estimate (an open-loop gain modulation of 0.152 at 100 Hz) is -22.4 dB.
    constexpr double K_RIPPLE_REACHES_DB = -40.0;

    /// @brief How far below the carrier the worst sideband beyond bin 3 must stay, in dB.
    /// Characterized from the first version of the test; it has no derivation yet.
    constexpr double K_FOLD_BOUND_DB = -70.0;

    /// @brief Bins from the carrier inside which a bin-exact Blackman-Harris tone has its main lobe.
    constexpr int K_MAIN_LOBE_BINS = 4;

    /// @brief Bins searched on each side of the carrier for a folded sideband.
    constexpr int K_SIDEBAND_SEARCH_BINS = 12;

    /// @brief Names an input peak in volts.
    struct LevelRow
    {
        const char* name;
        double input_volts;
    };

    /// @brief The compressor is probed from 1 % of the reference to four times it.
    const LevelRow K_COMPRESSOR_LEVELS[] = {{"Hundredth", 0.01 * K_PUBLISHED_REFERENCE_VOLTS},
                                            {"Tenth", 0.1 * K_PUBLISHED_REFERENCE_VOLTS},
                                            {"Reference", K_PUBLISHED_REFERENCE_VOLTS},
                                            {"Four", 4.0 * K_PUBLISHED_REFERENCE_VOLTS}};

    /// @brief The expander is probed where the compressor leaves its outputs.
    const LevelRow K_EXPANDER_LEVELS[] = {{"Tenth", 0.1 * K_PUBLISHED_REFERENCE_VOLTS},
                                          {"Mid", std::sqrt(K_PUBLISHED_REFERENCE_VOLTS * 0.1 * K_PUBLISHED_REFERENCE_VOLTS)},
                                          {"Reference", K_PUBLISHED_REFERENCE_VOLTS},
                                          {"Two", 2.0 * K_PUBLISHED_REFERENCE_VOLTS}};

    /// @brief The round trip is probed across the same span.
    const LevelRow K_ROUND_TRIP_LEVELS[] = {{"Hundredth", 0.01 * K_PUBLISHED_REFERENCE_VOLTS},
                                            {"Tenth", 0.1 * K_PUBLISHED_REFERENCE_VOLTS},
                                            {"Mid", std::sqrt(K_PUBLISHED_REFERENCE_VOLTS * 0.1 * K_PUBLISHED_REFERENCE_VOLTS)},
                                            {"Reference", K_PUBLISHED_REFERENCE_VOLTS},
                                            {"Two", 2.0 * K_PUBLISHED_REFERENCE_VOLTS}};

    /// @brief The relative size of the envelope ripple a one-pole smoother leaves on a rectified sine.
    ///
    /// The full-wave rectified sine, scaled by pi/2, is 1 - 2 sum cos(2k w t)/(4k^2 - 1). The smoother
    /// passes harmonic k with a gain of at most f_c/(2 k f), so the ripple is at most
    /// (f_c/f) sum 1/(k (4k^2 - 1)) = (2 ln 2 - 1) f_c/f.
    double envelope_ripple(double frequency_hz)
    {
        return (2.0 * std::log(2.0) - 1.0) * Ne570Compander::K_RIPPLE_CORNER_HZ / frequency_hz;
    }

    /// @brief The relative error bound of one half's settled peak against its static law.
    double law_bound(double frequency_hz)
    {
        return K_RIPPLE_PASSES * envelope_ripple(frequency_hz) + 2.0 * K_FLOAT_ROUNDOFF;
    }

    /// @brief Settled peak of a compander driven by a sine of the given peak amplitude.
    /// @param compander A prepared compander.
    /// @param input_peak_volts Peak of the drive signal.
    /// @param frequency_hz Drive frequency, dividing the sample rate into a whole number of samples.
    /// @return Peak output in volts, measured over the last 20 cycles of a 500 ms run.
    template <typename Chain>
    double settled_peak(Chain& chain, double input_peak_volts, double frequency_hz)
    {
        const std::size_t settle = 24000; // 500 ms, 73 time constants
        const std::size_t measure = static_cast<std::size_t>(std::lround(20.0 * K_SAMPLE_RATE / frequency_hz));
        double peak = 0.0;
        for (std::size_t n = 0; n < settle + measure; ++n)
        {
            const float y = chain.process_sample(static_cast<float>(
                input_peak_volts * std::sin(2.0 * M_PI * frequency_hz * static_cast<double>(n) / K_SAMPLE_RATE)));
            if (n >= settle)
            {
                peak = std::max(peak, std::abs(static_cast<double>(y)));
            }
        }
        return peak;
    }

    /// @brief A compander of one half, prepared at K_SAMPLE_RATE.
    Ne570Compander prepared(CompanderHalf half)
    {
        Ne570Compander compander(half);
        compander.prepare(K_SAMPLE_RATE);
        return compander;
    }

    /// @brief The compressor followed by the expander, as one chain.
    class RoundTrip
    {
        public:
            RoundTrip() : compressor_(prepared(CompanderHalf::Compressor)), expander_(prepared(CompanderHalf::Expander))
            {
            }

            float process_sample(float volts)
            {
                return expander_.process_sample(compressor_.process_sample(volts));
            }

        private:
            Ne570Compander compressor_;
            Ne570Compander expander_;
    };

    /// @brief The envelope of the expander after `samples` samples of a 1 V DC step, in closed form.
    ///
    /// The expander rectifies the input in 8 linear sub-steps per sample, so the first sample
    /// ramps the rectifier input as j/8 for j = 1..8 and every later sub-step reads 1. With a = exp(-1/(8 fs tau))
    /// the one-pole sum is E = (pi/2) [1 - a^M - (1 - a) sum_j (1 - j/8) a^(M - j)] for M = 8 samples.
    double step_envelope(std::size_t samples)
    {
        const double a = std::exp(-1.0 / (static_cast<double>(Ne570Compander::K_RECTIFIER_OVERSAMPLING) * K_SAMPLE_RATE *
                                          Ne570Compander::K_TIME_CONSTANT_SECONDS));
        const double sub_steps = static_cast<double>(Ne570Compander::K_RECTIFIER_OVERSAMPLING) * static_cast<double>(samples);
        double missing = 0.0;
        for (int j = 1; j <= Ne570Compander::K_RECTIFIER_OVERSAMPLING; ++j)
        {
            missing += (1.0 - j / static_cast<double>(Ne570Compander::K_RECTIFIER_OVERSAMPLING)) * std::pow(a, sub_steps - j);
        }
        return M_PI / 2.0 * (1.0 - std::pow(a, sub_steps) - (1.0 - a) * missing);
    }

    /// @brief The first sample count at which the closed-form step envelope reaches `level`.
    std::size_t first_sample_reaching(double level)
    {
        std::size_t samples = 1;
        while (step_envelope(samples) < level)
        {
            ++samples;
        }
        return samples;
    }

    /// @brief The first sample count at which the expander's own envelope reaches `level` under a 1 V step.
    std::size_t measured_crossing(double level, std::size_t limit)
    {
        Ne570Compander expander = prepared(CompanderHalf::Expander);
        for (std::size_t n = 0; n < limit; ++n)
        {
            expander.process_sample(1.0f);
            if (expander.envelope_volts() >= level)
            {
                return n + 1;
            }
        }
        return 0;
    }

    /// @brief Renders a compressor driven by a bin-exact sine for one second plus one FFT.
    std::vector<float> render_compressor_sine(double frequency_hz, double peak_volts)
    {
        Ne570Compander compressor = prepared(CompanderHalf::Compressor);
        std::vector<float> output(48000u + SushiDSP::measure::K_FFT_SIZE, 0.0f);
        for (std::size_t n = 0; n < output.size(); ++n)
        {
            output[n] = compressor.process_sample(
                static_cast<float>(peak_volts * std::sin(2.0 * M_PI * frequency_hz * static_cast<double>(n) / K_SAMPLE_RATE)));
        }
        return output;
    }

    /// @brief The loudest bin, relative to the carrier, between the main lobe and the search edge.
    double worst_sideband_db(const std::vector<double>& spectrum, std::uint32_t centre, double carrier_db)
    {
        double worst = -300.0;
        for (int offset = -K_SIDEBAND_SEARCH_BINS; offset <= K_SIDEBAND_SEARCH_BINS; ++offset)
        {
            if (std::abs(offset) >= K_MAIN_LOBE_BINS)
            {
                worst = std::max(worst, spectrum[static_cast<std::size_t>(static_cast<int>(centre) + offset)] - carrier_db);
            }
        }
        return worst;
    }
} // namespace

TEST(Small_Ne570Compander, Ne570CompressorFollowsTheTwoToOneLaw)
{
    // V_out = sqrt(V_ref * V_in) with V_ref = 6.6125 V. See tests/nodes/dd3/README.md.
    for (const LevelRow& row : K_COMPRESSOR_LEVELS)
    {
        Ne570Compander compressor = prepared(CompanderHalf::Compressor);
        const double expected = std::sqrt(K_PUBLISHED_REFERENCE_VOLTS * row.input_volts);

        EXPECT_NEAR(settled_peak(compressor, row.input_volts, K_LAW_HZ), expected, law_bound(K_LAW_HZ) * expected)
            << row.name;
    }
}

TEST(Small_Ne570Compander, Ne570ExpanderFollowsTheOneToTwoLaw)
{
    // V_out = V_in^2 / V_ref. See tests/nodes/dd3/README.md.
    for (const LevelRow& row : K_EXPANDER_LEVELS)
    {
        Ne570Compander expander = prepared(CompanderHalf::Expander);
        const double expected = row.input_volts * row.input_volts / K_PUBLISHED_REFERENCE_VOLTS;

        EXPECT_NEAR(settled_peak(expander, row.input_volts, K_LAW_HZ), expected, law_bound(K_LAW_HZ) * expected)
            << row.name;
    }
}

TEST(Small_Ne570Compander, Ne570RoundTripIsTheIdentityInTheStaticLimit)
{
    // The two halves are exact inverses on a steady tone: expander(compressor(x)) = x.
    const double half_error = law_bound(K_LAW_HZ);
    const double bound = std::pow(1.0 + half_error, 3.0) - 1.0;
    for (const LevelRow& row : K_ROUND_TRIP_LEVELS)
    {
        RoundTrip chain;

        EXPECT_NEAR(settled_peak(chain, row.input_volts, K_LAW_HZ), row.input_volts, bound * row.input_volts)
            << row.name;
    }
}

TEST(Small_Ne570Compander, Ne570EnvelopeReachesSixtyThreePercentAtSixPointEightMilliseconds)
{
    // tau = 10k * 0.68u = 6.8 ms, 326.4 samples at 48 kHz. See tests/nodes/dd3/README.md.
    const double level = (1.0 - std::exp(-1.0)) * M_PI / 2.0;

    EXPECT_EQ(measured_crossing(level, 4800), first_sample_reaching(level));
}

TEST(Small_Ne570Compander, Ne570TimeConstantAndRippleCornerFollowFromTheRectifierComponents)
{
    // The ripple corner is printed to six digits, so it may sit half a unit of the last digit away.
    const double tau = Ne570Compander::K_RECTIFIER_RESISTANCE_OHMS * Ne570Compander::K_RECTIFIER_CAPACITANCE_FARADS;
    const double exact_corner = 1.0 / (2.0 * M_PI * tau);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(
        Ne570Compander::K_TIME_CONSTANT_SECONDS, tau, 4)));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        Ne570Compander::K_RIPPLE_CORNER_HZ, exact_corner, 0.5e-4, 1.0)));
}

TEST(Small_Ne570Compander, Ne570CompressorGainSaturatesAtTheEnvelopeFloor)
{
    // Below 6.6125e-4 V the gain stops at 1e4. See tests/nodes/dd3/README.md.
    const double tiny = 1e-9;
    Ne570Compander compressor = prepared(CompanderHalf::Compressor);

    const double output = settled_peak(compressor, tiny, K_LAW_HZ);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(output / tiny, 1.0e4, 4)));
}

TEST(Small_Ne570Compander, Ne570RectifierRippleReachesTheSignalAtLowFrequencies)
{
    // The third harmonic has a floor, not a ceiling. See tests/nodes/dd3/README.md.
    const double f0 = SushiDSP::measure::bin_exact_frequency(50.0, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE);
    const std::vector<float> output = render_compressor_sine(f0, K_PUBLISHED_REFERENCE_VOLTS);

    const auto result = SushiDSP::measure::thd(output, f0, K_SAMPLE_RATE, 5u);

    EXPECT_GT(result.per_harmonic_db[2], K_RIPPLE_REACHES_DB);
}

TEST(Small_Ne570Compander, Ne570RectifierDoesNotFoldItsOwnHarmonicsOntoTheCarrier)
{
    // The bound covers the comb's tail from bin 4. See tests/nodes/dd3/README.md.
    const double f0 = SushiDSP::measure::bin_exact_frequency(8000.0, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE);
    const std::vector<float> output = render_compressor_sine(f0, 2.0);
    const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(output, 48000u);
    const std::uint32_t centre = SushiDSP::measure::bin_of(f0, K_SAMPLE_RATE);

    const double worst = worst_sideband_db(spectrum, centre, SushiDSP::measure::peak_near(spectrum, centre));

    EXPECT_LE(worst, K_FOLD_BOUND_DB);
}

TEST(Small_Ne570Compander, Ne570HalvesDoNotShareState)
{
    // The expander is open-loop; instances share no state. See tests/nodes/dd3/README.md.
    const std::size_t samples = 1000;
    Ne570Compander first = prepared(CompanderHalf::Expander);
    Ne570Compander second = prepared(CompanderHalf::Expander);
    for (std::size_t n = 0; n < samples; ++n)
    {
        first.process_sample(1.0f);
        second.process_sample(0.0f);
    }

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(
        first.envelope_volts(), step_envelope(samples), 3u * 8u * samples)));
    EXPECT_EQ(second.envelope_volts(), 0.0);
}
