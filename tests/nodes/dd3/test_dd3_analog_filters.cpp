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
 * @file test_dd3_analog_filters.cpp
 * @brief Tests for the DD-3 anti-alias, reconstruction and emphasis filters.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/dd3/dd3_analog_filters.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    /// @brief Peak output of a chain driven by a unit sine, after it has settled.
    /// @param process Callable taking one float and returning one float.
    /// @param frequency_hz Probe frequency.
    /// @param sample_rate Sample rate the chain was prepared at.
    /// @return Peak amplitude, which is |H| for a unit input.
    template <typename Process>
    double chain_magnitude(Process&& process, double frequency_hz, double sample_rate)
    {
        const std::size_t settle = static_cast<std::size_t>(0.1 * sample_rate);
        const std::size_t measure =
            static_cast<std::size_t>(std::round(20.0 * sample_rate / frequency_hz));
        double peak = 0.0;
        for (std::size_t n = 0; n < settle + measure; ++n)
        {
            const float y = process(static_cast<float>(
                std::sin(2.0 * M_PI * frequency_hz * static_cast<double>(n) / sample_rate)));
            if (n >= settle)
            {
                peak = std::max(peak, std::abs(static_cast<double>(y)));
            }
        }
        return peak;
    }

    /// @brief Magnitude of one Sallen-Key section with its buffer damping applied.
    /// @param corner_hz The section's f0.
    /// @param unity_gain_q The section's Q for an ideal buffer.
    /// @param frequency_hz Probe frequency.
    /// @return |H| of the second-order low-pass.
    double section_magnitude(double corner_hz, double unity_gain_q, double frequency_hz)
    {
        const double q = SushiDSP::dd3::emitter_follower_damped_q(unity_gain_q);
        const double x = frequency_hz / corner_hz;
        return 1.0 / std::sqrt((1.0 - x * x) * (1.0 - x * x) + (x / q) * (x / q));
    }

    /// @brief Magnitude of the damped 6th-order anti-alias prototype.
    /// @param frequency_hz Probe frequency.
    /// @return |H| of the three sections in series.
    double anti_alias_prototype(double frequency_hz)
    {
        double magnitude = 1.0;
        for (std::size_t i = 0; i < SushiDSP::dd3::Dd3AntiAliasFilter::K_SECTION_COUNT; ++i)
        {
            magnitude *= section_magnitude(SushiDSP::dd3::K_ANTI_ALIAS_SECTION_FREQUENCIES[i],
                                           SushiDSP::dd3::K_ANTI_ALIAS_SECTION_Q[i], frequency_hz);
        }
        return magnitude;
    }

    /// @brief Magnitude of the damped 4th-order reconstruction prototype.
    /// @param frequency_hz Probe frequency.
    /// @return |H| of the two sections in series.
    double reconstruction_prototype(double frequency_hz)
    {
        double magnitude = 1.0;
        for (std::size_t i = 0; i < SushiDSP::dd3::Dd3ReconstructionFilter::K_SECTION_COUNT; ++i)
        {
            magnitude *=
                section_magnitude(SushiDSP::dd3::K_RECONSTRUCTION_SECTION_FREQUENCIES[i],
                                  SushiDSP::dd3::K_RECONSTRUCTION_SECTION_Q[i], frequency_hz);
        }
        return magnitude;
    }

    /// @brief Magnitude of the analogue pre-emphasis shelf, C5 dropped.
    /// @param frequency_hz Probe frequency.
    /// @return |H| of the zero/pole pair scaled by R12/R7.
    double pre_emphasis_prototype(double frequency_hz)
    {
        const double zero_ratio = frequency_hz / SushiDSP::dd3::K_EMPHASIS_ZERO_HZ;
        const double pole_ratio = frequency_hz / SushiDSP::dd3::K_EMPHASIS_POLE_HZ;
        return SushiDSP::dd3::K_PRE_EMPHASIS_LOW_GAIN *
               std::sqrt((1.0 + zero_ratio * zero_ratio) / (1.0 + pole_ratio * pole_ratio));
    }
} // namespace

TEST(Small_Dd3AnalogFilters, Dd3SectionConstantsMatchTheComponentValues)
{
    // f0 = 1/(2*pi*R*sqrt(C1*C2)) and Q = 0.5*sqrt(C1/C2). See tests/nodes/dd3/README.md.
    struct Section
    {
            double c1;
            double c2;
    };
    const Section sections[5] = {
        {4.7e-9, 2.7e-9}, {6.8e-9, 4.7e-10}, {1.8e-8, 2.2e-10}, {2.7e-9, 1.2e-9}, {6.8e-9, 1.5e-10},
    };
    double frequencies[5] = {0.0};
    double quality_factors[5] = {0.0};
    for (std::size_t i = 0; i < 5; ++i)
    {
        frequencies[i] = 1.0 / (2.0 * M_PI * SushiDSP::dd3::K_SALLEN_KEY_RESISTANCE_OHMS *
                                std::sqrt(sections[i].c1 * sections[i].c2));
        quality_factors[i] = 0.5 * std::sqrt(sections[i].c1 / sections[i].c2);
    }
    for (std::size_t i = 0; i < 3; ++i)
    {
        EXPECT_NEAR(SushiDSP::dd3::K_ANTI_ALIAS_SECTION_FREQUENCIES[i], frequencies[i], 1e-6) << i;
        EXPECT_NEAR(SushiDSP::dd3::K_ANTI_ALIAS_SECTION_Q[i], quality_factors[i], 1e-12) << i;
    }
    for (std::size_t i = 0; i < 2; ++i)
    {
        EXPECT_NEAR(SushiDSP::dd3::K_RECONSTRUCTION_SECTION_FREQUENCIES[i], frequencies[3 + i],
                    1e-6)
            << i;
        EXPECT_NEAR(SushiDSP::dd3::K_RECONSTRUCTION_SECTION_Q[i], quality_factors[3 + i], 1e-12)
            << i;
    }
    EXPECT_NEAR(SushiDSP::dd3::K_EMPHASIS_ZERO_HZ, 1.0 / (2.0 * M_PI * 57000.0 * 4.7e-9), 1e-9);
    EXPECT_NEAR(SushiDSP::dd3::K_EMPHASIS_POLE_HZ, 1.0 / (2.0 * M_PI * 10000.0 * 4.7e-9), 1e-9);
    EXPECT_NEAR(SushiDSP::dd3::K_PRE_EMPHASIS_LOW_GAIN * SushiDSP::dd3::K_DE_EMPHASIS_LOW_GAIN, 1.0,
                1e-15);
}

TEST(Small_Dd3AnalogFilters, Dd3EmitterFollowerDampingLowersEveryQ)
{
    // Q = 1/(1/q + 2*(1-K)*q) for a buffer of gain K. See tests/nodes/dd3/README.md.
    const double damped_anti_alias[3] = {0.6347216596, 1.4333063892, 1.5876726071};
    const double damped_reconstruction[2] = {0.7137168140, 1.6630691464};
    for (std::size_t i = 0; i < 3; ++i)
    {
        const double q =
            SushiDSP::dd3::emitter_follower_damped_q(SushiDSP::dd3::K_ANTI_ALIAS_SECTION_Q[i]);
        EXPECT_NEAR(q, damped_anti_alias[i], 1e-9) << i;
        EXPECT_LT(q, SushiDSP::dd3::K_ANTI_ALIAS_SECTION_Q[i]) << i;
    }
    for (std::size_t i = 0; i < 2; ++i)
    {
        const double q =
            SushiDSP::dd3::emitter_follower_damped_q(SushiDSP::dd3::K_RECONSTRUCTION_SECTION_Q[i]);
        EXPECT_NEAR(q, damped_reconstruction[i], 1e-9) << i;
        EXPECT_LT(q, SushiDSP::dd3::K_RECONSTRUCTION_SECTION_Q[i]) << i;
    }
    EXPECT_DOUBLE_EQ(SushiDSP::dd3::emitter_follower_damped_q(1.0),
                     1.0 / (1.0 + 2.0 * (1.0 - SushiDSP::dd3::K_EMITTER_FOLLOWER_GAIN)));
}

TEST(Small_Dd3AnalogFilters, Dd3PrewarpIsExactAtThePassbandEdgeAndScalesEveryCornerAlike)
{
    // One prewarp reference serves the whole cascade. See tests/nodes/dd3/README.md.
    for (const double sample_rate : {48000.0, 96000.0, 192000.0})
    {
        EXPECT_NEAR(
            SushiDSP::dd3::prewarped_corner_hz(SushiDSP::dd3::K_PASSBAND_EDGE_HZ, sample_rate),
            SushiDSP::dd3::K_PASSBAND_EDGE_HZ, 1e-9)
            << sample_rate;
        const double edge_tangent =
            std::tan(M_PI * SushiDSP::dd3::K_PASSBAND_EDGE_HZ / sample_rate);
        for (const double corner : SushiDSP::dd3::K_ANTI_ALIAS_SECTION_FREQUENCIES)
        {
            const double digital = SushiDSP::dd3::prewarped_corner_hz(corner, sample_rate);
            EXPECT_LT(digital, 0.5 * sample_rate) << corner;
            EXPECT_NEAR(std::tan(M_PI * digital / sample_rate) / edge_tangent,
                        corner / SushiDSP::dd3::K_PASSBAND_EDGE_HZ, 1e-12)
                << corner;
        }
    }
}

TEST(Small_Dd3AnalogFilters, Dd3AntiAliasCascadeMatchesItsDampedPrototype)
{
    // The prewarp holds the passband within 0.4 %. See tests/nodes/dd3/README.md.
    for (const double sample_rate : {48000.0, 96000.0, 192000.0})
    {
        SushiDSP::dd3::Dd3AntiAliasFilter filter;
        filter.prepare(sample_rate);
        for (const double frequency : {1000.0, 2000.0, 5000.0, 7000.0})
        {
            filter.reset();
            const double measured = chain_magnitude(
                [&](float x)
                {
                    return filter.process_sample(x);
                },
                frequency, sample_rate);
            EXPECT_NEAR(measured, anti_alias_prototype(frequency),
                        0.015 * anti_alias_prototype(frequency))
                << sample_rate << " Hz rate, " << frequency << " Hz";
        }
    }
    // 1.009006 at 1 kHz, 1.025214 at 2 kHz, 0.981280 at 5 kHz, 0.871989 at 7 kHz.
    EXPECT_NEAR(anti_alias_prototype(1000.0), 1.009006, 1e-5);
    EXPECT_NEAR(anti_alias_prototype(7000.0), 0.871989, 1e-5);
}

TEST(Small_Dd3AnalogFilters, Dd3ReconstructionCascadeMatchesItsDampedPrototype)
{
    for (const double sample_rate : {48000.0, 96000.0, 192000.0})
    {
        SushiDSP::dd3::Dd3ReconstructionFilter filter;
        filter.prepare(sample_rate);
        for (const double frequency : {1000.0, 2000.0, 5000.0, 7000.0})
        {
            filter.reset();
            const double measured = chain_magnitude(
                [&](float x)
                {
                    return filter.process_sample(x);
                },
                frequency, sample_rate);
            EXPECT_NEAR(measured, reconstruction_prototype(frequency),
                        0.015 * reconstruction_prototype(frequency))
                << sample_rate << " Hz rate, " << frequency << " Hz";
        }
    }
    // The 4th-order filter is still rising at the band edge. See tests/nodes/dd3/README.md.
    EXPECT_NEAR(reconstruction_prototype(1000.0), 1.003462, 1e-5);
    EXPECT_NEAR(reconstruction_prototype(7000.0), 1.010033, 1e-5);
}

TEST(Small_Dd3AnalogFilters, Dd3DelayPathMeetsThePublishedResponseWindow)
{
    // The window is +1/-3 dB of 1 kHz from 40 Hz to 7 kHz. See tests/nodes/dd3/README.md.
    for (const double sample_rate : {48000.0, 96000.0})
    {
        SushiDSP::dd3::Dd3AntiAliasFilter anti_alias;
        SushiDSP::dd3::Dd3ReconstructionFilter reconstruction;
        anti_alias.prepare(sample_rate);
        reconstruction.prepare(sample_rate);

        const auto alone = [&](float x)
        {
            return anti_alias.process_sample(x);
        };
        const auto both = [&](float x)
        {
            return reconstruction.process_sample(anti_alias.process_sample(x));
        };

        anti_alias.reset();
        const double alone_reference = chain_magnitude(alone, 1000.0, sample_rate);
        anti_alias.reset();
        reconstruction.reset();
        const double both_reference = chain_magnitude(both, 1000.0, sample_rate);

        double frequency = 40.0;
        while (frequency < 7000.0)
        {
            anti_alias.reset();
            const double alone_db =
                20.0 * std::log10(chain_magnitude(alone, frequency, sample_rate) / alone_reference);
            anti_alias.reset();
            reconstruction.reset();
            const double both_db =
                20.0 * std::log10(chain_magnitude(both, frequency, sample_rate) / both_reference);
            EXPECT_LE(alone_db, 1.0) << sample_rate << " Hz rate, " << frequency << " Hz";
            EXPECT_GE(alone_db, -3.0) << sample_rate << " Hz rate, " << frequency << " Hz";
            EXPECT_LE(both_db, 1.0) << sample_rate << " Hz rate, " << frequency << " Hz";
            EXPECT_GE(both_db, -3.0) << sample_rate << " Hz rate, " << frequency << " Hz";
            frequency *= std::pow(2.0, 1.0 / 6.0);
        }

        anti_alias.reset();
        const double alone_edge =
            20.0 * std::log10(chain_magnitude(alone, 7000.0, sample_rate) / alone_reference);
        anti_alias.reset();
        reconstruction.reset();
        const double both_edge =
            20.0 * std::log10(chain_magnitude(both, 7000.0, sample_rate) / both_reference);
        std::printf("[ measure ] DD-3 at %.0f Hz rate, 7 kHz: anti-alias %.3f dB, "
                    "delay path %.3f dB\n",
                    sample_rate, alone_edge, both_edge);
        EXPECT_GE(alone_edge, -3.0) << sample_rate;
        EXPECT_GE(both_edge, -3.0) << sample_rate;
        EXPECT_LE(alone_edge, 1.0) << sample_rate;
        EXPECT_LE(both_edge, 1.0) << sample_rate;
    }
}

TEST(Small_Dd3AnalogFilters, Dd3AntiAliasCascadeRejectsAboveTheBandEdge)
{
    // 20 kHz is further down at 48 kHz than at 96 kHz. See tests/nodes/dd3/README.md.
    SushiDSP::dd3::Dd3AntiAliasFilter filter;
    const auto run = [&](double frequency, double sample_rate)
    {
        filter.reset();
        return chain_magnitude(
            [&](float x)
            {
                return filter.process_sample(x);
            },
            frequency, sample_rate);
    };

    // The thresholds sit just above the measured readings. See tests/nodes/dd3/README.md.
    filter.prepare(48000.0);
    EXPECT_LT(run(10000.0, 48000.0), 0.15);
    EXPECT_LT(run(20000.0, 48000.0), 1e-4);

    filter.prepare(96000.0);
    EXPECT_LT(run(10000.0, 96000.0), 0.23);
    EXPECT_LT(run(20000.0, 96000.0), 2e-3);
}

TEST(Small_Dd3AnalogFilters, Dd3EmphasisPairIsTheIdentity)
{
    // The pair is unity by construction: 33/47 * 47/33 = 1. See tests/nodes/dd3/README.md.
    for (const double sample_rate : {48000.0, 96000.0})
    {
        SushiDSP::dd3::Dd3PreEmphasis pre;
        SushiDSP::dd3::Dd3DeEmphasis de;
        pre.prepare(sample_rate);
        de.prepare(sample_rate);
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
        EXPECT_LE(10.0 * std::log10(error_power / signal_power), -120.0) << sample_rate;
    }
}

TEST(Small_Dd3AnalogFilters, Dd3PreEmphasisHitsItsThreeDerivedGains)
{
    // The probe stays at fs/16; fs/4 hides both errors. See tests/nodes/dd3/README.md.
    SushiDSP::dd3::Dd3PreEmphasis pre;
    pre.prepare(96000.0);
    const auto run = [&](double frequency)
    {
        pre.reset();
        return chain_magnitude(
            [&](float x)
            {
                return pre.process_sample(x);
            },
            frequency, 96000.0);
    };
    EXPECT_NEAR(run(20.0), 0.7025132, 2e-3);
    EXPECT_NEAR(run(1000.0), 1.3184111, 3e-3);
    EXPECT_NEAR(run(6000.0), 3.5023974, 3e-2);
    EXPECT_NEAR(pre_emphasis_prototype(20.0), 0.7025132, 1e-6);
    EXPECT_NEAR(pre_emphasis_prototype(1000.0), 1.3184111, 1e-6);
    EXPECT_NEAR(SushiDSP::dd3::K_PRE_EMPHASIS_LOW_GAIN * 5.7, 4.0021277, 1e-6);
}

TEST(Small_Dd3AnalogFilters, Dd3DeEmphasisIsTheReciprocalOfThePreEmphasis)
{
    // The tolerance is the bilinear warp, not the estimator. See tests/nodes/dd3/README.md.
    SushiDSP::dd3::Dd3DeEmphasis de;
    de.prepare(96000.0);
    for (const double frequency : {20.0, 1000.0, 5000.0})
    {
        de.reset();
        const double measured = chain_magnitude(
            [&](float x)
            {
                return de.process_sample(x);
            },
            frequency, 96000.0);
        EXPECT_NEAR(measured, 1.0 / pre_emphasis_prototype(frequency), 4e-3) << frequency;
    }
}

TEST(Small_Dd3AnalogFilters, Dd3FiltersResetClearsEverySection)
{
    SushiDSP::dd3::Dd3AntiAliasFilter anti_alias;
    SushiDSP::dd3::Dd3ReconstructionFilter reconstruction;
    SushiDSP::dd3::Dd3PreEmphasis pre;
    SushiDSP::dd3::Dd3DeEmphasis de;
    anti_alias.prepare(48000.0);
    reconstruction.prepare(48000.0);
    pre.prepare(48000.0);
    de.prepare(48000.0);

    const float first_anti_alias = anti_alias.process_sample(1.0f);
    const float first_reconstruction = reconstruction.process_sample(1.0f);
    const float first_pre = pre.process_sample(1.0f);
    const float first_de = de.process_sample(1.0f);
    for (int i = 0; i < 500; ++i)
    {
        anti_alias.process_sample(0.7f);
        reconstruction.process_sample(0.7f);
        pre.process_sample(0.7f);
        de.process_sample(0.7f);
    }
    anti_alias.reset();
    reconstruction.reset();
    pre.reset();
    de.reset();

    EXPECT_FLOAT_EQ(anti_alias.process_sample(1.0f), first_anti_alias);
    EXPECT_FLOAT_EQ(reconstruction.process_sample(1.0f), first_reconstruction);
    EXPECT_FLOAT_EQ(pre.process_sample(1.0f), first_pre);
    EXPECT_FLOAT_EQ(de.process_sample(1.0f), first_de);
}
