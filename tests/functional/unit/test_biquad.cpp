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

#include <cmath>

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
