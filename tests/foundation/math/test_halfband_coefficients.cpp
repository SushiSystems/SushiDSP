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
 * @brief Tests the three generated half-band designs: size, symmetry, zeros and unity DC gain.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/halfband_coefficients.hpp>

#include <cstddef>

namespace
{
    double sum_of(const SushiDSP::HalfbandDesign& design)
    {
        double sum = 0.0;
        for (std::uint32_t i = 0; i < design.tap_count; ++i)
        {
            sum += design.taps[i];
        }
        return sum;
    }
} // namespace

TEST(Small_HalfbandCoefficients, TapCountsMatchTheStageDesigns)
{
    EXPECT_EQ(SushiDSP::K_HALFBAND_STAGE1.tap_count, 115u);
    EXPECT_EQ(SushiDSP::K_HALFBAND_STAGE2.tap_count, 27u);
    EXPECT_EQ(SushiDSP::K_HALFBAND_STAGE3.tap_count, 15u);
}

TEST(Small_HalfbandCoefficients, EachDesignIsSymmetricAboutItsCentre)
{
    for (const SushiDSP::HalfbandDesign* design :
         {&SushiDSP::K_HALFBAND_STAGE1, &SushiDSP::K_HALFBAND_STAGE2, &SushiDSP::K_HALFBAND_STAGE3})
    {
        for (std::uint32_t i = 0; i < design->tap_count / 2; ++i)
        {
            EXPECT_NEAR(design->taps[i], design->taps[design->tap_count - 1 - i], 1e-9f);
        }
    }
}

TEST(Small_HalfbandCoefficients, EveryOtherTapAwayFromCentreIsZero)
{
    const SushiDSP::HalfbandDesign& design = SushiDSP::K_HALFBAND_STAGE1;
    const std::uint32_t centre = (design.tap_count - 1) / 2;
    for (std::uint32_t offset = 2; offset <= centre; offset += 2)
    {
        EXPECT_NEAR(design.taps[centre - offset], 0.0f, 1e-9f);
        EXPECT_NEAR(design.taps[centre + offset], 0.0f, 1e-9f);
    }
}

TEST(Small_HalfbandCoefficients, EachDesignHasUnityDcGain)
{
    EXPECT_NEAR(sum_of(SushiDSP::K_HALFBAND_STAGE1), 1.0, 1e-4);
    EXPECT_NEAR(sum_of(SushiDSP::K_HALFBAND_STAGE2), 1.0, 1e-4);
    EXPECT_NEAR(sum_of(SushiDSP::K_HALFBAND_STAGE3), 1.0, 1e-4);
}
