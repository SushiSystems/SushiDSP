/**************************************************************************/
/* test_spice_block_compare.cpp                                           */
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
 * @file test_spice_block_compare.cpp
 * @brief Tests the reference path of a block and the band-limit applied before a comparison.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/spice_reference/spice_block_compare.hpp>

namespace
{
    /// The sample rate and the corner of the band-limit under test, in hertz.
    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr double K_CORNER = 1000.0;

    /// The signal length, long enough for both sections' transients to die: the slowest pole radius is
    /// sqrt((1 - alpha) / (1 + alpha)) = 0.951 at the second section, and 0.951^19000 underflows.
    constexpr std::size_t K_LENGTH = 20000;

    /// The quality factors of the two sections of the 4th-order Butterworth low-pass, as the header sets them.
    constexpr double K_QUALITY_FIRST = 0.54119610;
    constexpr double K_QUALITY_SECOND = 1.30656296;

    /// @brief Returns the condition of the DC gain of one RBJ low-pass section, (1 + |a1| + |a2|) / (1 + a1 + a2),
    /// by which the roundings of its normalised coefficients are magnified in the unit DC gain.
    double dc_condition(double quality)
    {
        const double omega = 2.0 * 3.14159265358979323846 * K_CORNER / K_SAMPLE_RATE;
        const double alpha = std::sin(omega) / (2.0 * quality);
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * std::cos(omega) / a0;
        const double a2 = (1.0 - alpha) / a0;
        return (1.0 + std::abs(a1) + std::abs(a2)) / (1.0 + a1 + a2);
    }

    /// @brief Returns a signal alternating +1, -1: all of its energy sits at the Nyquist frequency.
    std::vector<float> nyquist_signal()
    {
        std::vector<float> signal(K_LENGTH);
        for (std::size_t i = 0; i < signal.size(); ++i)
        {
            signal[i] = (i % 2 == 0) ? 1.0f : -1.0f;
        }
        return signal;
    }

    /// @brief Returns the largest magnitude in the last count samples.
    float tail_peak(const std::vector<float>& signal, std::size_t count)
    {
        float peak = 0.0f;
        for (std::size_t i = signal.size() - count; i < signal.size(); ++i)
        {
            peak = std::max(peak, std::fabs(signal[i]));
        }
        return peak;
    }
} // namespace

TEST(Small_SpiceBlockCompare, SpicePathNamesTheBlockFolderAndItsKind)
{
    EXPECT_EQ(SushiDSP::test_circuits::spice_path("ts_tone", "ac"),
              std::string(SUSHIDSP_SPICE_DIR) + "/ts_tone/ts_tone_ac.csv");
}

// Each section's normalised coefficients round by u_d, which the DC gain magnifies by the section's condition.
TEST(Small_SpiceBlockCompare, BandLimitKeepsADcSignalAtItsLevel)
{
    std::vector<float> signal(K_LENGTH, 0.5f);

    SushiDSP::test_circuits::band_limit(signal, K_SAMPLE_RATE, K_CORNER);

    const double relative_bound = TestSupport::unit_roundoff<float>() +
                                  4.0 * TestSupport::unit_roundoff<double>() *
                                      (dc_condition(K_QUALITY_FIRST) + dc_condition(K_QUALITY_SECOND));
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(signal.back(), 0.5, 0.5 * relative_bound, 1.0)));
}

// Both sections have b0 - b1 + b2 exactly zero (b1 is exactly 2 b0), so the settled tail is exactly zero.
TEST(Small_SpiceBlockCompare, BandLimitRemovesNearNyquistContent)
{
    std::vector<float> signal = nyquist_signal();

    SushiDSP::test_circuits::band_limit(signal, K_SAMPLE_RATE, K_CORNER);

    EXPECT_EQ(tail_peak(signal, 1000), 0.0f);
}
