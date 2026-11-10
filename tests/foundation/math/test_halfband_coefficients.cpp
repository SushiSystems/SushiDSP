/**************************************************************************/
/* test_halfband_coefficients.cpp                                         */
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
 * @file test_halfband_coefficients.cpp
 * @brief Tests the three generated half-band designs: symmetry, zeros and unity DC gain.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/math/halfband_coefficients.hpp>

namespace
{
    /// One design under test, with the name its test carries.
    struct Row
    {
            const char* name;
            const SushiDSP::HalfbandDesign* design;
    };

    const Row kRows[] = {
        {"Stage1", &SushiDSP::K_HALFBAND_STAGE1},
        {"Stage2", &SushiDSP::K_HALFBAND_STAGE2},
        {"Stage3", &SushiDSP::K_HALFBAND_STAGE3},
    };

    std::string row_name(const ::testing::TestParamInfo<Row>& info)
    {
        return info.param.name;
    }

    /// @brief Returns how many taps differ from their mirror image about the centre.
    std::uint32_t asymmetric_tap_count(const SushiDSP::HalfbandDesign& design)
    {
        std::uint32_t count = 0;
        for (std::uint32_t i = 0; i < design.tap_count / 2; ++i)
        {
            count += design.taps[i] != design.taps[design.tap_count - 1 - i] ? 1u : 0u;
        }
        return count;
    }

    /// @brief Returns how many taps at an even distance from the centre, the centre excluded, are not zero.
    std::uint32_t nonzero_even_distance_tap_count(const SushiDSP::HalfbandDesign& design)
    {
        const std::uint32_t centre = (design.tap_count - 1) / 2;
        std::uint32_t count = 0;
        for (std::uint32_t offset = 2; offset <= centre; offset += 2)
        {
            count += design.taps[centre - offset] != 0.0f ? 1u : 0u;
            count += design.taps[centre + offset] != 0.0f ? 1u : 0u;
        }
        return count;
    }

    /// @brief Returns the sum of the taps and the sum of their magnitudes.
    struct TapSums
    {
            double sum = 0.0;
            double magnitude = 0.0;
    };

    TapSums tap_sums(const SushiDSP::HalfbandDesign& design)
    {
        TapSums sums;
        for (std::uint32_t i = 0; i < design.tap_count; ++i)
        {
            sums.sum += design.taps[i];
            sums.magnitude += std::abs(static_cast<double>(design.taps[i]));
        }
        return sums;
    }

    class Small_HalfbandDesign : public ::testing::TestWithParam<Row>
    {
    };
} // namespace

TEST_P(Small_HalfbandDesign, IsSymmetricAboutItsCentre)
{
    EXPECT_EQ(asymmetric_tap_count(*GetParam().design), 0u);
}

TEST_P(Small_HalfbandDesign, HasExactZeroTapsAtEveryEvenDistanceFromItsCentre)
{
    EXPECT_EQ(nonzero_even_distance_tap_count(*GetParam().design), 0u);
}

// The table holds the design rounded to float, so each tap is off by at most u of itself.
TEST_P(Small_HalfbandDesign, HasUnityDcGain)
{
    const TapSums sums = tap_sums(*GetParam().design);

    const double bound = TestSupport::unit_roundoff<float>() * sums.magnitude;
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(sums.sum, 1.0, bound)));
}

INSTANTIATE_TEST_SUITE_P(Designs, Small_HalfbandDesign, ::testing::ValuesIn(kRows), row_name);
