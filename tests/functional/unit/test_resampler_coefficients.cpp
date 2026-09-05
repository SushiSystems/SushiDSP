/**************************************************************************/
/* test_resampler_coefficients.cpp                                        */
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
 * @file test_resampler_coefficients.cpp
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

#include "SushiDSP/math/ResamplerCoefficients.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Unit_Dsp, ResamplerKernelHasTheCommittedShape)
{
    // 34 zero crossings * 512 entries + 1 endpoint + 1 guard = 17410  (DR1+DR2 plan, Task 1.1
    // controller ruling: HALF_WIDTH grew from the planned 32 to clear the -100 dB stopband
    // target with margin).
    EXPECT_EQ(SushiDSP::kResamplerKernelLength, 17410u);
    EXPECT_EQ(SushiDSP::kResamplerHalfWidthZeroCrossings * SushiDSP::kResamplerPhasesPerZeroCrossing,
              17408u);
    // The centre tap of a sub-Nyquist lowpass impulse response is 2*fc, not 1: at t = 0 the
    // ideal kernel is 2*cutoff*sinc(0) = 2*cutoff, and the polyphase-DC-sum normalisation
    // (which targets the branch sums, already ~1 by Poisson summation) barely moves it.
    EXPECT_NEAR(SushiDSP::kResamplerKernel[0], 2.0f * SushiDSP::kResamplerCutoff, 1e-3f);
    EXPECT_FLOAT_EQ(SushiDSP::kResamplerKernel[17408], 0.0f);
    EXPECT_FLOAT_EQ(SushiDSP::kResamplerKernel[17409], 0.0f);
    // A windowed sinc alternates in sign between zero crossings; the first sidelobe is negative.
    EXPECT_LT(SushiDSP::kResamplerKernel[768], 0.0f);   // 1.5 input samples out
    EXPECT_GT(SushiDSP::kResamplerKernel[1280], 0.0f);  // 2.5 input samples out
}

TEST(Unit_Dsp, ResamplerKernelSumsToOnePerPhase)
{
    // Every polyphase branch must have unity DC gain, or a rate conversion would change the
    // level of a constant. Branch p is h(p/512 + k) for integer k, over both wings.
    for (std::uint32_t phase = 0; phase < SushiDSP::kResamplerPhasesPerZeroCrossing; ++phase)
    {
        double sum = 0.0;
        for (std::uint32_t tap = 0; tap < SushiDSP::kResamplerHalfWidthZeroCrossings; ++tap)
        {
            const std::uint32_t right = tap * SushiDSP::kResamplerPhasesPerZeroCrossing + phase;
            const std::uint32_t left  = (tap + 1) * SushiDSP::kResamplerPhasesPerZeroCrossing - phase;
            sum += SushiDSP::kResamplerKernel[right];
            if (left < SushiDSP::kResamplerKernelLength)
            {
                sum += SushiDSP::kResamplerKernel[left];
            }
        }
        EXPECT_NEAR(sum, 1.0, 2.0e-4) << phase;
    }
}

TEST(Unit_Dsp, ResamplerKernelMeetsItsPassbandAndStopbandTargets)
{
    // The full symmetric kernel, evaluated as a filter running at 512x the input rate. Its
    // passband is |f| <= 0.400 input-rate cycles/sample and its stopband |f| >= 0.500.
    // DFT directly at the frequencies of interest - the full kernel is too many taps for a
    // naive FFT here, and only a handful of frequencies matter.
    auto magnitude_db = [](double frequency_cycles_per_input_sample)
    {
        std::complex<double> sum(SushiDSP::kResamplerKernel[0], 0.0);
        for (std::uint32_t i = 1; i < 17409u; ++i)
        {
            const double t = static_cast<double>(i) / SushiDSP::kResamplerPhasesPerZeroCrossing;
            const double phase = 2.0 * M_PI * frequency_cycles_per_input_sample * t;
            sum += 2.0 * SushiDSP::kResamplerKernel[i] * std::complex<double>(std::cos(phase), 0.0);
        }
        // The kernel is even and real, so the transform is real; the 1/512 factor turns the sum
        // into the continuous-time integral the interpolating filter actually applies.
        const double gain = std::abs(sum) / SushiDSP::kResamplerPhasesPerZeroCrossing;
        return 20.0 * std::log10(std::max(gain, 1e-300));
    };

    for (const double f : { 0.0, 0.05, 0.10, 0.20, 0.30, 0.35, 0.399 })
    {
        EXPECT_NEAR(magnitude_db(f), 0.0, 0.10) << f;
    }
    for (const double f : { 0.50, 0.60, 0.80, 1.00, 1.50, 2.00, 4.00 })
    {
        EXPECT_LE(magnitude_db(f), -100.0) << f;
    }
}

TEST(Unit_Dsp, ResamplerKernelIsSmoothEnoughForLinearInterpolation)
{
    // N1: the linear-interpolation error is bounded by |h''| * step^2 / 8, and the committed
    // table's second difference already carries the step^2 factor. The predicted bound is
    // 3.8127e-6 (-108.4 dB); assert it against 4.0e-6 so a table designed at half the density
    // fails here rather than quietly setting the noise floor.
    double worst = 0.0;
    for (std::uint32_t i = 1; i + 1 < 17409u; ++i)
    {
        const double second_difference = static_cast<double>(SushiDSP::kResamplerKernel[i + 1])
                                       - 2.0 * SushiDSP::kResamplerKernel[i]
                                       + SushiDSP::kResamplerKernel[i - 1];
        worst = std::max(worst, std::abs(second_difference) / 8.0);
    }
    std::printf("[ measure ] resampler kernel linear-interpolation bound %.4e (%.2f dB)\n",
                worst, 20.0 * std::log10(worst));
    EXPECT_LE(worst, 4.0e-6);
}
