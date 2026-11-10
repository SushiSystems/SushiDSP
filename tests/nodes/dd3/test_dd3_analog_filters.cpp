/**************************************************************************/
/* test_dd3_analog_filters.cpp                                            */
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
 * @file test_dd3_analog_filters.cpp
 * @brief Tests for the DD-3 anti-alias, reconstruction and emphasis filters.
 * @author Mustafa Garip & Sushi Systems
 */

#include <algorithm>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/dd3/dd3_analog_filters.hpp>

#include "support/tolerance.hpp"

namespace
{
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief The published delay-path window around the 1 kHz response, upper edge in dB.
    constexpr double K_WINDOW_UPPER_DB = 1.0;

    /// @brief The published delay-path window around the 1 kHz response, lower edge in dB.
    constexpr double K_WINDOW_LOWER_DB = -3.0;

    /// @brief The window's lowest probe in Hz.
    constexpr double K_WINDOW_LOW_HZ = 40.0;

    /// @brief The window's highest probe in Hz, the 7 kHz band edge.
    constexpr double K_WINDOW_HIGH_HZ = 7000.0;

    /// @brief The spacing every probe frequency is a multiple of, in Hz: one over the window.
    constexpr double K_PROBE_GRID_HZ = 20.0;

    /// @brief How far the digital response may sit from the damped analogue prototype between 1 and
    /// 7 kHz, relative to the prototype. Characterized: an offline evaluation of both responses reads
    /// 0.446 % at worst (the anti-alias cascade at 48 kHz, 5 kHz); it has no closed-form derivation yet.
    constexpr double K_PROTOTYPE_AGREEMENT = 0.005;

    /// @brief Seconds each probe discards before it measures. The slowest pole of any filter here
    /// is the 594 Hz de-emphasis pole, a time constant of 0.27 ms, so 10 ms is 37 of them.
    constexpr double K_SETTLE_SECONDS = 0.01;

    /// @brief Seconds each probe measures. At 1/K_PROBE_GRID_HZ every probe frequency fits a whole
    /// number of cycles in the window, so the quadrature estimate is exact.
    constexpr double K_WINDOW_SECONDS = 1.0 / K_PROBE_GRID_HZ;

    /// @brief Samples of an impulse response summed for its L1 norm. Every pole is faster than 0.3 ms.
    constexpr std::size_t K_IMPULSE_FRAMES = 4096;

    /// @brief Unit roundoff of the float that carries the signal between sections.
    constexpr double K_FLOAT_ROUNDOFF = TestSupport::unit_roundoff<float>();

    /// @brief Names the capacitors of one Sallen-Key section and the constants the code holds for it.
    struct SectionRow
    {
        const char* name;
        double c1;
        double c2;
        double frequency;
        double quality_factor;
    };

    /// @brief The five sections of the board; C1 bridges to the emitter, C2 shunts the base.
    const SectionRow K_SECTION_ROWS[] = {
        {"AntiAlias1", 4.7e-9, 2.7e-9, SushiDSP::dd3::K_ANTI_ALIAS_SECTION_FREQUENCIES[0],
         SushiDSP::dd3::K_ANTI_ALIAS_SECTION_Q[0]},
        {"AntiAlias2", 6.8e-9, 4.7e-10, SushiDSP::dd3::K_ANTI_ALIAS_SECTION_FREQUENCIES[1],
         SushiDSP::dd3::K_ANTI_ALIAS_SECTION_Q[1]},
        {"AntiAlias3", 1.8e-8, 2.2e-10, SushiDSP::dd3::K_ANTI_ALIAS_SECTION_FREQUENCIES[2],
         SushiDSP::dd3::K_ANTI_ALIAS_SECTION_Q[2]},
        {"Reconstruction1", 2.7e-9, 1.2e-9, SushiDSP::dd3::K_RECONSTRUCTION_SECTION_FREQUENCIES[0],
         SushiDSP::dd3::K_RECONSTRUCTION_SECTION_Q[0]},
        {"Reconstruction2", 6.8e-9, 1.5e-10, SushiDSP::dd3::K_RECONSTRUCTION_SECTION_FREQUENCIES[1],
         SushiDSP::dd3::K_RECONSTRUCTION_SECTION_Q[1]},
    };

    /// @brief Names a sample rate to prepare a filter at.
    struct RateRow
    {
        const char* name;
        double sample_rate;
    };

    const RateRow K_PASSBAND_RATES[] = {
        {"Rate48k", 48000.0}, {"Rate96k", 96000.0}, {"Rate192k", 192000.0}};

    const RateRow K_NODE_RATES[] = {{"Rate48k", 48000.0}, {"Rate96k", 96000.0}};

    /// @brief Names a probe frequency, a multiple of K_PROBE_GRID_HZ.
    struct ProbeRow
    {
        const char* name;
        double hz;
    };

    const ProbeRow K_BAND_PROBES[] = {
        {"Hz1000", 1000.0}, {"Hz2000", 2000.0}, {"Hz5000", 5000.0}, {"Hz7000", 7000.0}};

    const ProbeRow K_EMPHASIS_PROBES[] = {{"Hz20", 20.0}, {"Hz1000", 1000.0}, {"Hz6000", 6000.0}};

    const ProbeRow K_RECIPROCAL_PROBES[] = {{"Hz20", 20.0}, {"Hz1000", 1000.0}, {"Hz5000", 5000.0}};

    /// @brief Names a stopband probe and the sample rate it runs at.
    struct StopbandRow
    {
        const char* name;
        double sample_rate;
        double hz;
    };

    const StopbandRow K_STOPBAND_ROWS[] = {{"Hz10000At48k", 48000.0, 10000.0},
                                           {"Hz20000At48k", 48000.0, 20000.0},
                                           {"Hz10000At96k", 96000.0, 10000.0},
                                           {"Hz20000At96k", 96000.0, 20000.0}};

    /// @brief Passes true when `computed` is within a bound that is already the worst case.
    ::testing::AssertionResult within_bound(double computed, double reference, double bound)
    {
        return TestSupport::is_within(TestSupport::measure_error(computed, reference, bound, 1.0));
    }

    /// @brief Passes true when `computed` is within `operations` double roundings of `reference`.
    ::testing::AssertionResult within_double_roundings(double computed, double reference,
                                                       std::size_t operations)
    {
        return TestSupport::is_within(
            TestSupport::measure_relative_error<double>(computed, reference, operations));
    }

    /// @brief The response of a filter to a unit sine, read as the quadrature amplitude over a
    /// window of whole cycles, after it has settled.
    /// @param filter Any DD-3 filter: it has reset() and process_sample().
    /// @param hz Probe frequency, a multiple of K_PROBE_GRID_HZ.
    /// @param sample_rate Sample rate the filter was prepared at.
    /// @return |H| for a unit input.
    template <typename Filter>
    double gain_at(Filter& filter, double hz, double sample_rate)
    {
        filter.reset();
        const std::size_t settle = static_cast<std::size_t>(std::lround(K_SETTLE_SECONDS * sample_rate));
        const std::size_t window = static_cast<std::size_t>(std::lround(K_WINDOW_SECONDS * sample_rate));
        const double omega = 2.0 * K_PI * hz / sample_rate;
        double in_phase = 0.0;
        double quadrature = 0.0;
        for (std::size_t n = 0; n < settle + window; ++n)
        {
            const double phase = omega * static_cast<double>(n);
            const double output = filter.process_sample(static_cast<float>(std::sin(phase)));
            if (n >= settle)
            {
                in_phase += output * std::cos(phase);
                quadrature += output * std::sin(phase);
            }
        }
        return 2.0 / static_cast<double>(window) * std::hypot(in_phase, quadrature);
    }

    /// @brief The sum of the absolute impulse response of a filter, over K_IMPULSE_FRAMES samples.
    template <typename Filter>
    double l1_norm(Filter& filter)
    {
        filter.reset();
        double norm = std::abs(static_cast<double>(filter.process_sample(1.0f)));
        for (std::size_t n = 1; n < K_IMPULSE_FRAMES; ++n)
        {
            norm += std::abs(static_cast<double>(filter.process_sample(0.0f)));
        }
        return norm;
    }

    /// @brief The worst-case error of gain_at() that the float conversions cause: 2 (sections + 1) u
    /// times the L1 norm of the chain. The derivation is in tests/nodes/dd3/README.md.
    /// @param filter The prepared filter.
    /// @param sections Number of sections in the chain.
    /// @return The bound on the difference between gain_at() and the exact digital response.
    template <typename Filter>
    double rounding_bound(Filter& filter, std::size_t sections)
    {
        return 2.0 * static_cast<double>(sections + 1) * K_FLOAT_ROUNDOFF * l1_norm(filter);
    }

    /// @brief Magnitude of one second-order low-pass as the bilinear transform with its corner
    /// prewarped gives it: H = 1/(1 - x^2 + j x/Q) with x = tan(pi f/fs)/tan(pi fc/fs).
    double low_pass_magnitude(double corner_hz, double q, double hz, double sample_rate)
    {
        const double x = std::tan(K_PI * hz / sample_rate) / std::tan(K_PI * corner_hz / sample_rate);
        return 1.0 / std::sqrt((1.0 - x * x) * (1.0 - x * x) + (x / q) * (x / q));
    }

    /// @brief Magnitude of the cascade the node builds: each corner prewarped, each Q damped.
    double digital_cascade_magnitude(const double* corners, const double* quality_factors,
                                     std::size_t sections, double hz, double sample_rate)
    {
        double magnitude = 1.0;
        for (std::size_t i = 0; i < sections; ++i)
        {
            magnitude *= low_pass_magnitude(SushiDSP::dd3::prewarped_corner_hz(corners[i], sample_rate),
                                            SushiDSP::dd3::emitter_follower_damped_q(quality_factors[i]),
                                            hz, sample_rate);
        }
        return magnitude;
    }

    double digital_anti_alias_magnitude(double hz, double sample_rate)
    {
        return digital_cascade_magnitude(SushiDSP::dd3::K_ANTI_ALIAS_SECTION_FREQUENCIES,
                                         SushiDSP::dd3::K_ANTI_ALIAS_SECTION_Q,
                                         SushiDSP::dd3::Dd3AntiAliasFilter::K_SECTION_COUNT, hz,
                                         sample_rate);
    }

    double digital_reconstruction_magnitude(double hz, double sample_rate)
    {
        return digital_cascade_magnitude(SushiDSP::dd3::K_RECONSTRUCTION_SECTION_FREQUENCIES,
                                         SushiDSP::dd3::K_RECONSTRUCTION_SECTION_Q,
                                         SushiDSP::dd3::Dd3ReconstructionFilter::K_SECTION_COUNT, hz,
                                         sample_rate);
    }

    /// @brief Magnitude of the analogue Sallen-Key section with its buffer damping applied.
    double analog_section_magnitude(double corner_hz, double unity_gain_q, double hz)
    {
        const double q = SushiDSP::dd3::emitter_follower_damped_q(unity_gain_q);
        const double x = hz / corner_hz;
        return 1.0 / std::sqrt((1.0 - x * x) * (1.0 - x * x) + (x / q) * (x / q));
    }

    double analog_anti_alias_magnitude(double hz)
    {
        double magnitude = 1.0;
        for (std::size_t i = 0; i < SushiDSP::dd3::Dd3AntiAliasFilter::K_SECTION_COUNT; ++i)
        {
            magnitude *= analog_section_magnitude(SushiDSP::dd3::K_ANTI_ALIAS_SECTION_FREQUENCIES[i],
                                                  SushiDSP::dd3::K_ANTI_ALIAS_SECTION_Q[i], hz);
        }
        return magnitude;
    }

    double analog_reconstruction_magnitude(double hz)
    {
        double magnitude = 1.0;
        for (std::size_t i = 0; i < SushiDSP::dd3::Dd3ReconstructionFilter::K_SECTION_COUNT; ++i)
        {
            magnitude *=
                analog_section_magnitude(SushiDSP::dd3::K_RECONSTRUCTION_SECTION_FREQUENCIES[i],
                                         SushiDSP::dd3::K_RECONSTRUCTION_SECTION_Q[i], hz);
        }
        return magnitude;
    }

    /// @brief Magnitude of the first-order shelf with both corners prewarped:
    /// H = g (1 + j t/tz)/(1 + j t/tp), t = tan(pi f/fs), tz and tp the tangents of the corners.
    double digital_shelf_magnitude(double zero_hz, double pole_hz, double low_gain, double hz,
                                   double sample_rate)
    {
        const double t = std::tan(K_PI * hz / sample_rate);
        const double zero_ratio = t / std::tan(K_PI * zero_hz / sample_rate);
        const double pole_ratio = t / std::tan(K_PI * pole_hz / sample_rate);
        return low_gain * std::sqrt((1.0 + zero_ratio * zero_ratio) / (1.0 + pole_ratio * pole_ratio));
    }

    double digital_pre_emphasis_magnitude(double hz, double sample_rate)
    {
        return digital_shelf_magnitude(SushiDSP::dd3::K_EMPHASIS_ZERO_HZ, SushiDSP::dd3::K_EMPHASIS_POLE_HZ,
                                       SushiDSP::dd3::K_PRE_EMPHASIS_LOW_GAIN, hz, sample_rate);
    }

    /// @brief The analogue pre-emphasis shelf, C5 dropped: the zero/pole pair scaled by R12/R7.
    double analog_pre_emphasis_magnitude(double hz)
    {
        const double zero_ratio = hz / SushiDSP::dd3::K_EMPHASIS_ZERO_HZ;
        const double pole_ratio = hz / SushiDSP::dd3::K_EMPHASIS_POLE_HZ;
        return SushiDSP::dd3::K_PRE_EMPHASIS_LOW_GAIN *
               std::sqrt((1.0 + zero_ratio * zero_ratio) / (1.0 + pole_ratio * pole_ratio));
    }

    /// @brief Probe frequencies from 40 Hz to 7 kHz in sixth-octave steps, each rounded to K_PROBE_GRID_HZ.
    std::vector<double> window_sweep()
    {
        std::vector<double> frequencies;
        for (std::size_t step = 0;; ++step)
        {
            const double hz = K_PROBE_GRID_HZ * std::round(K_WINDOW_LOW_HZ * std::pow(2.0, static_cast<double>(step) / 6.0) / K_PROBE_GRID_HZ);
            if (hz >= K_WINDOW_HIGH_HZ)
            {
                break;
            }
            frequencies.push_back(hz);
        }
        frequencies.push_back(K_WINDOW_HIGH_HZ);
        return frequencies;
    }

    /// @brief Gain in dB of `chain` at each frequency, relative to its gain at 1 kHz.
    template <typename Filter>
    std::vector<double> response_in_db_re_1k(Filter& chain, const std::vector<double>& frequencies,
                                             double sample_rate)
    {
        const double reference = gain_at(chain, 1000.0, sample_rate);
        std::vector<double> response;
        response.reserve(frequencies.size());
        for (const double hz : frequencies)
        {
            response.push_back(20.0 * std::log10(gain_at(chain, hz, sample_rate) / reference));
        }
        return response;
    }

    /// @brief Expects every entry of a response to sit inside the published window.
    void expect_inside_window(const std::vector<double>& response, const std::vector<double>& frequencies)
    {
        for (std::size_t i = 0; i < response.size(); ++i)
        {
            EXPECT_LE(response[i], K_WINDOW_UPPER_DB) << frequencies[i] << " Hz";
            EXPECT_GE(response[i], K_WINDOW_LOWER_DB) << frequencies[i] << " Hz";
        }
    }

    /// @brief A filter chain of the anti-alias filter followed by the reconstruction filter.
    class DelayPath
    {
        public:
            void prepare(double sample_rate)
            {
                anti_alias_.prepare(sample_rate);
                reconstruction_.prepare(sample_rate);
            }

            void reset()
            {
                anti_alias_.reset();
                reconstruction_.reset();
            }

            float process_sample(float input)
            {
                return reconstruction_.process_sample(anti_alias_.process_sample(input));
            }

        private:
            SushiDSP::dd3::Dd3AntiAliasFilter anti_alias_;
            SushiDSP::dd3::Dd3ReconstructionFilter reconstruction_;
    };

    /// @brief Uniform noise in (-0.5, 0.5) from a fixed linear congruential generator.
    std::vector<float> seeded_noise(std::size_t count)
    {
        std::vector<float> noise(count);
        std::uint32_t state = 22695477u;
        for (float& sample : noise)
        {
            state = state * 1103515245u + 12345u;
            sample = 0.5f * (static_cast<float>(state >> 16) / 32768.0f - 1.0f);
        }
        return noise;
    }

    /// @brief Feeds a constant to every filter for a number of samples.
    template <typename... Filters>
    void drive_constant(float value, std::size_t count, Filters&... filters)
    {
        for (std::size_t n = 0; n < count; ++n)
        {
            (filters.process_sample(value), ...);
        }
    }

    /// @brief Expects the filter to repeat its answer to a unit step after a reset.
    template <typename Filter>
    void expect_reset_restores_the_first_answer(Filter& filter)
    {
        filter.prepare(48000.0);
        const float first = filter.process_sample(1.0f);
        drive_constant(0.7f, 500, filter);
        filter.reset();

        EXPECT_EQ(filter.process_sample(1.0f), first);
    }
} // namespace

TEST(Small_Dd3AnalogFilters, Dd3SectionConstantsMatchTheComponentValues)
{
    // f0 = 1/(2*pi*R*sqrt(C1*C2)) and Q = 0.5*sqrt(C1/C2). See tests/nodes/dd3/README.md.
    for (const SectionRow& row : K_SECTION_ROWS)
    {
        const double expected_frequency =
            1.0 / (2.0 * K_PI * SushiDSP::dd3::K_SALLEN_KEY_RESISTANCE_OHMS * std::sqrt(row.c1 * row.c2));
        const double expected_q = 0.5 * std::sqrt(row.c1 / row.c2);

        EXPECT_TRUE(within_double_roundings(row.frequency, expected_frequency, 8)) << row.name;
        EXPECT_TRUE(within_double_roundings(row.quality_factor, expected_q, 4)) << row.name;
    }
}

TEST(Small_Dd3AnalogFilters, Dd3EmphasisConstantsMatchTheComponentValues)
{
    // The zero and the pole follow from R7, R8 and C4, and the shelf gain from 33/47. See tests/nodes/dd3/README.md.
    EXPECT_TRUE(within_double_roundings(SushiDSP::dd3::K_EMPHASIS_ZERO_HZ,
                                        1.0 / (2.0 * K_PI * 57000.0 * 4.7e-9), 6));
    EXPECT_TRUE(within_double_roundings(SushiDSP::dd3::K_EMPHASIS_POLE_HZ,
                                        1.0 / (2.0 * K_PI * 10000.0 * 4.7e-9), 6));
    EXPECT_TRUE(within_double_roundings(
        SushiDSP::dd3::K_PRE_EMPHASIS_LOW_GAIN * SushiDSP::dd3::K_DE_EMPHASIS_LOW_GAIN, 1.0, 4));
    EXPECT_TRUE(within_double_roundings(SushiDSP::dd3::K_PRE_EMPHASIS_LOW_GAIN *
                                            SushiDSP::dd3::K_EMPHASIS_POLE_HZ /
                                            SushiDSP::dd3::K_EMPHASIS_ZERO_HZ,
                                        1881.0 / 470.0, 8));
}

TEST(Small_Dd3AnalogFilters, Dd3EmitterFollowerDampingLowersEveryQ)
{
    // The oracle is the equal-R Sallen-Key Q for a buffer of gain K, built from the capacitors. See tests/nodes/dd3/README.md.
    for (const SectionRow& row : K_SECTION_ROWS)
    {
        const double expected =
            1.0 / (2.0 * std::sqrt(row.c2 / row.c1) +
                   (1.0 - SushiDSP::dd3::K_EMITTER_FOLLOWER_GAIN) * std::sqrt(row.c1 / row.c2));
        const double damped = SushiDSP::dd3::emitter_follower_damped_q(row.quality_factor);

        EXPECT_TRUE(within_double_roundings(damped, expected, 12)) << row.name;
        EXPECT_LT(damped, row.quality_factor) << row.name;
    }
    // At q = 1 the formula reduces to 1/(1 + 2*(1-K)).
    EXPECT_TRUE(within_double_roundings(
        SushiDSP::dd3::emitter_follower_damped_q(1.0),
        1.0 / (1.0 + 2.0 * (1.0 - SushiDSP::dd3::K_EMITTER_FOLLOWER_GAIN)), 4));
}

TEST(Small_Dd3AnalogFilters, Dd3PrewarpIsExactAtThePassbandEdgeAndScalesEveryCornerAlike)
{
    // One prewarp reference serves the whole cascade, and the edge maps to itself. See tests/nodes/dd3/README.md.
    for (const RateRow& rate : K_PASSBAND_RATES)
    {
        const double edge = SushiDSP::dd3::K_PASSBAND_EDGE_HZ;
        EXPECT_TRUE(within_double_roundings(SushiDSP::dd3::prewarped_corner_hz(edge, rate.sample_rate),
                                            edge, 8))
            << rate.name;
        const double edge_tangent = std::tan(K_PI * edge / rate.sample_rate);
        for (const double corner : SushiDSP::dd3::K_ANTI_ALIAS_SECTION_FREQUENCIES)
        {
            const double digital = SushiDSP::dd3::prewarped_corner_hz(corner, rate.sample_rate);
            EXPECT_LT(digital, 0.5 * rate.sample_rate) << rate.name << " " << corner;
            EXPECT_TRUE(within_double_roundings(std::tan(K_PI * digital / rate.sample_rate) / edge_tangent,
                                                corner / edge, 16))
                << rate.name << " " << corner;
        }
    }
}

TEST(Small_Dd3AnalogFilters, Dd3AntiAliasCascadeMatchesItsDigitalResponse)
{
    // The exact digital response is the product of the prewarped bilinear sections. See tests/nodes/dd3/README.md.
    for (const RateRow& rate : K_PASSBAND_RATES)
    {
        SushiDSP::dd3::Dd3AntiAliasFilter filter;
        filter.prepare(rate.sample_rate);
        const double bound = rounding_bound(filter, SushiDSP::dd3::Dd3AntiAliasFilter::K_SECTION_COUNT);
        for (const ProbeRow& probe : K_BAND_PROBES)
        {
            EXPECT_TRUE(within_bound(gain_at(filter, probe.hz, rate.sample_rate),
                                     digital_anti_alias_magnitude(probe.hz, rate.sample_rate), bound))
                << rate.name << " " << probe.name;
        }
    }
}

TEST(Small_Dd3AnalogFilters, Dd3AntiAliasCascadeMatchesItsDampedPrototype)
{
    // The prewarp holds the passband close to the analogue prototype; the bound is characterized. See tests/nodes/dd3/README.md.
    for (const RateRow& rate : K_PASSBAND_RATES)
    {
        for (const ProbeRow& probe : K_BAND_PROBES)
        {
            const double analog = analog_anti_alias_magnitude(probe.hz);
            EXPECT_NEAR(digital_anti_alias_magnitude(probe.hz, rate.sample_rate), analog,
                        K_PROTOTYPE_AGREEMENT * analog)
                << rate.name << " " << probe.name;
        }
    }
}

TEST(Small_Dd3AnalogFilters, Dd3ReconstructionCascadeMatchesItsDigitalResponse)
{
    for (const RateRow& rate : K_PASSBAND_RATES)
    {
        SushiDSP::dd3::Dd3ReconstructionFilter filter;
        filter.prepare(rate.sample_rate);
        const double bound =
            rounding_bound(filter, SushiDSP::dd3::Dd3ReconstructionFilter::K_SECTION_COUNT);
        for (const ProbeRow& probe : K_BAND_PROBES)
        {
            EXPECT_TRUE(within_bound(gain_at(filter, probe.hz, rate.sample_rate),
                                     digital_reconstruction_magnitude(probe.hz, rate.sample_rate), bound))
                << rate.name << " " << probe.name;
        }
    }
}

TEST(Small_Dd3AnalogFilters, Dd3ReconstructionCascadeMatchesItsDampedPrototype)
{
    // The 4th-order filter is still rising at the band edge; the bound is characterized. See tests/nodes/dd3/README.md.
    for (const RateRow& rate : K_PASSBAND_RATES)
    {
        for (const ProbeRow& probe : K_BAND_PROBES)
        {
            const double analog = analog_reconstruction_magnitude(probe.hz);
            EXPECT_NEAR(digital_reconstruction_magnitude(probe.hz, rate.sample_rate), analog,
                        K_PROTOTYPE_AGREEMENT * analog)
                << rate.name << " " << probe.name;
        }
    }
}

TEST(Small_Dd3AnalogFilters, Dd3AntiAliasAloneMeetsThePublishedResponseWindow)
{
    // The window is +1/-3 dB of 1 kHz from 40 Hz to 7 kHz. See tests/nodes/dd3/README.md.
    const std::vector<double> frequencies = window_sweep();
    for (const RateRow& rate : K_NODE_RATES)
    {
        SushiDSP::dd3::Dd3AntiAliasFilter filter;
        filter.prepare(rate.sample_rate);

        expect_inside_window(response_in_db_re_1k(filter, frequencies, rate.sample_rate), frequencies);
    }
}

TEST(Small_Dd3AnalogFilters, Dd3DelayPathMeetsThePublishedResponseWindow)
{
    // On the board the wet signal passes through both cascades. See tests/nodes/dd3/README.md.
    const std::vector<double> frequencies = window_sweep();
    for (const RateRow& rate : K_NODE_RATES)
    {
        DelayPath path;
        path.prepare(rate.sample_rate);

        expect_inside_window(response_in_db_re_1k(path, frequencies, rate.sample_rate), frequencies);
    }
}

TEST(Small_Dd3AnalogFilters, Dd3AntiAliasCascadeRejectsAboveTheBandEdge)
{
    // 20 kHz is further down at 48 kHz than at 96 kHz. See tests/nodes/dd3/README.md.
    for (const StopbandRow& row : K_STOPBAND_ROWS)
    {
        SushiDSP::dd3::Dd3AntiAliasFilter filter;
        filter.prepare(row.sample_rate);
        const double bound = rounding_bound(filter, SushiDSP::dd3::Dd3AntiAliasFilter::K_SECTION_COUNT);

        EXPECT_TRUE(within_bound(gain_at(filter, row.hz, row.sample_rate),
                                 digital_anti_alias_magnitude(row.hz, row.sample_rate), bound))
            << row.name;
    }
}

TEST(Small_Dd3AnalogFilters, Dd3EmphasisPairIsTheIdentity)
{
    // The pair is unity by construction: 33/47 * 47/33 = 1. See tests/nodes/dd3/README.md.
    const std::vector<float> input = seeded_noise(200000);
    for (const RateRow& rate : K_NODE_RATES)
    {
        SushiDSP::dd3::Dd3PreEmphasis pre;
        SushiDSP::dd3::Dd3DeEmphasis de;
        pre.prepare(rate.sample_rate);
        de.prepare(rate.sample_rate);
        const double pre_l1 = l1_norm(pre);
        const double de_l1 = l1_norm(de);
        pre.reset();
        de.reset();
        double signal_power = 0.0;
        double error_power = 0.0;
        for (const float x : input)
        {
            const float y = de.process_sample(pre.process_sample(x));
            signal_power += static_cast<double>(x) * x;
            error_power += static_cast<double>(y - x) * (y - x);
        }
        const double peak_error = K_FLOAT_ROUNDOFF * (0.5 * pre_l1 * de_l1 + 0.5);
        const double power_ratio_bound = peak_error * peak_error / (1.0 / 12.0);

        EXPECT_LE(error_power / signal_power, power_ratio_bound) << rate.name;
    }
}

TEST(Small_Dd3AnalogFilters, Dd3PreEmphasisMatchesItsDigitalShelfAtThreeProbes)
{
    // Both corners are prewarped separately. See tests/nodes/dd3/README.md.
    SushiDSP::dd3::Dd3PreEmphasis pre;
    pre.prepare(96000.0);
    const double bound = rounding_bound(pre, 1);
    for (const ProbeRow& probe : K_EMPHASIS_PROBES)
    {
        EXPECT_TRUE(within_bound(gain_at(pre, probe.hz, 96000.0),
                                 digital_pre_emphasis_magnitude(probe.hz, 96000.0), bound))
            << probe.name;
    }
}

TEST(Small_Dd3AnalogFilters, Dd3PreEmphasisHitsItsThreeDerivedGains)
{
    // The digital shelf sits above the analogue one as the tangent stretches the probe. See tests/nodes/dd3/README.md.
    for (const ProbeRow& probe : K_EMPHASIS_PROBES)
    {
        const double larger_angle =
            K_PI * std::max(probe.hz, SushiDSP::dd3::K_EMPHASIS_POLE_HZ) / 96000.0;
        const double analog = analog_pre_emphasis_magnitude(probe.hz);

        EXPECT_NEAR(digital_pre_emphasis_magnitude(probe.hz, 96000.0), analog,
                    std::expm1(larger_angle * larger_angle / 2.0) * analog)
            << probe.name;
    }
}

TEST(Small_Dd3AnalogFilters, Dd3DeEmphasisIsTheReciprocalOfThePreEmphasis)
{
    // The de-emphasis swaps the two corners and inverts the low-frequency gain. See tests/nodes/dd3/README.md.
    SushiDSP::dd3::Dd3DeEmphasis de;
    de.prepare(96000.0);
    const double bound = rounding_bound(de, 1);
    for (const ProbeRow& probe : K_RECIPROCAL_PROBES)
    {
        EXPECT_TRUE(within_bound(gain_at(de, probe.hz, 96000.0),
                                 1.0 / digital_pre_emphasis_magnitude(probe.hz, 96000.0), bound))
            << probe.name;
    }
}

TEST(Small_Dd3AnalogFilters, Dd3FiltersResetClearsEverySection)
{
    SushiDSP::dd3::Dd3AntiAliasFilter anti_alias;
    SushiDSP::dd3::Dd3ReconstructionFilter reconstruction;
    SushiDSP::dd3::Dd3PreEmphasis pre;
    SushiDSP::dd3::Dd3DeEmphasis de;

    expect_reset_restores_the_first_answer(anti_alias);
    expect_reset_restores_the_first_answer(reconstruction);
    expect_reset_restores_the_first_answer(pre);
    expect_reset_restores_the_first_answer(de);
}
