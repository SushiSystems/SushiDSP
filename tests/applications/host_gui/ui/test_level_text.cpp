/**************************************************************************/
/* test_level_text.cpp                                                    */
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
 * @file test_level_text.cpp
 * @brief Tests the decibel text and meter fill of LevelText.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

#include "support/tolerance.hpp"
#include "ui/level_text.hpp"

namespace
{
    /// @brief One peak whose decibel reading the test checks.
    struct PeakRow
    {
            const char* name;
            float peak;
    };

    /// @brief Returns the row's name, for the parameterised test names.
    std::string peak_row_name(const ::testing::TestParamInfo<PeakRow>& info)
    {
        return std::string(info.param.name);
    }

    /// @brief Rounded operations on the longest path of the reading: the logarithm and the
    /// product with twenty.
    constexpr std::size_t K_PEAK_TO_DECIBEL_OPERATIONS = 2;
} // namespace

class Small_LevelTextPeak : public ::testing::TestWithParam<PeakRow>
{
};

TEST_P(Small_LevelTextPeak, LevelTextReadsThePeakAsTwentyTimesItsCommonLogarithm)
{
    const double peak = GetParam().peak;
    const double reference = 20.0 * std::log10(peak);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(
        ui::peak_to_decibels(GetParam().peak), reference, K_PEAK_TO_DECIBEL_OPERATIONS)));
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_LevelTextPeak,
                         ::testing::Values(PeakRow{"Unity", 1.0f}, PeakRow{"Half", 0.5f},
                                           PeakRow{"Quarter", 0.25f}),
                         peak_row_name);

TEST(Small_LevelText, LevelTextZeroPeakReadsNegativeInfinity)
{
    EXPECT_TRUE(std::isinf(ui::peak_to_decibels(0.0f)));
    EXPECT_LT(ui::peak_to_decibels(0.0f), 0.0f);
}

TEST(Small_LevelText, LevelTextFillClampsAtBothEnds)
{
    EXPECT_FLOAT_EQ(ui::decibels_to_fill(0.0f, -60.0f), 1.0f);
    EXPECT_FLOAT_EQ(ui::decibels_to_fill(10.0f, -60.0f), 1.0f);
    EXPECT_FLOAT_EQ(ui::decibels_to_fill(-60.0f, -60.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::decibels_to_fill(-100.0f, -60.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::decibels_to_fill(-30.0f, -60.0f), 0.5f);
}

TEST(Small_LevelText, LevelTextDecibelLabelFormatsOneDecimalOrNegativeInfinity)
{
    EXPECT_EQ(ui::decibel_label(0.0f), "0.0 dB");
    EXPECT_EQ(ui::decibel_label(-12.34f), "-12.3 dB");
    EXPECT_EQ(ui::decibel_label(-std::numeric_limits<float>::infinity()), "-inf dB");
}
