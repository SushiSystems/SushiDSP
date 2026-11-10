/**************************************************************************/
/* test_output_transformer_stage.cpp                                      */
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
 * @file test_output_transformer_stage.cpp
 * @brief Tests the DC blocking, saturation and reset of OutputTransformerStage.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/stages/output_transformer_stage.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr float K_ROLLOFF_HZ = 60.0f;
    constexpr float K_LEAKAGE_HZ = 8000.0f;
    constexpr float K_DRIVE = 0.3f;

    /// @brief Holds the bound on the mean of the secondary over the second second of a clipped square: a sixtieth of the -0.0635 the stage read with the mean left in (stages/README.md), which the stage with the mean removed beats by four orders of magnitude (+0.000005).
    constexpr double K_MEAN_BOUND = 1e-3;

    const double kFloatU = TestSupport::unit_roundoff<float>();

    SushiDSP::OutputTransformerStage make_stage()
    {
        return SushiDSP::OutputTransformerStage(K_SAMPLE_RATE, K_ROLLOFF_HZ, K_LEAKAGE_HZ, K_DRIVE);
    }

    /// @brief Returns the stage's output for every sample of the input.
    std::vector<float> run(SushiDSP::OutputTransformerStage& stage, const std::vector<float>& input)
    {
        std::vector<float> output(input.size());
        std::transform(input.begin(), input.end(), output.begin(), [&](float x) { return stage.process_sample(x); });
        return output;
    }

    /// @brief Returns a 220 Hz square of amplitude 2.2 at 40 % duty, @p count samples at 48 kHz.
    std::vector<float> clipped_square(std::size_t count)
    {
        std::vector<float> input(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            input[i] = std::fmod(220.0 * static_cast<double>(i) / K_SAMPLE_RATE, 1.0) < 0.4 ? 2.2f : -2.2f;
        }
        return input;
    }

    /// @brief Returns a 200 Hz sine of amplitude 50, @p count samples at 48 kHz.
    std::vector<float> loud_sine(std::size_t count)
    {
        std::vector<float> input(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            input[i] = 50.0f * static_cast<float>(std::sin(2.0 * M_PI * 200.0 * static_cast<double>(i) / K_SAMPLE_RATE));
        }
        return input;
    }

    bool all_finite(const std::vector<float>& values)
    {
        return std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); });
    }

    double mean_of_tail(const std::vector<float>& values, std::size_t from)
    {
        return std::accumulate(values.begin() + static_cast<std::ptrdiff_t>(from), values.end(), 0.0) /
               static_cast<double>(values.size() - from);
    }

    float peak_of_tail(const std::vector<float>& values, std::size_t from)
    {
        return std::accumulate(values.begin() + static_cast<std::ptrdiff_t>(from), values.end(), 0.0f,
                               [](float peak, float value) { return std::max(peak, std::fabs(value)); });
    }
} // namespace

// The rolloff is a second-order Butterworth high pass whose poles decay by exp(-w0 n / sqrt 2) per sample count n, w0 = 2 pi fc / fs; its step response peaks below 2, so after 10000 samples the output is under 2 exp(-w0 n / sqrt 2) plus a few roundings of the float state.
TEST(Small_OutputTransformerStage, OutputTransformerStageBlocksDc)
{
    SushiDSP::OutputTransformerStage stage = make_stage();
    const std::vector<float> output = run(stage, std::vector<float>(10000, 1.0f));
    const double w0 = 2.0 * M_PI * K_ROLLOFF_HZ / K_SAMPLE_RATE;
    const double bound = 2.0 * std::exp(-w0 * 10000.0 / std::sqrt(2.0)) + 8.0 * kFloatU;

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(output.back(), 0.0, bound)));
}

TEST(Small_OutputTransformerStage, OutputTransformerStageSecondaryCarriesNoMean)
{
    // A transformer's secondary passes no mean. See tests/foundation/circuits/stages/README.md.
    SushiDSP::OutputTransformerStage stage = make_stage();
    const std::vector<float> output = run(stage, clipped_square(96000));

    EXPECT_TRUE(all_finite(output));
    EXPECT_LT(std::fabs(mean_of_tail(output, 48000)), K_MEAN_BOUND);
}

TEST(Small_OutputTransformerStage, OutputTransformerStageKeepsLargeSignalsFinite)
{
    SushiDSP::OutputTransformerStage stage = make_stage();

    EXPECT_TRUE(all_finite(run(stage, loud_sine(4000))));
}

// The saturated sample is tanh(x d) / d, under 1 / d in magnitude, and the tracked mean is a running average of such samples, so the output stays under 2 / d; the float cast adds one rounding.
TEST(Small_OutputTransformerStage, OutputTransformerStageSaturatesLargeSignalsInsideTwiceItsDriveLimit)
{
    SushiDSP::OutputTransformerStage stage = make_stage();
    const std::vector<float> output = run(stage, loud_sine(4000));
    const double limit = 2.0 / K_DRIVE * (1.0 + 2.0 * kFloatU);

    EXPECT_LE(peak_of_tail(output, 3001), limit);
}

// The reset state is the constructed state and the arithmetic is deterministic, so the first sample after a reset equals a fresh stage's first sample bit for bit.
TEST(Small_OutputTransformerStage, OutputTransformerStageResetClearsFilterState)
{
    SushiDSP::OutputTransformerStage used = make_stage();
    SushiDSP::OutputTransformerStage fresh = make_stage();

    used.process_sample(1.0f);
    used.reset();

    EXPECT_EQ(used.process_sample(0.2f), fresh.process_sample(0.2f));
}
