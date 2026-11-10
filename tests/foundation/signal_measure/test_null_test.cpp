/**************************************************************************/
/* test_null_test.cpp                                                     */
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
 * @file test_null_test.cpp
 * @brief Tests that the null test aligns delay, gain and polarity, and reports a real difference.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/signal_measure/null_test.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "support/tolerance.hpp"

namespace
{
    /// @brief Pi to double precision, spelled out so the tests do not depend on M_PI.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief The rate of the tonal signals.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Amplitude of the tone the difference test compares against.
    constexpr double K_SIGNAL_AMPLITUDE = 0.5;

    /// @brief Amplitude of the tone the difference test adds.
    constexpr double K_ERROR_AMPLITUDE = 0.005;

    /// @brief A deterministic pseudo-random sequence in [-1, 1], so the test has no seed to vary.
    std::vector<float> pseudo_random(std::size_t length)
    {
        std::vector<float> values(length);
        std::uint32_t state = 12345u;
        for (std::size_t i = 0; i < length; ++i)
        {
            state = state * 1664525u + 1013904223u;
            values[i] = static_cast<float>(state) / 2147483648.0f - 1.0f;
        }
        return values;
    }

    /// @brief A 440 Hz tone of one second.
    std::vector<float> tone()
    {
        return SushiDSP::measure::sine_stimulus(440.0, K_SAMPLE_RATE, 48000, static_cast<float>(K_SIGNAL_AMPLITUDE));
    }

    /// @brief Returns a copy of a, delayed by delay samples and scaled by gain.
    std::vector<float> delayed_copy(const std::vector<float>& a, std::size_t delay, float gain)
    {
        std::vector<float> b(a.size(), 0.0f);
        for (std::size_t i = delay; i < b.size(); ++i)
        {
            b[i] = gain * a[i - delay];
        }
        return b;
    }

    /// @brief One delayed, scaled copy of a signal, and the level it must null below.
    struct CopyRow
    {
            const char* name;
            std::vector<float> (*signal)();
            std::size_t delay;
            float gain;
            std::size_t maximum_lag;
            double level_db;
    };

    // The noise copy keeps the bound of the test it came from; the tone copies null to float rounding.
    const CopyRow kCopyRows[] = {
        {"NoiseAtDelayThree", [] { return pseudo_random(4096); }, 3u, 1.0f, 5u, -60.0},
        {"ToneAtQuarterGain", tone, 37u, 0.25f, 64u, -120.0},
        // The lag search matches on correlation magnitude. See tests/foundation/signal_measure/README.md.
        {"ToneAtInvertedQuarterGain", tone, 37u, -0.25f, 64u, -120.0},
    };

    std::string copy_name(const ::testing::TestParamInfo<CopyRow>& info)
    {
        return info.param.name;
    }

    class Small_NullTestAlignment : public ::testing::TestWithParam<CopyRow>
    {
    };

    /// @brief The residual of the added tone against the tone: two tones of whole cycles in one
    /// second are orthogonal, so it is the amplitude ratio, 20 log10(0.005 / 0.5) = -40 dB.
    double expected_difference_db()
    {
        return 20.0 * std::log10(K_ERROR_AMPLITUDE / K_SIGNAL_AMPLITUDE);
    }

    /// @brief How far float rounding moves that residual: the added tone's relative error is
    /// u * 0.5 / 0.005, which in decibels is 20 log10(1 + that).
    double rounding_bound_db()
    {
        return 20.0 * std::log10(1.0 + TestSupport::unit_roundoff<float>() * K_SIGNAL_AMPLITUDE / K_ERROR_AMPLITUDE);
    }

    /// @brief Returns the tone with a second tone of K_ERROR_AMPLITUDE at 3000 Hz added.
    std::vector<float> tone_with_an_added_tone()
    {
        std::vector<float> b = tone();
        for (std::size_t i = 0; i < b.size(); ++i)
        {
            b[i] += static_cast<float>(K_ERROR_AMPLITUDE) *
                    static_cast<float>(std::sin(2.0 * K_PI * 3000.0 * static_cast<double>(i) / K_SAMPLE_RATE));
        }
        return b;
    }
} // namespace

TEST_P(Small_NullTestAlignment, AlignedDelayedCopyNullsCompletely)
{
    const std::vector<float> a = GetParam().signal();

    const double level = SushiDSP::measure::null_test(
        a, delayed_copy(a, GetParam().delay, GetParam().gain), GetParam().maximum_lag);

    EXPECT_LT(level, GetParam().level_db);
}

INSTANTIATE_TEST_SUITE_P(Copies, Small_NullTestAlignment, ::testing::ValuesIn(kCopyRows), copy_name);

TEST(Small_NullTest, UnalignedDelayedCopyLeavesAResidual)
{
    const std::vector<float> a = pseudo_random(4096);

    EXPECT_GT(SushiDSP::measure::null_test(a, delayed_copy(a, 3u, 1.0f), 0), -20.0);
}

TEST(Small_NullTest, ReportsARealDifference)
{
    const double level = SushiDSP::measure::null_test(tone(), tone_with_an_added_tone(), 64);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(level, expected_difference_db(), rounding_bound_db())));
}
