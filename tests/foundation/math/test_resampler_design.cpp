/**************************************************************************/
/* test_resampler_design.cpp                                              */
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
 * @file test_resampler_design.cpp
 * @brief Tests for the committed resampler prototype coefficients.
 * @author Mustafa Garip & Sushi Systems
 */

#define _USE_MATH_DEFINES

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/resampler_design.hpp>

#include "resampler_kernel.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(ResamplerDesign, ResamplerKernelHasTheCommittedShape)
{
    // 34 * 512 + 1 endpoint + 1 guard = 17410. See tests/foundation/math/README.md.
    EXPECT_EQ(SushiDSP::K_RESAMPLER_KERNEL_LENGTH, 17410u);
    EXPECT_EQ(SushiDSP::K_RESAMPLER_HALF_WIDTH_ZERO_CROSSINGS *
                  SushiDSP::K_RESAMPLER_PHASES_PER_ZERO_CROSSING,
              17408u);
    // The centre tap of the kernel is 2*fc, not 1. See tests/foundation/math/README.md.
    EXPECT_NEAR(SushiDSP::K_RESAMPLER_KERNEL[0], 2.0f * SushiDSP::K_RESAMPLER_CUTOFF, 1e-3f);
    EXPECT_FLOAT_EQ(SushiDSP::K_RESAMPLER_KERNEL[17408], 0.0f);
    EXPECT_FLOAT_EQ(SushiDSP::K_RESAMPLER_KERNEL[17409], 0.0f);
    // A windowed sinc alternates in sign between zero crossings; the first sidelobe is negative.
    EXPECT_LT(SushiDSP::K_RESAMPLER_KERNEL[768], 0.0f);  // 1.5 input samples out
    EXPECT_GT(SushiDSP::K_RESAMPLER_KERNEL[1280], 0.0f); // 2.5 input samples out
}

TEST(ResamplerDesign, ResamplerKernelSumsToOnePerPhase)
{
    // Branch p is h(p/512 + k) for integer k. See tests/foundation/math/README.md.
    for (std::uint32_t phase = 0; phase < SushiDSP::K_RESAMPLER_PHASES_PER_ZERO_CROSSING; ++phase)
    {
        double sum = 0.0;
        for (std::uint32_t tap = 0; tap < SushiDSP::K_RESAMPLER_HALF_WIDTH_ZERO_CROSSINGS; ++tap)
        {
            const std::uint32_t right =
                tap * SushiDSP::K_RESAMPLER_PHASES_PER_ZERO_CROSSING + phase;
            const std::uint32_t left =
                (tap + 1) * SushiDSP::K_RESAMPLER_PHASES_PER_ZERO_CROSSING - phase;
            sum += SushiDSP::K_RESAMPLER_KERNEL[right];
            if (left < SushiDSP::K_RESAMPLER_KERNEL_LENGTH)
            {
                sum += SushiDSP::K_RESAMPLER_KERNEL[left];
            }
        }
        EXPECT_NEAR(sum, 1.0, 2.0e-4) << phase;
    }
}

TEST(ResamplerDesign, ResamplerKernelMeetsItsPassbandAndStopbandTargets)
{
    // The passband is |f| <= 0.400, the stopband >= 0.500. See tests/foundation/math/README.md.
    auto magnitude_db = [](double frequency_cycles_per_input_sample)
    {
        std::complex<double> sum(SushiDSP::K_RESAMPLER_KERNEL[0], 0.0);
        for (std::uint32_t i = 1; i < 17409u; ++i)
        {
            const double t =
                static_cast<double>(i) / SushiDSP::K_RESAMPLER_PHASES_PER_ZERO_CROSSING;
            const double phase = 2.0 * M_PI * frequency_cycles_per_input_sample * t;
            sum +=
                2.0 * SushiDSP::K_RESAMPLER_KERNEL[i] * std::complex<double>(std::cos(phase), 0.0);
        }
        // The kernel is even and real, so is its transform. See tests/foundation/math/README.md.
        const double gain = std::abs(sum) / SushiDSP::K_RESAMPLER_PHASES_PER_ZERO_CROSSING;
        return 20.0 * std::log10(std::max(gain, 1e-300));
    };

    for (const double f : {0.0, 0.05, 0.10, 0.20, 0.30, 0.35, 0.399})
    {
        EXPECT_NEAR(magnitude_db(f), 0.0, 0.10) << f;
    }
    for (const double f : {0.50, 0.60, 0.80, 1.00, 1.50, 2.00, 4.00})
    {
        EXPECT_LE(magnitude_db(f), -100.0) << f;
    }
}

TEST(ResamplerDesign, ResamplerKernelIsSmoothEnoughForLinearInterpolation)
{
    // The predicted interpolation error bound is 3.8127e-6. See tests/foundation/math/README.md.
    double worst = 0.0;
    for (std::uint32_t i = 1; i + 1 < 17409u; ++i)
    {
        const double second_difference = static_cast<double>(SushiDSP::K_RESAMPLER_KERNEL[i + 1]) -
                                         2.0 * SushiDSP::K_RESAMPLER_KERNEL[i] +
                                         SushiDSP::K_RESAMPLER_KERNEL[i - 1];
        worst = std::max(worst, std::abs(second_difference) / 8.0);
    }
    std::printf("[ measure ] resampler kernel linear-interpolation bound %.4e (%.2f dB)\n", worst,
                20.0 * std::log10(worst));
    EXPECT_LE(worst, 4.0e-6);
}
