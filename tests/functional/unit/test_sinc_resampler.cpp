/**************************************************************************/
/* test_sinc_resampler.cpp                                                */
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
 * @file test_sinc_resampler.cpp
 * @brief Tests that SincResampler preserves a tone and low-passes when downsampling.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <cmath>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/SincResampler.hpp"

TEST(Unit_Dsp, SincResamplerPreservesAOneKilohertzToneWithinMinusEightyDb)
{
    std::vector<float> in(44100);
    for (std::size_t i = 0; i < in.size(); ++i)
    {
        in[i] = static_cast<float>(std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 44100.0));
    }

    const auto out = SushiDSP::SincResampler::resample(in, 44100.0, 48000.0);
    ASSERT_NEAR(static_cast<double>(out.size()), 48000.0, 2.0);

    double error  = 0.0;
    double energy = 0.0;
    for (std::size_t i = 1000; i + 1000 < out.size(); ++i)
    {
        const double expected = std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
        error += (out[i] - expected) * (out[i] - expected);
        energy += expected * expected;
    }

    EXPECT_LT(10.0 * std::log10(error / energy), -80.0);
}

TEST(Unit_Dsp, SincResamplerLowpassesAboveTheNewNyquistWhenDownsampling)
{
    const auto tone_energy_after_halving_the_rate = [](double frequency_hz)
    {
        std::vector<float> in(96000);
        for (std::size_t i = 0; i < in.size(); ++i)
        {
            in[i] = static_cast<float>(std::sin(2.0 * M_PI * frequency_hz * static_cast<double>(i) / 96000.0));
        }

        const auto out = SushiDSP::SincResampler::resample(in, 96000.0, 48000.0);

        double energy = 0.0;
        std::size_t count = 0;
        for (std::size_t i = 1000; i + 1000 < out.size(); ++i)
        {
            energy += static_cast<double>(out[i]) * out[i];
            ++count;
        }
        return energy / static_cast<double>(count);
    };

    const double passband_energy = tone_energy_after_halving_the_rate(1000.0);
    const double aliased_energy  = tone_energy_after_halving_the_rate(30000.0);

    EXPECT_LT(10.0 * std::log10(aliased_energy / passband_energy), -60.0);
}
